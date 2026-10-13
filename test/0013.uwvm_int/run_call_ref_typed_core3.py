#!/usr/bin/env python3
"""Core 3 typed function references through declarations and deep tail calls.

Run only inside the 64 GiB/20-CPU Linux cgroup. Wasmtime supplies an independent
reference. --uwvm adds the selected repository's real interpreter modes.
"""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess


CASES = {
    'typed-param': ('ok', '''(module
      (type $t (func (param i32) (result i32)))
      (func $f (type $t) local.get 0 i32.const 1 i32.add)
      (elem declare func $f)
      (func $invoke (param (ref $t)) (result i32)
        i32.const 41 local.get 0 call_ref $t)
      (func (export "_start")
        ref.func $f call $invoke i32.const 42 i32.ne if unreachable end))'''),
    'typed-result': ('ok', '''(module
      (type $t (func (param i32) (result i32)))
      (func $f (type $t) local.get 0 i32.const 1 i32.add)
      (elem declare func $f)
      (func $pick (result (ref $t)) ref.func $f)
      (func (export "_start")
        i32.const 41 call $pick call_ref $t
        i32.const 42 i32.ne if unreachable end))'''),
    'typed-block-nonnull-result': ('ok', '''(module
      (type $t (func))
      (func $f (type $t))
      (elem declare func $f)
      (func (export "_start")
        block (result (ref $t)) ref.func $f end
        call_ref $t))'''),
    'typed-block-nonnull-rejects-null': ('validation', '''(module
      (type $t (func))
      (func (export "_start")
        block (result (ref $t)) ref.null $t end
        drop))'''),
    'typed-param-wrong-direct-call': ('validation', '''(module
      (type $a (func))
      (type $b (func (param i32)))
      (func $f (type $b) local.get 0 drop)
      (elem declare func $f)
      (func $invoke (param (ref $a)))
      (func (export "_start") ref.func $f call $invoke))'''),
    'typed-result-wrong': ('validation', '''(module
      (type $a (func))
      (type $b (func (param i32)))
      (func $f (type $b) local.get 0 drop)
      (elem declare func $f)
      (func $pick (result (ref $a)) ref.func $f)
      (func (export "_start") call $pick drop))'''),
    'typed-local-nullable': ('ok', '''(module
      (type $t (func (param i32) (result i32)))
      (func $f (type $t) local.get 0 i32.const 1 i32.add)
      (elem declare func $f)
      (func (export "_start") (local $slot (ref null $t))
        ref.func $f local.set $slot
        i32.const 41 local.get $slot call_ref $t
        i32.const 42 i32.ne if unreachable end))'''),
    'typed-local-nonnull': ('ok', '''(module
      (type $t (func (param i32) (result i32)))
      (func $f (type $t) local.get 0 i32.const 1 i32.add)
      (elem declare func $f)
      (func (export "_start") (local $slot (ref $t))
        ref.func $f local.set $slot
        i32.const 41 local.get $slot call_ref $t
        i32.const 42 i32.ne if unreachable end))'''),
    'typed-local-unset': ('validation', '''(module
      (type $t (func))
      (func (export "_start") (local $slot (ref $t))
        local.get $slot call_ref $t))'''),
    'typed-local-wrong-set': ('validation', '''(module
      (type $t (func))
      (func (export "_start") (local $slot (ref $t))
        ref.null $t local.set $slot))'''),
    'typed-local-block-no-escape': ('validation', '''(module
      (type $t (func))
      (func $f (type $t))
      (elem declare func $f)
      (func (export "_start") (local $slot (ref $t))
        block ref.func $f local.set $slot end
        local.get $slot call_ref $t))'''),
    'typed-local-preinit-survives': ('ok', '''(module
      (type $t (func))
      (func $f (type $t))
      (elem declare func $f)
      (func (export "_start") (local $slot (ref $t))
        ref.func $f local.set $slot
        block ref.func $f local.set $slot end
        local.get $slot call_ref $t))'''),
    'typed-local-else-reset': ('validation', '''(module
      (type $t (func))
      (func $f (type $t))
      (elem declare func $f)
      (func (export "_start") (local $slot (ref $t))
        i32.const 1
        if ref.func $f local.set $slot
        else local.get $slot drop
        end))'''),
    'typed-local-tee': ('ok', '''(module
      (type $t (func))
      (func $f (type $t))
      (elem declare func $f)
      (func (export "_start") (local $slot (ref $t))
        ref.func $f local.tee $slot call_ref $t
        local.get $slot call_ref $t))'''),
    'typed-global': ('ok', '''(module
      (type $t (func (param i32) (result i32)))
      (func $f (type $t) local.get 0 i32.const 1 i32.add)
      (elem declare func $f)
      (global $slot (ref null $t) (ref.func $f))
      (func (export "_start")
        i32.const 41 global.get $slot call_ref $t
        i32.const 42 i32.ne if unreachable end))'''),
    'typed-global-nonnull': ('ok', '''(module
      (type $t (func (param i32) (result i32)))
      (func $f (type $t) local.get 0 i32.const 1 i32.add)
      (elem declare func $f)
      (global $slot (ref $t) (ref.func $f))
      (func (export "_start")
        i32.const 41 global.get $slot call_ref $t
        i32.const 42 i32.ne if unreachable end))'''),
    'typed-global-null': ('null reference', '''(module
      (type $t (func))
      (global $slot (ref null $t) (ref.null $t))
      (func (export "_start") global.get $slot call_ref $t))'''),
    'typed-global-null-unknown-type': ('validation', '''(module
      (type $t (func))
      (global $slot (ref null $t) (ref.null 1))
      (func (export "_start") global.get $slot call_ref $t))'''),
    'typed-br-on-null': ('ok', '''(module
      (type $t (func))
      (func $f (type $t))
      (elem declare func $f)
      (global $slot (ref null $t) (ref.func $f))
      (func (export "_start")
        block
          global.get $slot br_on_null 0
          call_ref $t
        end))'''),
    'typed-br-on-nonnull-narrow': ('ok', '''(module
      (type $t (func))
      (func $f (type $t))
      (elem declare func $f)
      (global $slot (ref null $t) (ref.func $f))
      (func (export "_start")
        block (result (ref $t))
          global.get $slot br_on_non_null 0
          unreachable
        end
        call_ref $t))'''),
    'typed-br-on-nonnull-wrong-heap': ('validation', '''(module
      (type $a (func (param i32)))
      (type $b (func (param i64)))
      (func $fa (type $a) (param i32))
      (func $fb (type $b) (param i64))
      (elem declare func $fa $fb)
      (func (export "_start")
        block (result (ref null $a))
          ref.func $fb br_on_non_null 0
          unreachable
        end
        drop))'''),
    'typed-br-on-null-prefix-wrong-heap': ('validation', '''(module
      (type $a (func (param i32)))
      (type $b (func (param i64)))
      (func $fa (type $a) (param i32))
      (func $fb (type $b) (param i64))
      (elem declare func $fa $fb)
      (func (export "_start")
        block (result (ref null $a))
          ref.func $fb ref.null func br_on_null 0
          drop unreachable
        end
        drop))'''),
    'typed-as-non-null': ('ok', '''(module
      (type $t (func))
      (func $f (type $t))
      (elem declare func $f)
      (global $slot (ref null $t) (ref.func $f))
      (func (export "_start")
        global.get $slot ref.as_non_null call_ref $t))'''),
    'typed-select-call-ref': ('ok', '''(module
      (type $t (func))
      (func $f (type $t))
      (elem declare func $f)
      (func (export "_start")
        ref.func $f ref.null $t i32.const 1
        select (result (ref null $t)) call_ref $t))'''),
    'typed-select-wrong-heap': ('validation', '''(module
      (type $a (func))
      (type $b (func (param i32)))
      (func $f (type $b) local.get 0 drop)
      (elem declare func $f)
      (func (export "_start")
        ref.func $f ref.null $a i32.const 1
        select (result (ref null $a)) drop))'''),
    'typed-table': ('ok', '''(module
      (type $t (func (param i32) (result i32)))
      (func $f (type $t) local.get 0 i32.const 1 i32.add)
      (elem declare func $f)
      (table $tab 1 (ref null $t))
      (func (export "_start")
        i32.const 0 ref.func $f table.set $tab
        i32.const 41 i32.const 0 table.get $tab call_ref $t
        i32.const 42 i32.ne if unreachable end))'''),
    'typed-table-null': ('null reference', '''(module
      (type $t (func))
      (table $tab 1 (ref null $t))
      (func (export "_start")
        i32.const 0 table.get $tab call_ref $t))'''),
    'typed-table-copy': ('ok', '''(module
      (type $t (func))
      (func $f (type $t))
      (elem declare func $f)
      (table $src 1 (ref null $t))
      (table $dst 1 (ref null $t))
      (func (export "_start")
        i32.const 0 ref.func $f table.set $src
        i32.const 0 i32.const 0 i32.const 1 table.copy $dst $src
        i32.const 0 table.get $dst call_ref $t))'''),
    'typed-table-copy-wrong-heap': ('validation', '''(module
      (type $a (func))
      (type $b (func (param i32)))
      (table $src 1 (ref null $a))
      (table $dst 1 (ref null $b))
      (func (export "_start")
        i32.const 0 i32.const 0 i32.const 1 table.copy $dst $src))'''),
    'typed-table-grow': ('ok', '''(module
      (type $t (func))
      (func $f (type $t))
      (elem declare func $f)
      (table $tab 1 (ref null $t))
      (func (export "_start")
        ref.func $f i32.const 1 table.grow $tab drop
        i32.const 1 table.get $tab call_ref $t))'''),
    'typed-table-grow-wrong-heap': ('validation', '''(module
      (type $a (func))
      (type $b (func (param i32)))
      (table $tab 1 (ref null $b))
      (func (export "_start")
        ref.null $a i32.const 1 table.grow $tab drop))'''),
    'typed-table-fill': ('ok', '''(module
      (type $t (func))
      (func $f (type $t))
      (elem declare func $f)
      (table $tab 1 (ref null $t))
      (func (export "_start")
        i32.const 0 ref.func $f i32.const 1 table.fill $tab
        i32.const 0 table.get $tab call_ref $t))'''),
    'typed-table-fill-wrong-heap': ('validation', '''(module
      (type $a (func))
      (type $b (func (param i32)))
      (table $tab 1 (ref null $b))
      (func (export "_start")
        i32.const 0 ref.null $a i32.const 1 table.fill $tab))'''),
    'typed-local-supertype': ('validation', '''(module
      (type $t (func))
      (func (export "_start") (local $slot funcref)
        local.get $slot call_ref $t))'''),
    'typed-block-result': ('ok', '''(module
      (type $t (func (param i32) (result i32)))
      (func $f (type $t) local.get 0 i32.const 1 i32.add)
      (elem declare func $f)
      (func (export "_start")
        i32.const 41
        block (result (ref null $t))
          ref.func $f
        end
        call_ref $t
        i32.const 42 i32.ne if unreachable end))'''),
    'typed-block-multivalue': ('ok', '''(module
      (type $t (func (param i32) (result i32)))
      (type $block (func (result i32 (ref null $t))))
      (func $f (type $t) local.get 0 i32.const 1 i32.add)
      (elem declare func $f)
      (func (export "_start")
        block (type $block)
          i32.const 41 ref.func $f
        end
        call_ref $t
        i32.const 42 i32.ne if unreachable end))'''),
    'typed-block-parameter': ('ok', '''(module
      (type $t (func (param i32) (result i32)))
      (type $block (func (param (ref null $t)) (result (ref null $t))))
      (func $f (type $t) local.get 0 i32.const 1 i32.add)
      (elem declare func $f)
      (func (export "_start")
        i32.const 41 ref.func $f
        block (type $block)
          nop
        end
        call_ref $t
        i32.const 42 i32.ne if unreachable end))'''),
    'typed-block-br': ('ok', '''(module
      (type $t (func (param i32) (result i32)))
      (func $f (type $t) local.get 0 i32.const 1 i32.add)
      (elem declare func $f)
      (func (export "_start")
        i32.const 41
        block (result (ref null $t))
          ref.func $f br 0
        end
        call_ref $t
        i32.const 42 i32.ne if unreachable end))'''),
    'typed-block-br-if': ('ok', '''(module
      (type $t (func (param i32) (result i32)))
      (func $f (type $t) local.get 0 i32.const 1 i32.add)
      (elem declare func $f)
      (func (export "_start")
        i32.const 41
        block (result (ref null $t))
          ref.func $f i32.const 1 br_if 0
          drop ref.func $f
        end
        call_ref $t
        i32.const 42 i32.ne if unreachable end))'''),
    'typed-block-br-table': ('ok', '''(module
      (type $t (func (param i32) (result i32)))
      (func $f (type $t) local.get 0 i32.const 1 i32.add)
      (elem declare func $f)
      (func (export "_start")
        i32.const 41
        block (result (ref null $t))
          ref.func $f i32.const 0 br_table 0 0
        end
        call_ref $t
        i32.const 42 i32.ne if unreachable end))'''),
    'typed-br-table-subtype-labels': ('ok', '''(module
      (type $t (func))
      (func $f (type $t))
      (elem declare func $f)
      (func (export "_start")
        block (result funcref)
          block (result (ref null $t))
            ref.func $f i32.const 0 br_table 0 1
          end
        end
        drop))'''),
    'typed-block-if-else': ('ok', '''(module
      (type $t (func (param i32) (result i32)))
      (func $f (type $t) local.get 0 i32.const 1 i32.add)
      (elem declare func $f)
      (func (export "_start")
        i32.const 41 i32.const 1
        if (result (ref null $t))
          ref.func $f
        else
          ref.func $f
        end
        call_ref $t
        i32.const 42 i32.ne if unreachable end))'''),
    # Wasmtime 48 validates this Core 3 subtype but Cranelift panics while
    # compiling it; wasm-tools' independent validator is the oracle here.
    'typed-if-implicit-subtype': ('ok-wasm-tools-validation', '''(module
      (type $t (func (param i32) (result i32)))
      (type $if-type (func (param (ref $t)) (result (ref null $t))))
      (func $f (type $t) local.get 0 i32.const 1 i32.add)
      (elem declare func $f)
      (func $choose (param i32) (param (ref $t)) (result (ref null $t))
        local.get 1 local.get 0
        if (type $if-type)
          ref.as_non_null
        end)
      (func (export "_start")
        i32.const 41 i32.const 0 ref.func $f call $choose
        call_ref $t
        i32.const 42 i32.ne if unreachable end))'''),
    'typed-block-loop-parameter': ('ok', '''(module
      (type $t (func (param i32) (result i32)))
      (type $loop-type (func (param (ref null $t)) (result (ref null $t))))
      (func $f (type $t) local.get 0 i32.const 1 i32.add)
      (elem declare func $f)
      (func (export "_start")
        i32.const 41 ref.func $f
        loop (type $loop-type)
          nop
        end
        call_ref $t
        i32.const 42 i32.ne if unreachable end))'''),
    'typed-block-result-wrong-heap': ('validation', '''(module
      (type $a (func))
      (type $b (func (param i32)))
      (func $f (type $b) local.get 0 drop)
      (elem declare func $f)
      (func (export "_start")
        block (result (ref null $a))
          ref.func $f
        end
        drop))'''),
    'typed-call-ref-wrong-parameter-heap': ('validation', '''(module
      (type $a (func))
      (type $b (func (param i32)))
      (type $callee (func (param (ref $a))))
      (func $fa (type $a))
      (func $fb (type $b) local.get 0 drop)
      (func $invoke (type $callee) local.get 0 drop)
      (elem declare func $fa $fb $invoke)
      (func (export "_start") ref.func $fb ref.func $invoke call_ref $callee))'''),
    'typed-call-indirect-wrong-parameter-heap': ('validation', '''(module
      (type $a (func))
      (type $b (func (param i32)))
      (type $callee (func (param (ref $a))))
      (func $fa (type $a))
      (func $fb (type $b) local.get 0 drop)
      (func $invoke (type $callee) local.get 0 drop)
      (table 1 funcref)
      (elem (i32.const 0) $invoke)
      (elem declare func $fa $fb)
      (func (export "_start") ref.func $fb i32.const 0 call_indirect (type $callee)))'''),
    'typed-call-indirect-rich-result': ('ok', '''(module
      (type $t (func (param i32) (result i32)))
      (type $getter (func (result (ref $t))))
      (func $f (type $t) local.get 0 i32.const 1 i32.add)
      (func $get (type $getter) ref.func $f)
      (elem declare func $f)
      (table 1 funcref)
      (elem (i32.const 0) $get)
      (func (export "_start")
        i32.const 41 i32.const 0 call_indirect (type $getter)
        call_ref $t i32.const 42 i32.ne if unreachable end))'''),
    'typed-return-call-wrong-parameter-heap': ('validation', '''(module
      (type $a (func))
      (type $b (func (param i32)))
      (type $callee (func (param (ref $a))))
      (func $fa (type $a))
      (func $fb (type $b) local.get 0 drop)
      (func $invoke (type $callee) local.get 0 drop)
      (elem declare func $fa $fb)
      (func $caller ref.func $fb return_call $invoke)
      (func (export "_start") call $caller))'''),
    'typed-return-call-wrong-result-heap': ('validation', '''(module
      (type $a (func))
      (type $b (func (param i32)))
      (type $callee (func (result (ref $b))))
      (func $fb (type $b) local.get 0 drop)
      (func $give (type $callee) ref.func $fb)
      (elem declare func $fb)
      (func $caller (result (ref $a)) return_call $give)
      (func (export "_start") call $caller drop))'''),
    'typed-return-call-indirect-wrong-parameter-heap': ('validation', '''(module
      (type $a (func))
      (type $b (func (param i32)))
      (type $callee (func (param (ref $a))))
      (func $fa (type $a))
      (func $fb (type $b) local.get 0 drop)
      (func $invoke (type $callee) local.get 0 drop)
      (elem declare func $fa $fb)
      (table 1 funcref)
      (elem (i32.const 0) $invoke)
      (func $caller ref.func $fb i32.const 0 return_call_indirect (type $callee))
      (func (export "_start") call $caller))'''),
    'typed-return-call-indirect-wrong-result-heap': ('validation', '''(module
      (type $a (func))
      (type $b (func (param i32)))
      (type $callee (func (result (ref $b))))
      (func $fb (type $b) local.get 0 drop)
      (func $give (type $callee) ref.func $fb)
      (elem declare func $fb)
      (table 1 funcref)
      (elem (i32.const 0) $give)
      (func $caller (result (ref $a)) i32.const 0 return_call_indirect (type $callee))
      (func (export "_start") call $caller drop))'''),
    'typed-return-call-ref-wrong-result-heap': ('validation', '''(module
      (type $a (func))
      (type $b (func (param i32)))
      (type $callee (func (result (ref $b))))
      (func $fb (type $b) local.get 0 drop)
      (func $give (type $callee) ref.func $fb)
      (elem declare func $fb $give)
      (func $caller (result (ref $a)) ref.func $give return_call_ref $callee)
      (func (export "_start") call $caller drop))'''),
    'typed-return-wrong-heap': ('validation', '''(module
      (type $a (func))
      (type $b (func (param i32)))
      (func $fb (type $b) local.get 0 drop)
      (elem declare func $fb)
      (func $give (result (ref $a)) ref.func $fb return)
      (func (export "_start") call $give drop))'''),
    'typed-br-if-wrong-heap': ('validation', '''(module
      (type $a (func))
      (type $b (func (param i32)))
      (func $fb (type $b) local.get 0 drop)
      (elem declare func $fb)
      (func (export "_start")
        block (result (ref null $a))
          ref.func $fb i32.const 1 br_if 0
          drop ref.null $a
        end drop))'''),
    'typed-table-set-wrong-heap': ('validation', '''(module
      (type $a (func))
      (type $b (func (param i32)))
      (func $fb (type $b) local.get 0 drop)
      (elem declare func $fb)
      (table $tab 1 (ref null $a))
      (func (export "_start") i32.const 0 ref.func $fb table.set $tab))'''),
    'typed-global-set-wrong-heap': ('validation', '''(module
      (type $a (func))
      (type $b (func (param i32)))
      (func $fb (type $b) local.get 0 drop)
      (elem declare func $fb)
      (global $slot (mut (ref null $a)) (ref.null $a))
      (func (export "_start") ref.func $fb global.set $slot))'''),
    'typed-select-nonnull-rejects-null': ('validation', '''(module
      (type $a (func))
      (func $fa (type $a))
      (elem declare func $fa)
      (func (export "_start") ref.func $fa ref.null $a i32.const 1
        select (result (ref $a)) drop))'''),
    'typed-table-init-exact': ('ok', '''(module
      (type $t (func))
      (func $f (type $t))
      (elem $e (ref null $t) (ref.func $f))
      (table $tab 1 (ref null $t))
      (func (export "_start")
        i32.const 0 i32.const 0 i32.const 1 table.init $tab $e
        i32.const 0 table.get $tab call_ref $t))'''),
    'typed-table-init-subtype': ('ok', '''(module
      (type $t (func))
      (func $f (type $t))
      (elem $e (ref $t) (ref.func $f))
      (table $tab 1 funcref)
      (func (export "_start")
        i32.const 0 i32.const 0 i32.const 1 table.init $tab $e
        i32.const 0 table.get $tab ref.is_null if unreachable end))'''),
    'typed-table-init-structurally-equivalent': ('ok', '''(module
      (type $a (func (param i32) (result i32)))
      (type $b (func (param i32) (result i32)))
      (func $f (type $a) local.get 0 i32.const 1 i32.add)
      (elem $e (ref null $a) (ref.func $f))
      (table $tab 1 (ref null $b))
      (func (export "_start")
        i32.const 0 i32.const 0 i32.const 1 table.init $tab $e
        i32.const 41 i32.const 0 table.get $tab call_ref $b
        i32.const 42 i32.ne if unreachable end))'''),
    'typed-table-init-nested-heap-mismatch': ('validation', '''(module
      (type $leaf-a (func))
      (type $leaf-b (func (param i32)))
      (type $nested-a (func (param (ref null $leaf-a))))
      (type $nested-b (func (param (ref null $leaf-b))))
      (func $f (type $nested-a) local.get 0 drop)
      (elem $e (ref null $nested-a) (ref.func $f))
      (table $tab 1 (ref null $nested-b))
      (func (export "_start") i32.const 0 i32.const 0 i32.const 1 table.init $tab $e))'''),
    'typed-table-init-wrong-heap': ('validation', '''(module
      (type $a (func))
      (type $b (func (param i32)))
      (func $fb (type $b) local.get 0 drop)
      (elem $e (ref null $b) (ref.func $fb))
      (table $tab 1 (ref null $a))
      (func (export "_start") i32.const 0 i32.const 0 i32.const 1 table.init $tab $e))'''),
    'typed-element-item-wrong-heap': ('validation', '''(module
      (type $a (func))
      (type $b (func (param i32)))
      (func $fb (type $b) local.get 0 drop)
      (elem $e (ref null $a) (ref.func $fb))
      (func (export "_start")))'''),
    'typed-active-element-exact': ('ok', '''(module
      (type $t (func))
      (func $f (type $t))
      (table $tab 1 (ref null $t))
      (elem (table $tab) (i32.const 0) (ref null $t) (ref.func $f))
      (func (export "_start") i32.const 0 table.get $tab call_ref $t))'''),
    'typed-active-element-wrong-heap': ('validation', '''(module
      (type $a (func))
      (type $b (func (param i32)))
      (func $fb (type $b) local.get 0 drop)
      (table $tab 1 (ref null $a))
      (elem (table $tab) (i32.const 0) (ref null $b) (ref.func $fb))
      (func (export "_start")))'''),
    'typed-element-null-wrong-heap': ('validation', '''(module
      (type $a (func))
      (type $b (func (param i32)))
      (elem $e (ref null $a) (ref.null $b))
      (func (export "_start")))'''),
    'typed-element-nonnull-rejects-null': ('validation', '''(module
      (type $t (func))
      (elem $e (ref $t) (ref.null $t))
      (func (export "_start")))'''),
    'tail-typed-local-250k': ('ok', '''(module
      (type $t (func (param i32) (result i32)))
      (func $loop (type $t) (local $next (ref $t))
        local.get 0 i32.eqz if i32.const 17 return end
        ref.func $loop local.set $next
        local.get 0 i32.const 1 i32.sub
        local.get $next return_call_ref $t)
      (elem declare func $loop)
      (func (export "_start")
        i32.const 250000 call $loop
        i32.const 17 i32.ne if unreachable end))'''),
    'rec-singleton-direct': ('ok', '''(module
  (rec (type $t (func (result i32))))
  (func $answer (type $t)
    i32.const 42)
  (func (export "_start")
    call $answer
    i32.const 42
    i32.ne
    if unreachable end))'''),
    'rec-singleton-call-ref': ('ok', '''(module
  (rec (type $t (func (param i32) (result i32))))
  (func $increment (type $t)
    local.get 0
    i32.const 1
    i32.add)
  (elem declare func $increment)
  (func (export "_start")
    i32.const 41
    ref.func $increment
    call_ref $t
    i32.const 42
    i32.ne
    if unreachable end))'''),
    'rec-singleton-self-call-ref': ('ok', '''(module
  (rec (type $t (func (param (ref null $t)) (result (ref null $t)))))
  (func $identity (type $t)
    local.get 0)
  (elem declare func $identity)
  (func (export "_start")
    ref.null $t
    ref.func $identity
    call_ref $t
    drop))'''),
    'rec-singleton-wrong-heap': ('validation', '''(module
  (rec (type $t (func (param (ref null $t)) (result (ref null $t)))))
  (func $identity (type $t)
    local.get 0)
  (elem declare func $identity)
  (func (export "_start")
    ref.null func
    ref.func $identity
    call_ref $t
    drop))'''),
    'rec-multi-simple': ('ok', '''(module
  (rec (type $a (func (result i32)))
       (type $b (func (param i32) (result i32))))
  (func $plus1 (type $b)
    local.get 0 i32.const 1 i32.add)
  (func (export "_start")
    i32.const 41 call $plus1
    i32.const 42 i32.ne if unreachable end))'''),
    'rec-multi-forward': ('ok', '''(module
  (rec (type $a (func (param (ref null $b)) (result (ref null $b))))
       (type $b (func)))
  (func $f (type $b))
  (elem declare func $f)
  (func $identity (type $a) local.get 0)
  (func (export "_start")
    ref.func $f call $identity
    ref.is_null if unreachable end))'''),
    'rec-multi-call-ref': ('ok', '''(module
  (rec (type $a (func (param (ref null $b)) (result (ref null $b))))
       (type $b (func)))
  (func $f (type $b))
  (func $identity (type $a) local.get 0)
  (elem declare func $f $identity)
  (func (export "_start")
    ref.func $f ref.func $identity
    call_ref $a
    ref.is_null if unreachable end))'''),
    'rec-multi-wrong-heap': ('validation', '''(module
  (rec (type $a (func (param (ref null $b)) (result (ref null $b))))
       (type $b (func))
       (type $c (func (param i32))))
  (func $f (type $c) local.get 0 drop)
  (elem declare func $f)
  (func $identity (type $a) local.get 0)
  (func (export "_start")
    ref.func $f call $identity drop))'''),
    'rec-call-ref-aggregate-result-mismatch': ('validation', '''(module
  (rec (type $sa (struct (field i32)))
       (type $sb (struct (field i64)))
       (type $fa (func (result (ref null $sa))))
       (type $fb (func (result (ref null $sb)))))
  (func $f (type $fa) ref.null $sa)
  (elem declare func $f)
  (func (export "_start") ref.func $f call_ref $fb drop))'''),
}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--uwvm', type=Path)
    parser.add_argument('--wasmtime', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--only-case', action='append')
    parser.add_argument('--only-mode', action='append')
    parser.add_argument('--all-combine-delay', action='store_true')
    parser.add_argument('--guard', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    subprocess.run(['bash', str(args.guard or root / 'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.output.mkdir(parents=True, exist_ok=False)
    cases = {name: value for name, value in CASES.items() if not args.only_case or name in args.only_case}
    if args.only_case and len(cases) != len(set(args.only_case)):
        parser.error('unknown --only-case')
    modes = ['full'] if args.ros else ['full', 'lazy', 'lazy+verification', 'tiered']
    if args.only_mode:
        if not set(args.only_mode) <= set(modes):
            parser.error('unknown --only-mode')
        modes = args.only_mode
    combinations = [('default', [])]
    if args.all_combine_delay:
        combinations = [(level + ('-no-delay' if no_delay else '-delay'),
                         ['-Rint-op-conbine-level', level] + (['-Rint-no-delay-local'] if no_delay else []))
                        for level in ('disable', 'soft', 'heavy', 'extra') for no_delay in (False, True)]
    rows = []
    for name, (outcome, source) in cases.items():
        gc_flags = ['-WFE-gc'] if name.startswith('rec-') else []
        function_reference_flags = [] if name == 'rec-singleton-direct' else ['-WFE-function-references']
        wat = args.output / (name + '.wat')
        wasm = wat.with_suffix('.wasm')
        wat.write_text(source + '\n')
        subprocess.run([str(args.wasm_tools), 'parse', str(wat), '-o', str(wasm)], check=True)
        commands = [('wasm-tools-validation', [str(args.wasm_tools), 'validate', '--features', 'all', str(wasm)])] if \
            outcome == 'ok-wasm-tools-validation' else [
                ('wasmtime', [str(args.wasmtime), '-C', 'cache=n', '-W', 'function-references=y',
                              '-W', 'tail-call=y', str(wasm)])]
        if args.uwvm:
            uwvm = str(args.uwvm.resolve())
            for mode in modes:
                base = ['-Rint'] if args.ros else ['--runtime-tiered', '-Rct', '0'] if mode == 'tiered' else ['-Rcc', 'int', '-Rcm', mode]
                for combination, flags in combinations:
                    label = mode if combination == 'default' else mode + '-' + combination
                    commands.append((label, [uwvm, *base, *flags, *function_reference_flags, *gc_flags,
                                             '-WFE-tail-call', '--run', str(wasm)]))
            commands.append(('validator', [uwvm, '-m', 'validation', *function_reference_flags,
                                           *gc_flags, '-WFE-tail-call', '--run', str(wasm)]))
            if name in ('typed-table-init-exact', 'typed-table-init-subtype',
                        'typed-table-init-structurally-equivalent', 'typed-active-element-exact',
                        'typed-global-null') or name.startswith('rec-'):
                commands.append(('feature-off', [uwvm, '-m', 'validation',
                    *(['-WFE-function-references'] if name.startswith('rec-') and name != 'rec-singleton-direct' else []),
                    '--run', str(wasm)]))
            if name == 'typed-global-null':
                full_int = ['-Rint'] if args.ros else ['-Rcc', 'int', '-Rcm', 'full']
                commands.append(('feature-off-runtime', [uwvm, *full_int, '-WFD-gc',
                    '-WFE-tail-call', '--run', str(wasm)]))
                commands.append(('gc-off-runtime', [uwvm, *full_int,
                    '-WFE-function-references', '-WFD-gc', '-WFE-tail-call', '--run', str(wasm)]))
        for mode, command in commands:
            result = subprocess.run(command, capture_output=True, timeout=120)
            log = (result.stdout + result.stderr).decode(errors='replace')
            (args.output / f'{name}-{mode}.log').write_text(log)
            expected_success = not mode.startswith('feature-off') and (outcome in ('ok', 'ok-wasm-tools-validation') or
                (mode == 'validator' and outcome == 'null reference'))
            passed = (result.returncode == 0) == expected_success
            if mode.startswith('feature-off'):
                expected_feature = 'gc' if name.startswith('rec-') else 'function-references'
                passed = passed and expected_feature in log.lower()
            if outcome == 'null reference' and mode != 'validator' and not mode.startswith('feature-off'):
                passed = passed and 'null reference' in log.lower()
            if outcome == 'validation' and not mode.startswith('feature-off'):
                diagnostic = log.lower()
                rich_element_mismatch = ('parsing error' in diagnostic and
                    'element segment reference type' in diagnostic and 'does not match' in diagnostic)
                passed = passed and (rich_element_mismatch or
                    any(text in diagnostic for text in ('validation error', 'type mismatch', 'invalid input',
                        'illegal type index', 'type index out of bounds')))
            rows.append({'case': name, 'mode': mode, 'outcome': outcome, 'passed': passed,
                         'exit': result.returncode, 'command': command})
            (args.output / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
            if not passed:
                raise RuntimeError(f'{name} {mode}: exit={result.returncode}\n{log}')
    summary = {'passed': True, 'checks': len(rows), 'product': 'uwvm2-ros' if args.ros else 'uwvm2',
               'binary_sha256': hashlib.sha256(args.uwvm.read_bytes()).hexdigest() if args.uwvm else None,
               'runner_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    (args.output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(f'PASS typed Core 3 call_ref/return_call_ref: {len(rows)} checks', flush=True)


if __name__ == '__main__':
    main()
