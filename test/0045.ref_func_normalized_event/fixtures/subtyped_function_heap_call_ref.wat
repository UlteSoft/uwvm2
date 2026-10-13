(module
  (type $base (sub (func (result i32))))
  (type $derived (sub $base (func (result i32))))
  (func $target (type $derived) (result i32) i32.const 42)
  (elem declare func $target)
  (func (export "_start")
    ref.func $target call_ref $base
    i32.const 42 i32.ne if unreachable end)
)
