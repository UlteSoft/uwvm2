(module
  (type $target (func))
  (func $bad (result externref)
    ref.null $target
    ref.as_non_null)
  (func (export "_start") call $bad drop))
