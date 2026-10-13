(module
(global $seed (mut i32) (i32.const 71))
(global $checksum (export "checksum") (mut i32) (i32.const 0))
(func $step (param $x i32) (result i32)
 local.get $x local.get $x i32.const 13 i32.shr_u i32.xor
 i32.const 1664525 i32.mul i32.const 1013904223 i32.add)
(type $ft (func (param i32) (result i32)))
(func $f (type $ft) local.get 0 call $step)
(func $h (type $ft) local.get 0 i32.const -1 i32.xor call $step)
(elem declare func $f $h)(table $t 2 funcref)
(func (export "_start") (local $x i32) (local $i i32) (local $p i32) (local $wide i64) (local $e exnref) 
 global.get $seed local.set $x i32.const 0 ref.func $f table.set $t i32.const 1 ref.func $h table.set $t
 loop $loop
 local.get $x local.get $x i32.const 1 i32.and call_indirect $t (type $ft)
 local.set $x
 local.get $i i32.const 1 i32.add local.tee $i i32.const 16384 i32.lt_u br_if $loop
 end
 local.get $x global.set $checksum
 local.get $x i32.const 1654394175 i32.ne if unreachable end
))