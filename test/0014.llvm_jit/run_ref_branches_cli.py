#!/usr/bin/env python3
"""Exercise Core 3 null-reference branches in each actual runtime mode.

The integrated declaration carriers remain funcref/externref; rich heap declarations
are a separate unfinished frontend task. Cases include loop parameters, tuple labels,
polymorphic reification, feature isolation and signed-cache policy.
"""
import run_ref_as_non_null_cli as runner
runner.CASES={
 'identity':('success','''(module (func $f) (elem declare func $f)
  (func $id (param funcref) (result funcref)
   local.get 0 br_on_non_null 0 ref.null func)
  (func (export "_start") ref.func $f call $id ref.is_null if unreachable end
   ref.null func call $id ref.is_null i32.eqz if unreachable end))'''),
 'null-both-edges':('success','''(module (func $f) (elem declare func $f)
  (func $test (param funcref) (result i32)
   i32.const 11 local.get 0 br_on_null 0 drop drop i32.const 22)
  (func (export "_start") ref.null func call $test i32.const 11 i32.ne if unreachable end
   ref.func $f call $test i32.const 22 i32.ne if unreachable end))'''),
 'extern-null':('success','''(module
  (func $null (param externref) (result i32) i32.const 7 local.get 0 br_on_null 0 drop)
  (func $non (param externref) (result externref) local.get 0 br_on_non_null 0 ref.null extern)
  (func (export "_start") ref.null extern call $null i32.const 7 i32.ne if unreachable end
   ref.null extern call $non ref.is_null i32.eqz if unreachable end))'''),
 'repair-null':('success','''(module (func (export "_start")
  block (result i32) i64.const 99 f32.const 2 i32.const 31 ref.null func br_on_null 0
   drop drop drop drop i32.const 32 end i32.const 31 i32.ne if unreachable end))'''),
 'repair-non-null':('success','''(module (func $f) (elem declare func $f) (func (export "_start")
  block (result funcref) f64.const 99 i32.const 19 ref.func $f br_on_non_null 0
   drop drop ref.null func end ref.is_null if unreachable end))'''),
 'tuple-non-null':('success','''(module (func $f) (elem declare func $f) (func (export "_start")
  block (result i32 i64 funcref) f64.const 99 i32.const 7 i64.const 9 ref.func $f br_on_non_null 0
   drop drop drop i32.const 0 i64.const 0 ref.null func end
  ref.is_null if unreachable end i64.const 9 i64.ne if unreachable end i32.const 7 i32.ne if unreachable end))'''),
 'tuple-null':('success','''(module (func (export "_start")
  block (result i32 i64) f64.const 99 i32.const 7 i64.const 9 ref.null extern br_on_null 0
   drop drop drop drop i32.const 0 i64.const 0 end
  i64.const 9 i64.ne if unreachable end i32.const 7 i32.ne if unreachable end))'''),
 'hot-loop-null':('success','''(module (func $f) (elem declare func $f)
  (func $loop (result i32) (local i32)
   i32.const 20000 loop (param i32) (result i32)
    i32.const 1 i32.sub local.tee 0 local.get 0 i32.eqz
    if (result funcref) ref.func $f else ref.null func end br_on_null 0 drop end)
  (func (export "_start") call $loop if unreachable end))'''),
 'hot-loop-non-null':('success','''(module (func $f) (elem declare func $f)
  (func $loop (result i32) (local i32)
   i32.const 20000 local.set 0 ref.func $f loop (param funcref) (result i32)
    drop local.get 0 i32.const 1 i32.sub local.tee 0
    if (result funcref) ref.func $f else ref.null func end br_on_non_null 0 i32.const 7 end)
  (func (export "_start") call $loop i32.const 7 i32.ne if unreachable end))'''),
 'nested':('success','''(module (func (export "_start") block (result i32)
  block i32.const 42 ref.null extern br_on_null 1 drop drop end i32.const 0 end
  i32.const 42 i32.ne if unreachable end))'''),
 'bottom-reification':('success','''(module
  (func (result externref) unreachable br_on_null 0 drop)
  (func (result funcref) unreachable br_on_non_null 0)
  (func (export "_start")))'''),
 'bad-empty-label':('validation','(module (func (export "_start") unreachable br_on_non_null 0))'),
 'bad-number-label':('validation','(module (func (export "_start") block (result i32) unreachable br_on_non_null 0 end drop))'),
 'bad-number':('validation','(module (func (export "_start") i32.const 0 br_on_null 0 drop))'),
 'bad-underflow':('validation','(module (func (export "_start") br_on_null 0 drop))'),
 'bad-label-index':('validation','(module (func (export "_start") ref.null func br_on_null 1 drop))'),
 'bad-reference':('validation','(module (func (export "_start") block (result funcref) ref.null extern br_on_non_null 0 ref.null func end drop))'),
 'bad-prefix':('validation','(module (func (export "_start") block (result i32) i64.const 0 ref.null func br_on_null 0 drop drop i32.const 0 end drop))'),
 'bad-bottom-number':('validation','(module (func (export "_start") unreachable br_on_null 0 i32.eqz drop))'),
 'bad-fallthrough':('validation','(module (func (export "_start") ref.null func br_on_null 0))'),
}
if __name__=='__main__':runner.main()
