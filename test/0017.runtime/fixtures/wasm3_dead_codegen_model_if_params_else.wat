(module
  (type $args (func (param i32 i32) (result i32)))
  (func $probe (result i32)
    unreachable i32.const 0 if (type $args)
      i32.const 0 select
    else i32.const 0 select end)
  (func (export "_start") call $probe drop))
