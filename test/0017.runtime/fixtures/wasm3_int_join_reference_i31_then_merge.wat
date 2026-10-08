(module
  (func $probe (param (ref i31)) (result i32)
    local.get 0 i32.const 1 if else unreachable end i31.get_s)
  (func (export "_start")
    i32.const 47 ref.i31 call $probe i32.const 47 i32.ne if unreachable end))
