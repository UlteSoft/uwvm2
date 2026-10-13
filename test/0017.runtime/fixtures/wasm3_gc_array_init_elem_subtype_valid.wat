(module
  (type $base (sub (func (result i32))))
  (type $child (sub final $base (func (result i32))))
  (type $array (array (mut (ref null $base))))
  (func $target (type $child) i32.const 42)
  (elem $segment (ref $child) (ref.func $target))
  (func $probe (result i32) (local $values (ref $array))
    i32.const 2 array.new_default $array local.set $values
    local.get $values i32.const 1 i32.const 0 i32.const 1 array.init_elem $array $segment
    local.get $values i32.const 0 array.get $array ref.is_null i32.eqz if unreachable end
    local.get $values i32.const 1 array.get $array ref.as_non_null call_ref $base)
  (func (export "_start") call $probe i32.const 42 i32.ne if unreachable end))
