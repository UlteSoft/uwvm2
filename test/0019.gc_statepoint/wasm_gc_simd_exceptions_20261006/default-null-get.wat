(module (type $a (array (mut i64)))
 (func (export "_start") i32.const 0 array.new_default $a drop ref.null $a i32.const 0 array.get $a drop))