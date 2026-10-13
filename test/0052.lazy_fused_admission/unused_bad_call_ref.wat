(module
  (type $fn (func (param i32) (result i32)))
  (func $identity (type $fn) local.get 0)
  (elem declare func $identity)
  (func $unused f32.const 1 ref.func $identity call_ref $fn drop)
  (func (export "_start")))
