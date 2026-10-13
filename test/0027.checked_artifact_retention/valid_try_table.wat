(module
  (tag $failure (param i32))
  (func $probe (result i32)
    block $caught (result i32)
      try_table (catch $failure $caught) i32.const 42 throw $failure end
      unreachable
    end)
  (func (export "_start") call $probe drop))
