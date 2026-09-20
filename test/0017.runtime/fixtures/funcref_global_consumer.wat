(module
  (type $result_i32 (func (result i32)))
  (import "A" "target_ref" (global $target_ref funcref))
  (table 4 8 funcref)
  ;; Deliberately shares function index 0 with A.target. Resolving A's bare index in this module would return 11.
  (func $decoy (type $result_i32) (result i32)
    i32.const 11)
  (func $start
    i32.const 0
    global.get $target_ref
    table.set
    i32.const 0
    call_indirect (type $result_i32)
    i32.const 73
    i32.ne
    if unreachable end

    global.get $target_ref
    i32.const 1
    table.grow
    drop
    i32.const 4
    call_indirect (type $result_i32)
    i32.const 73
    i32.ne
    if unreachable end

    i32.const 2
    global.get $target_ref
    i32.const 1
    table.fill
    i32.const 2
    call_indirect (type $result_i32)
    i32.const 73
    i32.ne
    if unreachable end)
  (start $start))
