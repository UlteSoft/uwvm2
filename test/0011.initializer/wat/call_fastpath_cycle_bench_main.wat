;; Benchmark workload for the self-cycle import fast path.
(module
  (type $unary_i32 (func (param i32) (result i32)))
  (import "call_fastpath_cycle_relay" "hot_alias" (func $hot_alias (type $unary_i32)))

  (func $hot (export "hot") (type $unary_i32)
    local.get 0
    i32.const 1
    i32.add)

  (func (export "_start")
    (local $i i32)
    (local $acc i32)

    i32.const 0
    local.set $i
    i32.const 0
    local.set $acc

    loop $loop
      local.get $acc
      call $hot_alias
      local.set $acc

      local.get $i
      i32.const 1
      i32.add
      local.tee $i
      i32.const 5000000
      i32.lt_u
      br_if $loop
    end

    local.get $acc
    i32.const 5000000
    i32.ne
    if
      unreachable
    end)
)
