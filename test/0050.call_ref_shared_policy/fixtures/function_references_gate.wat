(module
  (type $target (func))
  (func $target (type $target))
  (elem declare func $target)
  (func (export "_start") ref.func $target call_ref $target))
