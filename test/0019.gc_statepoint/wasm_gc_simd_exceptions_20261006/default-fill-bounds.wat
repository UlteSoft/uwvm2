(module (type $a (array (mut i64)))
 (func (export "_start") i32.const 16 array.new_default $a i32.const 16 i64.const 1 i32.const 1 array.fill $a))