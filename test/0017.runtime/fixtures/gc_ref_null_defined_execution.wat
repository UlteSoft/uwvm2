;; Core 3 GC: an indexed struct heap must not be misclassified as a function.
(module
  (type $pair (struct (field i32)))
  (func (export "_start") (local $value (ref null $pair))
    ref.null $pair
    local.set $value
    local.get $value
    ref.is_null
    i32.eqz
    if unreachable end))
