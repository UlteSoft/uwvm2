(module
  (type $signature (func (result i32)))
  (func $target (type $signature) (result i32) i32.const 42)
  (elem declare func $target)
  (func (export "_start") (local $saved (ref $signature))
    ref.func $target local.set $saved
    local.get $saved call_ref $signature
    i32.const 42 i32.ne if unreachable end)
)
