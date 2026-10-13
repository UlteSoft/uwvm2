;; Core 3 noexn is the bottom of the exn heap hierarchy. A nullable noexn
;; constant is a valid initializer for a nullable exnref global.
(module
  (global $saved (ref null exn) (ref.null noexn))
  (func (export "_start")
    global.get $saved
    ref.is_null
    i32.eqz
    if unreachable end))
