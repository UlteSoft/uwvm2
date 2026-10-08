;; Real cross-function throw -> catch_ref -> throw_ref -> outer catch.
;; A wrong tag payload traps in _start instead of silently accepting a load.
(module
  (tag $value (param i32))
  (func $raise i32.const 49 throw $value)
  (func (export "_start")
    (block $outer (result i32)
      (try_table (catch $value $outer)
        (block $inner (result i32 (ref exn))
          (try_table (catch_ref $value $inner)
            call $raise)
          unreachable)
        throw_ref)
      unreachable)
    i32.const 49
    i32.ne
    if unreachable end))
