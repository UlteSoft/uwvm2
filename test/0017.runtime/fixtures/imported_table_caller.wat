(module
  (type $result_i32 (func (result i32)))
  (import "A" "shared" (table 1 funcref))
  ;; C also has a function at index 0, but the table element belongs to B.
  (func $decoy (type $result_i32) (result i32)
    i32.const 19)
  (func $start
    i32.const 0
    call_indirect (type $result_i32)
    i32.const 83
    i32.ne
    if unreachable end)
  (start $start))
