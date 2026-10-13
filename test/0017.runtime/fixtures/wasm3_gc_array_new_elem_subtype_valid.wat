(module
  (type $base (sub (func (result i32))))
  (type $child (sub final $base (func (result i32))))
  (type $array (array (ref $base)))
  (func $target (type $child) i32.const 42)
  (elem $segment (ref $child) (ref.func $target))
  (func $probe (result i32)
    i32.const 0 i32.const 1 array.new_elem $array $segment
    i32.const 0 array.get $array call_ref $base)
  (func (export "_start") call $probe i32.const 42 i32.ne if unreachable end))
