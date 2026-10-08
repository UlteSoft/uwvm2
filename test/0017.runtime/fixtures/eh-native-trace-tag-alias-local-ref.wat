;; Load eh-native-trace-tag-alias-provider.wasm as EHOwner.
;; The inner catch_ref Y must shadow outer catch X: X/Y alias one instance.
;; After a non-ref re-catch, escape the saved reference to check its ORIGINAL trace.
(module
  (import "EHOwner" "X" (tag $x (param i32)))
  (import "EHOwner" "Y" (tag $y (param i32)))
  (global $saved (mut (ref null exn)) (ref.null exn))
  (func $probe
    block $outer (result i32)
      try_table (catch $x $outer)
        block $inner (result i32 (ref exn))
          try_table (catch_ref $y $inner)
            i32.const 41
            throw $x
          end
          unreachable
        end
        global.set $saved
        i32.const 41
        i32.ne
        if unreachable end
        global.get $saved
        throw_ref
      end
      unreachable
    end
    i32.const 41
    i32.ne
    if unreachable end)
  (func (export "_start")
    call $probe
    global.get $saved
    throw_ref))
