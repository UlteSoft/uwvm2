(module
  (func $answer)
  (table 1 (ref func) (ref.func $answer))
  ;; An expression segment may contain null, and cannot initialize a non-null table.
  (elem (table 0) (i32.const 0) funcref (ref.null func)))
