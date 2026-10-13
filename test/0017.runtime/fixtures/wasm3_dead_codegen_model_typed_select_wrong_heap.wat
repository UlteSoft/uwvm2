(module
  (type $s (struct (field i32)))
  (func $probe (result i32)
    block (result (ref any) (ref any)) unreachable end
    i32.const 0 select (result (ref $s)) drop i32.const 0)
  (func (export "_start") call $probe drop))
