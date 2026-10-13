(module
  (type $signature (func (result i32)))
  (func $value (type $signature) i32.const 17)
  (elem declare func $value)
  (table $destination 1 funcref)
  (table $source 1 funcref)
  (func (export "_start")
    i32.const 0 ref.func $value table.set $source
    i32.const 0 i32.const 0 i32.const 1 table.copy $destination $source
    i32.const 0 call_indirect $destination (type $signature)
    i32.const 17 i32.ne if unreachable end))
