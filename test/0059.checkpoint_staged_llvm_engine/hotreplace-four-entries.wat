(module
  (type $box (struct (field i32)))
  (type $worker (func (param i32) (result i32)))
  (tag $payload (param i32))
  (memory $wide i64 1)
  (table $dispatch i64 1 (ref null $worker))
  (global $expected (mut i32) (i32.const 42))
  (elem (table $dispatch) (i64.const 0) (ref $worker) (ref.func $hot))
  (func $hot (export "hot") (type $worker) (param $x i32) (result i32)
    (local $root (ref $box))
    i32.const 7 struct.new $box local.set $root
    (block $caught (result i32)
      (try_table (catch $payload $caught)
        local.get $x throw $payload)
      unreachable)
    local.get $root struct.get $box 0 i32.add)
  (func $main
    i64.const 0 i32.const 35 i64.const 0 call_indirect $dispatch (type $worker)
    i32.store $wide
    i64.const 0 i32.load $wide global.get $expected i32.ne
    if unreachable end)
  (start $main))
