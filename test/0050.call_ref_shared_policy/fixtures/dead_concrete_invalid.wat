(module
  (type $target (func (param i32) (result i32)))
  (func $target (type $target) local.get 0)
  (elem declare func $target)
  (func $not_called (result i32)
    unreachable i64.const 0 ref.func $target call_ref $target)
  (func (export "_start")))
