;; A normal protected call branches over a catch-only body. Both incoming
;; edges must present the same i32 result to the enclosing block's successor.
(module
  (tag $event (param i32))
  (func $step (result i32)
    i32.const 42)
  (func (export "_start")
    (local $value i32)
    block $after (result i32)
      block $caught (result i32)
        try_table (result i32) (catch $event $caught)
          call $step
        end
        br $after
      end
    end
    local.set $value
    local.get $value
    i32.const 42
    i32.ne
    if unreachable end))
