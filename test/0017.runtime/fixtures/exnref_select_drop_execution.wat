;; Core 3 nullable exnref must survive local.tee, drop, typed select and
;; ref.is_null. This has no GC or function-reference instruction.
(module
  (func (export "_start")
    (local $held (ref null exn))
    ref.null exn
    local.tee $held
    drop
    local.get $held
    ref.null exn
    i32.const 1
    select (result (ref null exn))
    ref.is_null
    i32.eqz
    if unreachable end))
