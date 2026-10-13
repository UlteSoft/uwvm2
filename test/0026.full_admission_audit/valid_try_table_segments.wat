(module
  (type $empty (func))
  (import "full-admission-provider" "memory" (memory 1 1))
  (import "full-admission-provider" "table" (table 1 1 funcref))
  (tag $error (param i32))
  (func $probe (result i32)
    block $caught (result i32)
      try_table (catch $error $caught) i32.const 42 throw $error end
      unreachable
    end)
  (func $replacement (type $empty))
  (data (i32.const 0) "\a5\5a")
  (elem (i32.const 0) func $replacement)
  (func (export "_start")
    ;; Must consume the table view AFTER the real active element initializes it.
    i32.const 0 call_indirect (type $empty)
    call $probe i32.const 42 i32.ne if unreachable end
    i32.const 2 i32.const 158 i32.store8))
