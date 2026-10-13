(module (type $A (array (mut i32))) (func (export "_start") (local $a (ref null $A)) (local $n i32) (local $sum i32)
 i32.const 0 i32.const 1024 array.new $A local.set $a
 (loop $L local.get $a local.get $n i32.const 1023 i32.and local.get $n array.set $A
 local.get $sum local.get $a local.get $n i32.const 1023 i32.and array.get $A i32.add local.set $sum
 local.get $n i32.const 1 i32.add local.tee $n i32.const 131072 i32.lt_u br_if $L)
 local.get $sum i32.const 4294901760 i32.ne if unreachable end
 i32.const 0 local.set $n
 (loop $V local.get $a local.get $n array.get $A local.get $n i32.const 130048 i32.add i32.ne if unreachable end
 local.get $n i32.const 1 i32.add local.tee $n i32.const 1024 i32.lt_u br_if $V)))
