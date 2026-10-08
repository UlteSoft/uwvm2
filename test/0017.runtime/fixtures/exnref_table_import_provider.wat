;; The imported table and tag remain owned by this module after linking.
(module
  (tag $t (export "t") (param i32))
  (table (export "tab") 1 4 exnref)
  (func (export "raise") (param i32)
    local.get 0
    throw $t)
  (func (export "clear")
    i32.const 0
    ref.null exn
    table.size 0
    table.fill 0))
