(module (type $ft (func (result i32))) (type $node (struct (field i32))) (type $funs (array (mut funcref)))
 (func $seed (type $ft) (result i32) i32.const 7)
 (func $other (type $ft) (result i32) i32.const 43)
 (elem declare func $seed) (elem $change func $other)
 (func (export "_start") (local $a (ref null $funs)) (local $j i32)
  ref.func $seed i32.const 4096 array.new $funs local.set $a
  local.get $a i32.const 4095 i32.const 0 i32.const 1 array.init_elem $funs $change
  loop $garbage i32.const 17 struct.new $node drop local.get $j i32.const 1 i32.add local.tee $j i32.const 8192 i32.lt_u br_if $garbage end
  local.get $a i32.const 4095 array.get $funs ref.cast (ref $ft) call_ref $ft i32.const 43 i32.ne if unreachable end
  local.get $a i32.const 0 array.get $funs ref.cast (ref $ft) call_ref $ft i32.const 7 i32.ne if unreachable end))