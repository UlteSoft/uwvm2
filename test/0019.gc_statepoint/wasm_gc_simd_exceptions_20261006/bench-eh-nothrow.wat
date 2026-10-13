(module
  ;; Runtime i64 iteration count and i32 seed; no WASI/import/GC payload.
  ;; The caller supplies an independently computed endpoint. Every entry traps
  ;; on a bad endpoint, missing visit, or wrong throw/catch count.
  (tag $event (param i32))
  (tag $raw (param i32 i64 f32 f64))
  (global $visits (export "visits") (mut i64) (i64.const 0))
  (global $throws (export "throws") (mut i64) (i64.const 0))
  (global $catches (export "catches") (mut i64) (i64.const 0))
  (global $last (export "last_checksum") (mut i32) (i32.const 0))
  (func $step (param $x i32) (result i32)
    local.get $x i32.const 1664525 i32.mul
    i32.const 1013904223 i32.add)
  (func $event_leaf (param $x i32) (param $raise i32) (result i32)
    (local $next i32)
    global.get $visits i64.const 1 i64.add global.set $visits
    local.get $x call $step local.set $next
    local.get $raise
    if
      global.get $throws i64.const 1 i64.add global.set $throws
      local.get $next throw $event
    end
    local.get $next)
  (func $relay (param $x i32) (result i32)
    local.get $x i32.const 1 call $event_leaf)
  (func $reset (param $seed i32)
    i64.const 0 global.set $visits
    i64.const 0 global.set $throws
    i64.const 0 global.set $catches
    local.get $seed global.set $last)
  (func $finish (param $n i64) (param $expected i32)
                (param $expected_throws i64) (param $expected_catches i64)
    global.get $last local.get $expected i32.ne if unreachable end
    global.get $visits local.get $n i64.ne if unreachable end
    global.get $throws local.get $expected_throws i64.ne if unreachable end
    global.get $catches local.get $expected_catches i64.ne if unreachable end)
  (func $bench_plain (export "bench_plain")
        (param $n i64) (param $seed i32) (param $expected i32)
    (local $left i64) (local $state i32)
    local.get $n i64.const 0 i64.lt_s if unreachable end
    local.get $seed call $reset
    local.get $seed local.set $state local.get $n local.set $left
    block $done
      loop $again
        local.get $left i64.eqz br_if $done
        local.get $state i32.const 0 call $event_leaf local.set $state
        local.get $left i64.const 1 i64.sub local.set $left
        br $again
      end
    end
    local.get $state global.set $last
    local.get $n local.get $expected i64.const 0 i64.const 0 call $finish)
  (func $bench_nothrow (export "bench_nothrow")
        (param $n i64) (param $seed i32) (param $expected i32)
    (local $left i64) (local $state i32)
    local.get $n i64.const 0 i64.lt_s if unreachable end
    local.get $seed call $reset
    local.get $seed local.set $state local.get $n local.set $left
    block $done
      loop $again
        local.get $left i64.eqz br_if $done
        block $out (result i32)
          block $hit (result i32)
            try_table (result i32) (catch $event $hit)
              local.get $state i32.const 0 call $event_leaf
            end
            br $out
          end
          global.get $catches i64.const 1 i64.add global.set $catches
        end
        local.set $state
        local.get $left i64.const 1 i64.sub local.set $left
        br $again
      end
    end
    local.get $state global.set $last
    local.get $n local.get $expected i64.const 0 i64.const 0 call $finish)
  (func $bench_same_tag (export "bench_same_tag") (export "run")
        (param $n i64) (param $seed i32) (param $expected i32)
    (local $left i64) (local $state i32)
    local.get $n i64.const 0 i64.lt_s if unreachable end
    local.get $seed call $reset
    local.get $seed local.set $state local.get $n local.set $left
    block $done
      loop $again
        local.get $left i64.eqz br_if $done
        block $out (result i32)
          block $hit (result i32)
            try_table (result i32) (catch $event $hit)
              local.get $state i32.const 1 call $event_leaf
            end
            br $out
          end
          global.get $catches i64.const 1 i64.add global.set $catches
        end
        local.set $state
        local.get $left i64.const 1 i64.sub local.set $left
        br $again
      end
    end
    local.get $state global.set $last
    local.get $n local.get $expected local.get $n local.get $n call $finish)
  (func $bench_cross (export "bench_cross")
        (param $n i64) (param $seed i32) (param $expected i32)
    (local $left i64) (local $state i32)
    local.get $n i64.const 0 i64.lt_s if unreachable end
    local.get $seed call $reset
    local.get $seed local.set $state local.get $n local.set $left
    block $done
      loop $again
        local.get $left i64.eqz br_if $done
        block $out (result i32)
          block $hit (result i32)
            try_table (result i32) (catch $event $hit)
              local.get $state call $relay
            end
            br $out
          end
          global.get $catches i64.const 1 i64.add global.set $catches
        end
        local.set $state
        local.get $left i64.const 1 i64.sub local.set $left
        br $again
      end
    end
    local.get $state global.set $last
    local.get $n local.get $expected local.get $n local.get $n call $finish)
  (func $throw_raw (param $x i32) (result i32 i64 f32 f64)
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
    throw $raw)
  (func $catch_raw (param $x i32) (result i32)
    (local $a i32) (local $b i64) (local $c f32) (local $d f64)
    block $hit (result i32 i64 f32 f64)
      try_table (result i32 i64 f32 f64) (catch $raw $hit)
        local.get $x call $throw_raw
        ;; A required throw must never return normally into the catch result.
        unreachable
      end
    end
    local.set $d local.set $c local.set $b local.set $a
    global.get $catches i64.const 1 i64.add global.set $catches
    local.get $a local.get $x call $step i32.ne if unreachable end
    local.get $b local.get $a i64.extend_i32_u
      i64.const 0xfedcba9876543210 i64.xor i64.ne if unreachable end
    local.get $c i32.reinterpret_f32
      local.get $a i32.const 0x003fffff i32.and
      i32.const 0x7f800001 i32.or i32.ne if unreachable end
    local.get $d i64.reinterpret_f64 local.get $a i64.extend_i32_u
      i64.const 0x7ff0000000000001 i64.or i64.ne if unreachable end
    local.get $a)
  (func $bench_raw_nan (export "bench_raw_nan")
        (param $n i64) (param $seed i32) (param $expected i32)
    (local $left i64) (local $state i32)
    local.get $n i64.const 0 i64.lt_s if unreachable end
    local.get $seed call $reset
    local.get $seed local.set $state local.get $n local.set $left
    block $done
      loop $again
        local.get $left i64.eqz br_if $done
        local.get $state call $catch_raw local.set $state
        local.get $left i64.const 1 i64.sub local.set $left
        br $again
      end
    end
    local.get $state global.set $last
    local.get $n local.get $expected local.get $n local.get $n call $finish)
  (func $checksum (export "checksum") (result i32) global.get $last)
  (func $_start (export "_start") i64.const 8192 i32.const 17 i32.const 244654097 call $bench_nothrow)
)
