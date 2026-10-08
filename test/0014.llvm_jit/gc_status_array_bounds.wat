(module (type $A (array (mut i64)))
 (func (export "_start") (drop (array.get $A (array.new_default $A (i32.const 1)) (i32.const 1)))))
