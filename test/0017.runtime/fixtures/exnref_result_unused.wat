;; The result-side call metadata must preserve the full exnref carrier.
(module
  (func $unused (result exnref) ref.null exn)
  (func (export "_start")))
