(module
  (import "full-admission-provider" "memory" (memory 1 1))
  (import "full-admission-provider" "table" (table 1 1 funcref))
  ;; Invalid even when never called: a non-defaultable local is uninitialized.
  (func $invalid (result (ref any)) (local (ref any)) local.get 0)
  (func $replacement)
  (data (i32.const 0) "\a5\5a")
  (elem (i32.const 0) func $replacement)
  (func (export "_start")))
