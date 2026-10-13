(module (memory 1) (tag $t (param v128))
 (func $send (result i32) i32.const 1 v128.const i32x4 0x7fa12345 0xdeadbeef 0x7ff00001 0x81234567 v128.store align=1
  i32.const 1 v128.load align=1 throw $t)
 (func (export "_start") (local $i i32) loop $again
  block $hit (result v128) try_table (result v128) (catch $t $hit) call $send drop unreachable end end
  v128.const i32x4 0x7fa12345 0xdeadbeef 0x7ff00001 0x81234567 v128.xor v128.any_true if unreachable end
  local.get $i i32.const 1 i32.add local.tee $i i32.const 257 i32.lt_u br_if $again end))