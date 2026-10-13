(module
(global $seed i32 (i32.add (i32.const 31) (i32.const 40)))
(global $checksum (export "checksum") (mut i32) (i32.const 0))
(func $step (param $x i32) (result i32)
 local.get $x local.get $x i32.const 13 i32.shr_u i32.xor
 i32.const 1664525 i32.mul i32.const 1013904223 i32.add)

(func (export "_start") (local $x i32) (local $i i32) (local $p i32) (local $wide i64)  
 global.get $seed local.set $x 
 loop $loop
 local.get $x call $step
 local.set $x
 local.get $i i32.const 1 i32.add local.tee $i i32.const 16384 i32.lt_u br_if $loop
 end
 local.get $x global.set $checksum
 local.get $x i32.const 551227849 i32.ne if unreachable end
))