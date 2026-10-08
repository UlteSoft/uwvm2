;; Active and passive Core 3 exnref element segments use the table element ABI.
(module
  (table 2 exnref)
  (elem (i32.const 0) exnref (ref.null exn))
  (elem $passive exnref (ref.null exn))
  (func (export "_start")
    i32.const 1
    i32.const 0
    i32.const 1
    table.init 0 $passive
    elem.drop $passive
    i32.const 0
    table.get 0
    ref.is_null
    i32.eqz
    if unreachable end
    i32.const 1
    table.get 0
    ref.is_null
    i32.eqz
    if unreachable end))
