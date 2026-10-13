(module
  (type $leaf (struct (field i32)))
  (type $identity (func (param i32) (result i32)))
  (import "staged-provider" "identity" (func $identity (type $identity)))
  (memory (export "memory") i64 1)
  (table (export "targets") i64 1 (ref null $identity))
  (elem (i64.const 0) (ref $identity) (ref.func $identity))
  (global (export "expected") (mut i32) (i32.const 37))
  (tag $payload (param i32))
  (func $hot (export "hot") (result i32) i32.const 37)
  (func (export "_start") (local $root (ref $leaf)) (local $value i32)
    i32.const 7 struct.new $leaf local.set $root
    block $caught (result i32)
      try_table (catch $payload $caught)
        call $hot throw $payload
      end
      unreachable
    end
    local.set $value
    i64.const 0
    local.get $value
    i64.const 0
    call_indirect (type $identity)
    i32.store
    i64.const 0 i32.load global.get 0 i32.ne
    if unreachable end
    local.get $root struct.get $leaf 0 i32.const 7 i32.ne
    if unreachable end))
