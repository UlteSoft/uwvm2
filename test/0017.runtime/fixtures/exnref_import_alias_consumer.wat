;; Both imported tag names identify the provider's one tag. catch_ref must
;; retain that exact exception across the imported call and throw_ref must
;; rethrow it with its original tag and i32 payload.
(module
  (import "p" "raise" (func $raise (param i32)))
  (import "p" "t" (tag $caught (param i32)))
  (import "p" "t" (tag $rethrown (param i32)))
  (func (export "_start")
    (local $saved (ref null exn))
    block $out (result i32)
      try_table (catch $rethrown $out)
        block $inner (result i32 (ref exn))
          try_table (catch_ref $caught $inner)
            i32.const 42
            call $raise
          end
          unreachable
        end
        local.set $saved
        drop
        local.get $saved
        throw_ref
      end
      unreachable
    end
    i32.const 42
    i32.ne
    if unreachable end))
