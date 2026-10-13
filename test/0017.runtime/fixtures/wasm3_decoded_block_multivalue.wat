(module
 (type $sig (func (param i32 i64) (result i64 i32)))
 (func $probe (result i32) (local $result i32)
  i32.const 3 i64.const 9
  block (type $sig) drop drop i64.const 9 i32.const 47 end
  local.set $result drop local.get $result)
 (func (export "_start") call $probe i32.const 47 i32.ne if unreachable end))
