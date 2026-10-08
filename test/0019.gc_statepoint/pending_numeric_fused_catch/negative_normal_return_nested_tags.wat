(module
  ;; Distinct INSTANCE identities with the same payload shape. LCG low bit
  ;; flips on each step, so seed and runtime N choose both nested catch routes.
  (tag $a (param i32))
  (tag $b (param i32))
  (global $issued_a (export "issued_a") (mut i64) (i64.const 0))
  (global $issued_b (export "issued_b") (mut i64) (i64.const 0))
  (global $caught_a (export "caught_a") (mut i64) (i64.const 0))
  (global $caught_b (export "caught_b") (mut i64) (i64.const 0))
  (global $last (export "last_checksum") (mut i32) (i32.const 0))
  (func $step (param $x i32) (result i32)
    local.get $x i32.const 1664525 i32.mul
    i32.const 1013904223 i32.add)
  (func $select_throw (param $x i32) (result i32)
    (local $next i32)
    local.get $x call $step local.set $next
    local.get $x i32.const 1 i32.and
    if
      global.get $issued_b i64.const 1 i64.add global.set $issued_b
      ;; Negative control: preserve issued_b and return the original payload.
      local.get $next return
    else
      global.get $issued_a i64.const 1 i64.add global.set $issued_a
      ;; Negative control: preserve issued_a and return the original payload.
      local.get $next return
    end
    unreachable)
  (func $nested (param $x i32) (result i32)
    block $done (result i32)
      block $b_hit (result i32)
        try_table (result i32) (catch $b $b_hit)
          block $a_hit (result i32)
            try_table (result i32) (catch $a $a_hit)
              local.get $x call $select_throw
              ;; Only the actual matching catch may produce this result.
              unreachable
            end
          end
          global.get $caught_a i64.const 1 i64.add global.set $caught_a
          br $done
        end
      end
      global.get $caught_b i64.const 1 i64.add global.set $caught_b
    end)
  (func $run (export "run")
        (param $n i64) (param $seed i32) (param $expected i32)
        (param $expected_a i64) (param $expected_b i64)
    (local $state i32) (local $left i64)
    local.get $n i64.const 0 i64.lt_s if unreachable end
    i64.const 0 global.set $issued_a i64.const 0 global.set $issued_b
    i64.const 0 global.set $caught_a i64.const 0 global.set $caught_b
    local.get $seed local.set $state local.get $n local.set $left
    block $done
      loop $again
        local.get $left i64.eqz br_if $done
        local.get $state call $nested local.set $state
        local.get $left i64.const 1 i64.sub local.set $left
        br $again
      end
    end
    local.get $state global.set $last
    global.get $last local.get $expected i32.ne if unreachable end
    global.get $issued_a local.get $expected_a i64.ne if unreachable end
    global.get $caught_a local.get $expected_a i64.ne if unreachable end
    global.get $issued_b local.get $expected_b i64.ne if unreachable end
    global.get $caught_b local.get $expected_b i64.ne if unreachable end
    global.get $issued_a global.get $issued_b i64.add
      local.get $n i64.ne if unreachable end)
  (func $checksum (export "checksum") (result i32) global.get $last)
  (func $_start (export "_start")
    i64.const 0 i32.const 17 i32.const 17 i64.const 0 i64.const 0 call $run
    i64.const 1 i32.const 0 i32.const 1013904223 i64.const 1 i64.const 0 call $run)
)
