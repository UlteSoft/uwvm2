(module
   (type $node (struct (field (mut i32))))
   (type $payload (array (mut v128)))
   (type $roots (array (mut (ref null $payload))))
   (func (export "_start") (local $roots (ref null $roots)) (local $p (ref null $payload)) (local $j i32) (local $k i32)
   
    i32.const 16 array.new_default $roots local.set $roots
    i32.const 0 local.set $j
    loop $allocate
      local.get $j i32x4.splat i32.const 4096 array.new $payload local.set $p
      local.get $roots local.get $j i32.const 15 i32.and local.get $p array.set $roots
      local.get $j i32.const 1 i32.add local.tee $j i32.const 4096 i32.lt_u br_if $allocate
    end
    i32.const 0 local.set $k
    loop $verify
      local.get $roots local.get $k array.get $roots ref.as_non_null local.tee $p
      i32.const 0 array.get $payload i32x4.extract_lane 3 i64.extend_i32_u
      i64.const 4080 local.get $k i64.extend_i32_u i64.add i64.ne if unreachable end
      local.get $p i32.const 4095 array.get $payload i32x4.extract_lane 3 i64.extend_i32_u
      i64.const 4080 local.get $k i64.extend_i32_u i64.add i64.ne if unreachable end
      local.get $k i32.const 1 i32.add local.tee $k i32.const 16 i32.lt_u br_if $verify
    end

    i32.const 256 array.new_default $roots local.set $roots
    i32.const 0 local.set $j
    loop $allocate
      local.get $j i32x4.splat i32.const 4096 array.new $payload local.set $p
      local.get $roots local.get $j i32.const 255 i32.and local.get $p array.set $roots
      local.get $j i32.const 1 i32.add local.tee $j i32.const 4096 i32.lt_u br_if $allocate
    end
    i32.const 0 local.set $k
    loop $verify
      local.get $roots local.get $k array.get $roots ref.as_non_null local.tee $p
      i32.const 0 array.get $payload i32x4.extract_lane 3 i64.extend_i32_u
      i64.const 3840 local.get $k i64.extend_i32_u i64.add i64.ne if unreachable end
      local.get $p i32.const 4095 array.get $payload i32x4.extract_lane 3 i64.extend_i32_u
      i64.const 3840 local.get $k i64.extend_i32_u i64.add i64.ne if unreachable end
      local.get $k i32.const 1 i32.add local.tee $k i32.const 256 i32.lt_u br_if $verify
    end

    i32.const 16 array.new_default $roots local.set $roots
    i32.const 0 local.set $j
    loop $allocate
      local.get $j i32x4.splat i32.const 4096 array.new $payload local.set $p
      local.get $roots local.get $j i32.const 15 i32.and local.get $p array.set $roots
      local.get $j i32.const 1 i32.add local.tee $j i32.const 4096 i32.lt_u br_if $allocate
    end
    i32.const 0 local.set $k
    loop $verify
      local.get $roots local.get $k array.get $roots ref.as_non_null local.tee $p
      i32.const 0 array.get $payload i32x4.extract_lane 3 i64.extend_i32_u
      i64.const 4080 local.get $k i64.extend_i32_u i64.add i64.ne if unreachable end
      local.get $p i32.const 4095 array.get $payload i32x4.extract_lane 3 i64.extend_i32_u
      i64.const 4080 local.get $k i64.extend_i32_u i64.add i64.ne if unreachable end
      local.get $k i32.const 1 i32.add local.tee $k i32.const 16 i32.lt_u br_if $verify
    end))