;; B re-exports P's exact table. The explicit nullable spelling must match
;; P's exnref shorthand without creating a second table or root owner.
(module
  (import "P" "tab" (table $alias 1 4 (ref null exn)))
  (export "tab" (table $alias))
  (func (export "clear")
    i32.const 0
    ref.null exn
    table.size $alias
    table.fill $alias))
