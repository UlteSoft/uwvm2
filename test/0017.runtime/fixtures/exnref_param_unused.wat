;; A Core 3 exnref parameter occupies one complete tagged reference ABI slot.
;; Full compilation must publish call metadata even when this function is unused.
(module
  (func $unused (param exnref))
  (func (export "_start")))
