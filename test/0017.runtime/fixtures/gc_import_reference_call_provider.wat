;; Cross-module aggregate parameter passing through a shared table and a funcref.
(module
  (type $unrelated (func))
  (type $record (struct (field i32)))
  (type $take (func (param (ref $record)) (result i32)))
  (table (export "t") 1 funcref)
  (func $take (type $take) (param $value (ref $record)) (result i32)
    local.get $value
    struct.get $record 0)
  (elem (i32.const 0) func $take)
  (export "take" (func $take)))
