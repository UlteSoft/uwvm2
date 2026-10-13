(module (global $vector (mut v128) (v128.const i32x4 1902261026 2156995121 305419896 4273772594))
 (global $checksumv (export "checksumv") (mut v128) (v128.const i32x4 0 0 0 0)) (global $mask (mut v128) (v128.const i8x16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0))
 (func (export "_start") (local $i i32) (local $v v128)
 global.get $vector local.set $v
 loop $loop local.get $v global.get $mask v128.const i8x16 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 v128.and i8x16.relaxed_swizzle v128.const i32x4 3 5 7 11 i32x4.add
 local.get $v i32.const 3 i32x4.shr_u v128.xor local.set $v
 local.get $i i32.const 1 i32.add local.tee $i i32.const 16384 i32.lt_u br_if $loop end
 local.get $v global.set $checksumv
 local.get $v v128.const i32x4 841251171 3526543109 241698853 4198882097 v128.xor v128.any_true if unreachable end))