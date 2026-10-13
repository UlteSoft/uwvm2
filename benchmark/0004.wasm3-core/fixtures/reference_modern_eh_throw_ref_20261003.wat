;; Source-only cross-VM control. Compile once under the Linux keeper's exact
;; wasm-tools closure; every admitted VM must execute those identical bytes.
;; Preserve a real exception reference across two catches and two throw_ref
;; uses. All throws are caught; two successful payload assertions return zero.
(module
  (tag $event (param i32))
  (global $saved (mut (ref null exn)) (ref.null exn))
  (func $original
    i32.const 47
    throw $event)
  (func (export "_start")
    block $first (result i32)
      try_table (catch $event $first)
        block $capture (result (ref exn))
          try_table (catch_all_ref $capture)
            call $original
          end
          unreachable
        end
        global.set $saved
        global.get $saved
        throw_ref
      end
      unreachable
    end
    i32.const 47
    i32.ne
    if unreachable end
    block $second (result i32)
      try_table (catch $event $second)
        global.get $saved
        throw_ref
      end
      unreachable
    end
    i32.const 47
    i32.ne
    if unreachable end))
