(module
  (type $args (func (param i32 i32) (result i32 i32)))
  (func $probe (result i32)
    unreachable i32.const 0 if (type $args) unreachable end
    i32.const 0 select)
  (func (export "_start") call $probe drop))
