;; Inner local.tee cannot initialize a nondefaultable local after try_table/end.
(module
  (type $box (struct (field i32)))
  (tag $value (param (ref $box)))
  (func $probe (local $slot (ref $box))
    block $caught (result (ref $box))
      try_table (catch $value $caught)
        i32.const 23 struct.new $box local.tee $slot
        local.get $slot ref.eq i32.eqz if unreachable end
        local.get $slot throw $value
      end
      unreachable
    end
    drop
    local.get $slot struct.get $box 0 drop)
  (func (export "_start") call $probe))
