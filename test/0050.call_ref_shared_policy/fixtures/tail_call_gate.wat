(module
  (type $target (func))
  (func $target (type $target))
  (elem declare func $target)
  (func $invoke (type $target) ref.func $target return_call_ref $target)
  (func (export "_start") call $invoke))
