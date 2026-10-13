(module $"module\0a\1b\22\5cescaped"
  (type $payload (func (param i32)))
  (tag $alpha (type $payload))
  (tag $beta (type $payload))
  ;; Real original throw is function 0. Its immutable trace must survive throw_ref.
  (func $"throw\0a\1b\22\5cescaped"
    i32.const 42
    throw $beta)
  (func $capturer (result (ref exn))
    block $caught (result (ref exn))
      try_table (catch_all_ref $caught)
        call 0
      end
      unreachable
    end)
  (func $entry (export "_start")
    call $capturer
    call $rethrower)
  ;; Function 3 is entered only after capture and must not replace the old trace.
  (func $rethrower (param (ref exn))
    local.get 0
    throw_ref))
