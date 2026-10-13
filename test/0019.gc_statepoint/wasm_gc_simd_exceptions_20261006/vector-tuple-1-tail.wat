(module (tag $t (param v128)) (tag $other (param v128))
   (global $caught (mut i32) (i32.const 0)) (func $send (param $x v128) (result i32) local.get $x  throw $t) (func $tail (param $x v128) (result i32) local.get $x return_call $send)
   (func $one (result i32) (local $v0 v128) block $done (result i32)
     block $hit (result v128) try_table (result v128) (catch $t $hit)
       v128.const i32x4 0x7fa12345 0xdeadbeef 0x7ff00001 0x81234567 call $tail drop unreachable end end
     global.get $caught i32.const 1 i32.add global.set $caught
     local.set $v0 local.get $v0 v128.const i32x4 0x7fa12345 0xdeadbeef 0x7ff00001 0x81234567 v128.xor v128.any_true if unreachable end i32.const 9837 end)
   (func (export "_start") (local $i i32) i32.const 0 global.set $caught
    loop $again call $one i32.const 9837 i32.ne if unreachable end
     local.get $i i32.const 1 i32.add local.tee $i i32.const 32 i32.lt_u br_if $again end
     global.get $caught i32.const 32 i32.ne if unreachable end))