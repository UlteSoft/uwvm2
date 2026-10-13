(module
  (func $probe (result i32)
    block (result i32 i32) unreachable end
    i32.const 0 select)
  (func (export "_start") call $probe drop))
