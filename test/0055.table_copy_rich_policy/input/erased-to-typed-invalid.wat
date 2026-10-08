(module
  (type $signature (func (result i32)))
  (table $erased 1 funcref)
  (table $typed 1 (ref null $signature))
  ;; Zero length cannot make an invalid reference conversion type-safe.
  (func (export "_start")
    i32.const 0 i32.const 0 i32.const 0 table.copy $typed $erased))
