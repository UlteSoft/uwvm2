(module
  (type $result_i32 (func (result i32)))
  (table 1 funcref)
  (func $target (type $result_i32) (result i32) i32.const 73)
  (global $source funcref (ref.func $target))
  (global $saved (mut funcref) (ref.null func))
  (global $external (mut externref) (ref.null extern))
  (func $start
    ;; Mutable reference globals must round-trip the whole tagged payload, not only its pointer/index.
    global.get $saved
    ref.is_null
    i32.eqz
    if unreachable end
    global.get $source
    global.set $saved
    i32.const 0
    global.get $saved
    table.set
    i32.const 0
    call_indirect (type $result_i32)
    i32.const 73
    i32.ne
    if unreachable end
    ref.null func
    global.set $saved
    global.get $saved
    ref.is_null
    i32.eqz
    if unreachable end
    ref.null extern
    global.set $external
    global.get $external
    ref.is_null
    i32.eqz
    if unreachable end)
  (start $start))
