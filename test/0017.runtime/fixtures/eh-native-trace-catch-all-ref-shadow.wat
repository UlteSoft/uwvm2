;; Inner catch_all_ref shadows an outer non-ref catch for EVERY guest tag.
;; Store, re-catch without a ref, then escape the old reference to verify trace retention.
(module
  (tag $event (param i32))
  (global $saved (mut (ref null exn)) (ref.null exn))
  (func $probe
    block $outer (result i32)
      try_table (catch $event $outer)
        block $inner (result (ref exn))
          try_table (catch_all_ref $inner)
            i32.const 47
            throw $event
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
    if unreachable end)
  (func (export "_start")
    call $probe
    global.get $saved
    throw_ref))
