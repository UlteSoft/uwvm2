;; Validation failure, not a native runtime mutation contract.
(module
  (type $s (struct (field i32)))
  (func (export "_start")
    (struct.set $s 0 (struct.new $s (i32.const 1)) (i32.const 7))))
