(module
  (type $box (struct (field i32)))
  (import "full-admission-provider" "memory" (memory 1 1))
  (import "full-admission-provider" "table" (table 1 1 funcref))
  ;; struct.get_s requires a packed i8/i16 field; this field is i32.
  (func $invalid (result i32) i32.const 42 struct.new $box struct.get_s $box 0)
  (func $replacement)
  (data (i32.const 0) "\a5\5a")
  (elem (i32.const 0) func $replacement)
  (func (export "_start")))
