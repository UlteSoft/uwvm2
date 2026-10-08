(module
  (type $base (sub (func (param i32) (result i32))))
  (type $unrelated (sub final (func (param i32) (result i32))))
  (func $target (type $unrelated) local.get 0)
  (elem declare func $target)
  (func (export "_start") i32.const 42 ref.func $target call_ref $base drop))
