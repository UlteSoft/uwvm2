(module
  (type $box (struct (field i32)))
  (func $probe (result i32) i32.const 42 struct.new $box struct.get $box 0)
  (func (export "_start") call $probe drop))
