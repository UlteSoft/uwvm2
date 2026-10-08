(module (func $later (result i32) i32.const 47)
 (func $probe (result i32) block (result funcref) ref.null nofunc end drop call $later)
 (func (export "_start") call $probe i32.const 47 i32.ne if unreachable end))
