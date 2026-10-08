(module
  (type $box (struct (field (mut i64))))
  (memory 1 1 shared)
  (data (i32.const 16) "\07\00\00\00\00\00\00\00")
  (func $entry (param $tag i64) (result i32)
    local.get $tag call $wait)
  (func $wait (param $tag i64) (result i32)
    (local $box (ref null $box)) (local $result i32)
    local.get $tag i64.const 100 i64.add struct.new $box local.set $box
    local.get $tag i64.const 1000 i64.add
    i32.const 16 i32.const 7 i64.const -1 memory.atomic.wait32
    local.set $result drop local.get $result)
  (func $notify (param $count i32) (result i32)
    i32.const 16 local.get $count memory.atomic.notify))
