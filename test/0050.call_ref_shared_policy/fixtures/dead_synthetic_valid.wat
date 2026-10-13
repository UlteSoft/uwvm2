(module
  (type $target (func (param i32) (result i32)))
  (func $not_called (result i32) unreachable call_ref $target)
  (func (export "_start")))
