(module $"module\0a\1b\22\5cescaped"
  ;; Both tags have identical signatures, but distinct actual instance identities.
  (type $payload (func (param i32)))
  (tag $alpha (type $payload))
  (tag $beta (type $payload))
  (func $"throw\0a\1b\22\5cescaped" (export "_start")
    i32.const 42
    throw $beta))
