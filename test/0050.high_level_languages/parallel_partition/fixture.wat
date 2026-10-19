(module
  (type $op (func (param i32) (result i32)))
  (import "wasi_snapshot_preview1" "fd_write" (func $fd_write (param i32 i32 i32 i32) (result i32)))
  (memory (export "memory") 1)
  (table (export "table") 4 funcref)
  (global $g (export "g") (mut i32) (i32.const 5))
  (tag $failure (param i32))
  (data (i32.const 32) "\07\00\00\00")
  (data (i32.const 256) "parallel-partition PASS result=42 marks=248\0a")
  (func $add1 (export "add1") (type $op) local.get 0 i32.const 1 i32.add)
  (func $add2 (export "add2") (type $op) local.get 0 i32.const 2 i32.add)
  (elem (i32.const 0) func $add1 $add2)
  (func $prepare (export "prepare")
    global.get $g i32.const 2 i32.add global.set $g
    i32.const 64 i32.const 11 i32.store
    i32.const 0 ref.func $add2 table.set 0)
  (func $direct (export "direct") (param i32) (result i32)
    local.get 0 global.get $g i32.add)
  (func $indirect (export "indirect") (param i32) (result i32)
    local.get 0 i32.const 0 call_indirect (type $op))
  (func $callref (export "callref") (param i32) (result i32)
    local.get 0 ref.func $add1 call_ref $op)
  (func $thrower (export "thrower") (param i32)
    local.get 0 throw $failure)
  (func $catcher (export "catcher") (param i32) (result i32)
    block $caught (result i32)
      try_table (catch $failure $caught)
        local.get 0 call $thrower
      end
      unreachable
    end)
  (func $mark0 (export "mark0") i32.const 128 global.get $g i32.const 1 i32.add i32.store)
  (func $mark1 (export "mark1") i32.const 132 global.get $g i32.const 2 i32.add i32.store)
  (func $mark2 (export "mark2") i32.const 136 global.get $g i32.const 3 i32.add i32.store)
  (func $mark3 (export "mark3") i32.const 140 global.get $g i32.const 4 i32.add i32.store)
  (func $mark4 (export "mark4") i32.const 144 global.get $g i32.const 5 i32.add i32.store)
  (func $mark5 (export "mark5") i32.const 148 global.get $g i32.const 6 i32.add i32.store)
  (func $mark6 (export "mark6") i32.const 152 global.get $g i32.const 7 i32.add i32.store)
  (func $mark7 (export "mark7") i32.const 156 global.get $g i32.const 8 i32.add i32.store)
  (func $mark8 (export "mark8") i32.const 160 global.get $g i32.const 9 i32.add i32.store)
  (func $mark9 (export "mark9") i32.const 164 global.get $g i32.const 10 i32.add i32.store)
  (func $mark10 (export "mark10") i32.const 168 global.get $g i32.const 11 i32.add i32.store)
  (func $mark11 (export "mark11") i32.const 172 global.get $g i32.const 12 i32.add i32.store)
  (func $mark12 (export "mark12") i32.const 176 global.get $g i32.const 13 i32.add i32.store)
  (func $mark13 (export "mark13") i32.const 180 global.get $g i32.const 14 i32.add i32.store)
  (func $mark14 (export "mark14") i32.const 184 global.get $g i32.const 15 i32.add i32.store)
  (func $mark15 (export "mark15") i32.const 188 global.get $g i32.const 16 i32.add i32.store)
  (func $verify (export "verify") (result i32)
    call $prepare
    call $mark0
    call $mark1
    call $mark2
    call $mark3
    call $mark4
    call $mark5
    call $mark6
    call $mark7
    call $mark8
    call $mark9
    call $mark10
    call $mark11
    call $mark12
    call $mark13
    call $mark14
    call $mark15
    i32.const 0
    i32.const 128 i32.load i32.add
    i32.const 132 i32.load i32.add
    i32.const 136 i32.load i32.add
    i32.const 140 i32.load i32.add
    i32.const 144 i32.load i32.add
    i32.const 148 i32.load i32.add
    i32.const 152 i32.load i32.add
    i32.const 156 i32.load i32.add
    i32.const 160 i32.load i32.add
    i32.const 164 i32.load i32.add
    i32.const 168 i32.load i32.add
    i32.const 172 i32.load i32.add
    i32.const 176 i32.load i32.add
    i32.const 180 i32.load i32.add
    i32.const 184 i32.load i32.add
    i32.const 188 i32.load i32.add
    i32.const 248 i32.ne if unreachable end
    global.get $g i32.const 7 i32.ne if unreachable end
    i32.const 5 call $direct
    i32.const 32 i32.load call $indirect i32.add
    i32.const 64 i32.load call $callref i32.add
    i32.const 9 call $catcher i32.add)
  (func $start (export "_start")
    call $verify i32.const 42 i32.ne if unreachable end
    i32.const 16 i32.const 256 i32.store
    i32.const 20 i32.const 44 i32.store
    i32.const 1 i32.const 16 i32.const 1 i32.const 8 call $fd_write
    if unreachable end
    i32.const 8 i32.load i32.const 44 i32.ne if unreachable end)
)
