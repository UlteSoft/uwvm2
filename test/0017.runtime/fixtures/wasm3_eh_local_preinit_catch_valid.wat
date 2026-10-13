;; Prior definite initialization survives exceptional control-frame retirement.
(module
  (type $box (struct (field i32)))
  (tag $value (param (ref $box)))
  (func $probe (local $slot (ref $box))
    i32.const 7 struct.new $box local.set $slot
    block $caught (result (ref $box))
      try_table (catch $value $caught)
        i32.const 23 struct.new $box local.set $slot
        local.get $slot throw $value
      end
      unreachable
    end
    drop
    local.get $slot struct.get $box 0 i32.const 23 i32.ne if unreachable end)
  (func (export "_start") call $probe))
