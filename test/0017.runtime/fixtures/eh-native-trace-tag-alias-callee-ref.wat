;; Load eh-native-trace-tag-alias-provider.wasm as EHOwner.
;; The provider throw crosses a call before an aliased inner ref catch consumes it.
;; The final uncaught exception must retain the provider's ORIGINAL throw frame.
(module
  (import "EHOwner" "X" (tag $x (param i32)))
  (import "EHOwner" "Y" (tag $y (param i32)))
  (import "EHOwner" "raise" (func $raise))
  (global $saved (mut (ref null exn)) (ref.null exn))
  (func $probe
    block $outer (result i32)
      try_table (catch $x $outer)
        block $inner (result i32 (ref exn))
          try_table (catch_ref $y $inner)
            call $raise
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
