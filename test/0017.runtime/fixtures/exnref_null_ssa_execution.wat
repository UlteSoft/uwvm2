;; A null exnref is a normal 16-byte Wasm value: it can pass through a local
;; before ref.is_null. This must not depend on an adjacent throw_ref shortcut.
(module
  (func (export "_start")
    (local $saved (ref null exn))
    ref.null exn
    local.set $saved
    local.get $saved
    ref.is_null
    i32.eqz
    if unreachable end))
