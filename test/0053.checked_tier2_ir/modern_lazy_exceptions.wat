(module
  (tag $error (param i32))
  (func $caught (result i32)
    block $done (result i32)
      try_table (catch $error $done) i32.const 7 throw $error end
      unreachable
    end)
  (func (export "_start") call $caught i32.const 7 i32.ne if unreachable end))
