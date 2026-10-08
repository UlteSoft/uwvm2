(module
      (tag $event (param i32))
      (global $catches (mut i32) (i32.const 0))
      (func $step (param $x i32) (result i32)
        (local $next i32)
        local.get $x
        i32.const 1664525
        i32.mul
        i32.const 1013904223
        i32.add
        local.set $next
        local.get $x
        i32.const 15
        i32.and
        i32.eqz
        if
          local.get $next
          throw $event
        end
        local.get $next)
      (func (export "_start")
        (local $index i32) (local $value i32)
        block $done
          loop $again
            local.get $index
            i32.const 64000000
            i32.ge_u
            br_if $done
            block $after (result i32)
          block $caught (result i32)
            try_table (result i32) (catch $event $caught)
              local.get $value
              call $step
            end
            br $after
          end
          global.get $catches
          i32.const 1
          i32.add
          global.set $catches
        end
            local.set $value
            local.get $index
            i32.const 1
            i32.add
            local.set $index
            br $again
          end
        end
        local.get $value
        i32.const 0x11ecd000
        i32.ne
        if unreachable end
        global.get $catches
        i32.const 4000000
        i32.ne
        if unreachable end))
