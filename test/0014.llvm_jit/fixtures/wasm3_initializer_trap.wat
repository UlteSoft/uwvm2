(module
  (global $sum i32 (i32.add (i32.const 1) (i32.const 2)))
  (func $leaf global.get $sum drop unreachable)
  (func $caller call $leaf)
  (func (export "_start") call $caller))
