(module
  ;; New Core 3 tail syntax plus ordinary same-function recursion. Tail entry
  ;; must receive a new incarnation even when native frame/CFA is reused.
  (func $helper (result i32) i32.const 7)
  (func $recursive (param $remaining i32) (result i32)
    local.get $remaining i32.eqz
    if (result i32)
      return_call $helper
    else
      local.get $remaining i32.const 1 i32.sub call $recursive
    end)
  (func (export "_start") (result i32)
    nop i32.const 3 call $recursive
    i32.const 3 call $recursive i32.add))
