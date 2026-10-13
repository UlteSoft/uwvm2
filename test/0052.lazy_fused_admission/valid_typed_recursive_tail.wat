(module
  (type $signature (func (param i32) (result i32)))
  (func $sum (type $signature)
    local.get 0 i32.eqz
    if (result i32) i32.const 0
    else local.get 0 local.get 0 i32.const 1 i32.sub call $sum i32.add end)
  (elem declare func $sum)
  (func $tail (type $signature)
    local.get 0 ref.func $sum return_call_ref $signature)
  (func (export "_start")
    i32.const 9 call $tail i32.const 45 i32.ne if unreachable end))
