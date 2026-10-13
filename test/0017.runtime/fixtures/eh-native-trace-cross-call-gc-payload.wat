;; A native cross-function throw owns the struct payload during unwind.
;; The receiver keeps the original value live across further allocations.
(module
  (type $box (struct (field i32)))
  (tag $event (param (ref $box)))
  (func $raise (param (ref $box))
    local.get 0
    throw $event)
  (func (export "_start") (local $received (ref null $box))
    block $caught (result (ref $box))
      try_table (catch $event $caught)
        i32.const 73
        struct.new $box
        call $raise
      end
      unreachable
    end
    local.set $received
    i32.const 99
    struct.new $box
    drop
    local.get $received
    struct.get $box 0
    i32.const 73
    i32.ne
    if unreachable end))
