(module
  (type $box (struct (field i32)))
  (func $nullable (param (ref null $box)) (result i32)
    local.get 0 ref.is_null)
  (func $nonnullable (param (ref $box)) (result i32)
    local.get 0 ref.is_null)
  (func (export "_start")
    i32.const 42 struct.new $box call $nonnullable
    if unreachable end
    ref.null $box call $nullable
    i32.const 1 i32.ne if unreachable end))
