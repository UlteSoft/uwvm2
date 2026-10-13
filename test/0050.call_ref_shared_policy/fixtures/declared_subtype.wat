(module
  (type $base (sub (func (param i32) (result i32))))
  (type $derived (sub final $base (func (param i32) (result i32))))
  (func $target (type $derived) local.get 0 i32.const 7 i32.add)
  (elem declare func $target)
  (func (export "_start") (local $r (ref null $base))
    ref.func $target local.set $r
    i32.const 35 local.get $r call_ref $base
    i32.const 42 i32.ne if unreachable end))
