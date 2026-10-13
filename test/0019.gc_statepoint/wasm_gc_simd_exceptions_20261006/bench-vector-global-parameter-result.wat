(module (tag $t (param v128))
 (global $v (mut v128) (v128.const i32x4 0x7fa12345 0xdeadbeef 0x7ff00001 0x81234567))
 (func $rotate (param $x v128) (result v128) local.get $x local.get $x
  i8x16.shuffle 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0)
 (func $send (param $x v128) (result v128) local.get $x throw $t)
 (func (export "_start") (local $got v128) (local $i i32) v128.const i32x4 0x7fa12345 0xdeadbeef 0x7ff00001 0x81234567 global.set $v
  loop $again global.get $v call $rotate call $rotate global.set $v
   block $hit (result v128) try_table (result v128) (catch $t $hit)
    global.get $v call $send unreachable end end local.set $got
   local.get $got v128.const i32x4 0x7fa12345 0xdeadbeef 0x7ff00001 0x81234567 v128.xor v128.any_true if unreachable end
   local.get $i i32.const 1 i32.add local.tee $i i32.const 8192 i32.lt_u br_if $again end))