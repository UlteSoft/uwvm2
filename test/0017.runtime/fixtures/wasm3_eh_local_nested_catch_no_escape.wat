;; Neither nested exceptional frame exports its local-initialization state.
(module
  (type $box (struct (field i32)))
  (tag $value (param (ref $box)))
  (func $probe (local $slot (ref $box))
    block $caught (result (ref $box))
      try_table (catch $value $caught)
        try_table
          i32.const 23 struct.new $box local.tee $slot
          throw $value
        end
      end
      unreachable
    end
    drop
    local.get $slot drop)
  (func (export "_start") call $probe))
