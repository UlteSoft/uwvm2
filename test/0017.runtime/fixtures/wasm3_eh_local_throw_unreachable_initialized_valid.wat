;; A prior initialized GC root remains readable and live after a caught throw.
(module
  (type $box (struct (field i32)))
  (tag $value)
  (func $probe (param $raise i32) (local $slot (ref $box))
    i32.const 7 struct.new $box local.set $slot
    block $handled
      try_table (catch_all $handled)
        local.get $raise
        if
          throw $value
          local.get $slot drop
        end
      end
    end
    local.get $slot struct.get $box 0 i32.const 7 i32.ne if unreachable end)
  (func (export "_start") i32.const 1 call $probe))
