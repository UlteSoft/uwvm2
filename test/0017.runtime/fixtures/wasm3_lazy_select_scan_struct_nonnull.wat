(module
  (type $s (struct (field i32)))
  (func $later (result i32) i32.const 47)
  (func $probe (result i32)
    i32.const 31 struct.new $s i32.const 41 struct.new $s
    i32.const 0 select (result (ref $s)) struct.get $s 0
    i32.const 41 i32.ne if unreachable end
    call $later)
  (func (export "_start")
    call $probe i32.const 47 i32.ne if unreachable end))
