(module (func $later (result i32) i32.const 47)
 (func $probe (result i32) block (result externref)
  ref.null extern any.convert_extern extern.convert_any end drop call $later)
 (func (export "_start") call $probe i32.const 47 i32.ne if unreachable end))
