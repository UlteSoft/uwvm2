;; Native EH must cross $raise -> _start. Local direct-throw folding cannot apply.
;; Every payload is checked, as is the number of executed catches.
(module
  (tag $event (param i32))
  (func $raise (param i32)
    local.get 0
    throw $event)
  (func (export "_start") (local $index i32) (local $catches i32)
    block $done
      loop $again
        local.get $index
        i32.const 32
        i32.ge_u
        br_if $done
        block $caught (result i32)
          try_table (catch $event $caught)
            local.get $index
            call $raise
          end
          unreachable
        end
        local.get $index
        i32.ne
        if unreachable end
        local.get $catches
        i32.const 1
        i32.add
        local.set $catches
        local.get $index
        i32.const 1
        i32.add
        local.set $index
        br $again
      end
    end
    local.get $catches
    i32.const 32
    i32.ne
    if unreachable end))
