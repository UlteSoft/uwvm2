(module
  (type $base (sub (func (param i32) (result i64 i32))))
  (type $derived (sub final $base (func (param i32) (result i64 i32))))
  (func $target (type $derived) (param $n i32) (result i64 i32)
    local.get $n i32.eqz
    if
      i64.const 400 i32.const 42 return
    end
    local.get $n i32.const 1 i32.sub ref.func $target return_call_ref $base)
  (elem declare func $target)
  (func (export "_start") (local $second i32)
    i32.const 100000 ref.func $target call_ref $base
    local.set $second
    i64.const 400 i64.ne if unreachable end
    local.get $second i32.const 42 i32.ne if unreachable end))
