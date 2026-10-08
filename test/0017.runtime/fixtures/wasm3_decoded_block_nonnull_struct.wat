(module
 (type $s (struct (field i32)))
 (func $probe (result i32)
  block (result (ref $s)) i32.const 41 struct.new $s end
  struct.get $s 0)
 (func (export "_start") call $probe i32.const 41 i32.ne if unreachable end))
