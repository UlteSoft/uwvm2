(module
(global $seed (mut i32) (i32.const 71))
(global $checksum (export "checksum") (mut i32) (i32.const 0))
(func $step (param $x i32) (result i32)
 local.get $x local.get $x i32.const 13 i32.shr_u i32.xor
 i32.const 1664525 i32.mul i32.const 1013904223 i32.add)
(memory (export "memory") 1)
(func (export "_start") (local $x i32) (local $i i32) (local $p i32) (local $wide i64) (local $e exnref) 
 global.get $seed local.set $x 
 loop $loop
 i32.const 0 local.get $x i32.const 255 i32.and i32.const 64 memory.fill
i32.const 128 i32.const 0 i32.const 64 memory.copy
local.get $x i32.const 8 i32.shr_u i32.const 63 i32.and i32.const 128 i32.add i32.load8_u
local.get $x i32.xor call $step
 local.set $x
 local.get $i i32.const 1 i32.add local.tee $i i32.const 16384 i32.lt_u br_if $loop
 end
 local.get $x global.set $checksum
 local.get $x i32.const 204273230 i32.ne if unreachable end
))