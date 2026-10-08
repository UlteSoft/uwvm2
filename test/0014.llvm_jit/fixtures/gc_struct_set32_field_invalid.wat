;; Validation failure before any lowering/helper invocation.
(module
  (type $s (struct (field (mut i32))))
  (func (export "_start")
    (struct.set $s 1 (struct.new $s (i32.const 1)) (i32.const 7))))
