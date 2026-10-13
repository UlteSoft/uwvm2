(module
  (type $s (struct (field (mut i32))))
  (func (export "_start")
    (struct.set $s 0 (ref.null $s) (i32.const 7))))
