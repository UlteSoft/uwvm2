(module
  (type $empty (func))
  (type $box (struct (field i32)))
  (import "full-admission-provider" "memory" (memory 1 1))
  (import "full-admission-provider" "table" (table 1 1 funcref))
  (func $probe (result i32) i32.const 42 struct.new $box struct.get $box 0)
  (func $replacement (type $empty))
  (data (i32.const 0) "\a5\5a")
  (elem (i32.const 0) func $replacement)
  (func (export "_start")
    ;; Must consume the table view AFTER the real active element initializes it.
    i32.const 0 call_indirect (type $empty)
    call $probe i32.const 42 i32.ne if unreachable end
    i32.const 2 i32.const 158 i32.store8))
