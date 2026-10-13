(module
  (func $target)
  (elem declare func $target)
  (func (export "_start")
    ref.func $target ref.is_null if unreachable end)
)
