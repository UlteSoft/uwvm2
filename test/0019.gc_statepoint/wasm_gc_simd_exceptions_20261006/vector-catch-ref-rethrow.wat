(module (tag $t (param v128))
  (func (export "_start") (local $e (ref null exn))
   block $hit (result v128 (ref exn)) try_table (catch_ref $t $hit)
    v128.const i32x4 0x7fa12345 0xdeadbeef 0x7ff00001 0x81234567 throw $t end unreachable end local.set $e drop
   block $caught (result v128) try_table (result v128) (catch $t $caught)
     local.get $e ref.as_non_null throw_ref end unreachable end
   v128.const i32x4 0x7fa12345 0xdeadbeef 0x7ff00001 0x81234567 v128.xor v128.any_true if unreachable end))