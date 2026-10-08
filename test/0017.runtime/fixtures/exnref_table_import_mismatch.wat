;; A provider exnref table cannot satisfy an externref table import.
(module
  (import "P" "tab" (table 1 4 externref))
  (func (export "_start")))
