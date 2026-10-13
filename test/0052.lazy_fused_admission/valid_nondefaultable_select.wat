(module
  (func $choose (result i32) (local (ref i31))
    i32.const 47 ref.i31 local.set 0
    local.get 0 local.get 0 i32.const 1 select (result (ref i31)) i31.get_u)
  (func (export "_start") call $choose i32.const 47 i32.ne if unreachable end))
