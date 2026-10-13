(module (type $f (func)) (func $callee (type $f)) (elem declare func $callee)
 (func $later (result i32) i32.const 47)
 (func $probe (result i32) block (result funcref) ref.func $callee end drop call $later)
 (func (export "_start") call $probe i32.const 47 i32.ne if unreachable end))
