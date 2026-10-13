;; An exnref (ref null exn) table may not satisfy a (ref exn) table import.
;; The reference type of a mutable table must match in both directions.
(module
  (import "B" "tab" (table 1 4 (ref exn)))
  (func (export "_start")))
