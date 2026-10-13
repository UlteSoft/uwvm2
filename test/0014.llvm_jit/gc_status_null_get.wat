(module (type $S (struct (field (mut i32))))
 (func (export "_start") (drop (struct.get $S 0 (ref.null $S)))))
