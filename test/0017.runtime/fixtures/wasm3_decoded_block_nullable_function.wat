(module
 (type $f (func))
 (func $later (result i32) i32.const 47)
 (func $probe (result i32)
  block (result (ref null $f)) ref.null $f end drop call $later)
 (func (export "_start") call $probe i32.const 47 i32.ne if unreachable end))
