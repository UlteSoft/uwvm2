(module (global $seed (mut i32) (i32.const 71))
(global $checksum (export "checksum") (mut i32) (i32.const 0))
(func $step (param $x i32) (result i32)
 local.get $x local.get $x i32.const 13 i32.shr_u i32.xor
 i32.const 1664525 i32.mul i32.const 1013904223 i32.add)(func $tail (param $n i32) (param $x i32) (result i32)
 local.get $n i32.eqz if (result i32) local.get $x
 else local.get $n i32.const 1 i32.sub local.get $x call $step return_call $tail end)(func (export "_start") i32.const 16384 global.get $seed call $tail global.set $checksum global.get $checksum i32.const 551227849 i32.ne if unreachable end))