(module
  (type $signature (func (result i32)))
  (func $value (type $signature) i32.const 9)
  (elem declare func $value)
  (table $erased 1 funcref)
  (table $typed 1 (ref null $signature))
  (func (export "_start")
    i32.const 0 ref.func $value table.set $typed
    i32.const 0 i32.const 0 i32.const 1 table.copy $erased $typed
    i32.const 0 call_indirect $erased (type $signature)
    i32.const 9 i32.ne if unreachable end))
