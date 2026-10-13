(module
    (type $n (struct (field (mut (ref null $n))) (field i64)))
    (type $a (array (mut (ref null eq))))
    (global $root (mut (ref null $a)) (ref.null $a))
    (func (export "_start") (local $left (ref null $n)) (local $right (ref null $n))
      (local $array (ref null $a)) (local $ref (ref null eq)) (local $i i32)
      ref.null $n i64.const 111 struct.new $n local.set $left
      ref.null $n i64.const 222 struct.new $n local.set $right
      local.get $left local.get $right struct.set $n 0
      local.get $right local.get $left struct.set $n 0
      i32.const 16 array.new_default $a local.set $array i32.const 0 local.set $i loop $fill
      local.get $array local.get $i local.get $i i32.const 1 i32.and if (result (ref null eq)) i32.const -1 ref.i31 else ref.null eq end array.set $a
      local.get $i i32.const 1 i32.add local.tee $i i32.const 16 i32.lt_u br_if $fill end local.get $array i32.const 0 local.get $left array.set $a
      local.get $array global.set $root
      ref.null $n local.set $left ref.null $n local.set $right ref.null $a local.set $array
      i32.const 0 local.set $i loop $churn ref.null $n i64.const 991 struct.new $n drop
        local.get $i i32.const 1 i32.add local.tee $i i32.const 8192 i32.lt_u br_if $churn end
      i32.const 0 local.set $i loop $verify
        global.get $root local.get $i array.get $a local.set $ref local.get $i i32.const 0 i32.eq if local.get $ref ref.cast (ref $n) local.tee $left struct.get $n 1 i64.const 111 i64.ne if unreachable end
       local.get $left struct.get $n 0 local.tee $right struct.get $n 1 i64.const 222 i64.ne if unreachable end
       local.get $right struct.get $n 0 local.get $left ref.eq i32.eqz if unreachable end else local.get $i i32.const 1 i32.and if local.get $ref ref.cast (ref i31) i31.get_s i32.const -1 i32.ne if unreachable end else local.get $ref ref.is_null i32.eqz if unreachable end end end
        local.get $i i32.const 1 i32.add local.tee $i i32.const 16 i32.lt_u br_if $verify end
      ref.null $a global.set $root))