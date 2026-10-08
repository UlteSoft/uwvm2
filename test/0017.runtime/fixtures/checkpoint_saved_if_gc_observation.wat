(module
  (type $node (struct (field i32)))
  (func (export "run") (result i32)
    (struct.new $node (i32.const 42))
    i32.const 1
    if (param (ref $node)) (result i32)
      ;; The actual original struct is no longer on the operand stack, but is
      ;; still the if/else saved entry value. Its precise root must stay live.
      drop (struct.new $node (i32.const 99)) drop i32.const 7
    else
      struct.get $node 0
    end))
