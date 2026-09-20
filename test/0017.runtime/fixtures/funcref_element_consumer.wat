(module
  (type $result_i32 (func (result i32)))
  (import "A" "target_ref" (global $target_ref funcref))
  (table 1 funcref)
  ;; Again occupy local function index 0 so a provider-index/consumer-index mix-up is observable.
  (func $decoy (type $result_i32) (result i32)
    i32.const 17)
  (elem $refs funcref (global.get 0))
  (func $start
    i32.const 0
    i32.const 0
    i32.const 1
    table.init $refs
    elem.drop $refs
    i32.const 0
    call_indirect (type $result_i32)
    i32.const 73
    i32.ne
    if unreachable end)
  (start $start))
