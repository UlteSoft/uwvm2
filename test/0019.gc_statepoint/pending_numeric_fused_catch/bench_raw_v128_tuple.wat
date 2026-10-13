(module
  ;; Current actual pending-plan schema refuses v128. Keep this SAME byte
  ;; module as a native fallback/SIMD semantic control, not a fused positive.
  (tag $tuple (param i32 i64 f32 f64 v128))
  (global $visits (export "visits") (mut i64) (i64.const 0))
  (global $throws (export "throws") (mut i64) (i64.const 0))
  (global $catches (export "catches") (mut i64) (i64.const 0))
  (global $last (export "last_checksum") (mut i32) (i32.const 0))
  (func $step (param $x i32) (result i32)
    local.get $x i32.const 1664525 i32.mul
    i32.const 1013904223 i32.add)
  (func $vector (param $x i32) (result v128)
    local.get $x i32x4.splat
    local.get $x i32.const -1 i32.xor i32x4.replace_lane 1
    local.get $x i32.const 7 i32.rotl i32x4.replace_lane 2
    local.get $x i32.const 0xa5a5a5a5 i32.xor i32x4.replace_lane 3)
  (func $throw_tuple (param $x i32) (result i32 i64 f32 f64 v128)
    (local $next i32)
    global.get $visits i64.const 1 i64.add global.set $visits
    global.get $throws i64.const 1 i64.add global.set $throws
    local.get $x call $step local.set $next
    local.get $next
    local.get $next i64.extend_i32_u i64.const 0xfedcba9876543210 i64.xor
    local.get $next i32.const 0x003fffff i32.and
      i32.const 0x7f800001 i32.or f32.reinterpret_i32
    local.get $next i64.extend_i32_u
      i64.const 0x7ff0000000000001 i64.or f64.reinterpret_i64
    local.get $next call $vector
    throw $tuple)
  (func $catch_tuple (param $x i32) (result i32)
    (local $a i32) (local $b i64) (local $c f32) (local $d f64) (local $v v128)
    block $hit (result i32 i64 f32 f64 v128)
      try_table (result i32 i64 f32 f64 v128) (catch $tuple $hit)
        local.get $x call $throw_tuple
        ;; A required throw must never return normally into the catch result.
        unreachable
      end
    end
    local.set $v local.set $d local.set $c local.set $b local.set $a
    global.get $catches i64.const 1 i64.add global.set $catches
    local.get $a local.get $x call $step i32.ne if unreachable end
    local.get $b local.get $a i64.extend_i32_u
      i64.const 0xfedcba9876543210 i64.xor i64.ne if unreachable end
    local.get $c i32.reinterpret_f32 local.get $a
      i32.const 0x003fffff i32.and i32.const 0x7f800001 i32.or
      i32.ne if unreachable end
    local.get $d i64.reinterpret_f64 local.get $a i64.extend_i32_u
      i64.const 0x7ff0000000000001 i64.or i64.ne if unreachable end
    local.get $v local.get $a call $vector i8x16.eq i8x16.all_true
      i32.eqz if unreachable end
    local.get $a)
  (func $run (export "run")
        (param $n i64) (param $seed i32) (param $expected i32)
    (local $left i64) (local $state i32)
    local.get $n i64.const 0 i64.lt_s if unreachable end
    i64.const 0 global.set $visits
    i64.const 0 global.set $throws
    i64.const 0 global.set $catches
    local.get $seed local.set $state local.get $n local.set $left
    block $done
      loop $again
        local.get $left i64.eqz br_if $done
        local.get $state call $catch_tuple local.set $state
        local.get $left i64.const 1 i64.sub local.set $left
        br $again
      end
    end
    local.get $state global.set $last
    global.get $last local.get $expected i32.ne if unreachable end
    global.get $visits local.get $n i64.ne if unreachable end
    global.get $throws local.get $n i64.ne if unreachable end
    global.get $catches local.get $n i64.ne if unreachable end)
  (func $checksum (export "checksum") (result i32) global.get $last)
  (func $_start (export "_start")
    i64.const 0 i32.const 17 i32.const 17 call $run
    i64.const 1 i32.const 0 i32.const 1013904223 call $run)
)
