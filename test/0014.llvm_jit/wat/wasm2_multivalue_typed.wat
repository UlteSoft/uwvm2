(module
  (type $pair (func (param i32 i32) (result i32 i32)))
  (type $pair_with_selector (func (param i32 i32 i32) (result i32 i32)))
  (type $ref_pair (func (result funcref externref)))
  (type $void (func))

  (table 1 funcref)
  (elem (i32.const 0) $return_pair)

  ;; A direct typed multi-result callee and an explicit tuple return.
  (func $return_pair (type $pair) (param $left i32) (param $right i32) (result i32 i32)
    local.get $left
    local.get $right
    return)

  ;; Reference results use the same typed tuple ABI and must not trigger fallback.
  (func $return_refs (type $ref_pair) (result funcref externref)
    ref.null func
    ref.null extern
    return)

  ;; A type-index block carries two parameters/results through an unconditional branch.
  (func $block_br (type $pair) (param $left i32) (param $right i32) (result i32 i32)
    local.get $left
    local.get $right
    block (type $pair)
      br 0
    end)

  ;; The loop label consumes two parameters; br_if feeds both values back to it.
  (func $loop_br_if (type $pair) (param $left i32) (param $right i32) (result i32 i32)
    (local $iteration i32)
    local.get $left
    local.get $right
    loop (type $pair)
      local.get $iteration
      i32.const 1
      i32.add
      local.tee $iteration
      i32.const 2
      i32.lt_u
      br_if 0
    end)

  ;; Both arms consume the type-index if parameters and produce a distinct tuple.
  (func $if_else (type $pair_with_selector)
      (param $left i32) (param $right i32) (param $condition i32) (result i32 i32)
    local.get $left
    local.get $right
    local.get $condition
    if (type $pair)
      local.set $right
      local.set $left
      local.get $left
      i32.const 10
      i32.add
      local.get $right
    else
      local.set $right
      local.set $left
      local.get $left
      i32.const 20
      i32.add
      local.get $right
    end)

  ;; Every br_table destination has the same two-value type-index signature.
  (func $br_table (type $pair_with_selector)
      (param $left i32) (param $right i32) (param $selector i32) (result i32 i32)
    local.get $left
    local.get $right
    block (type $pair)
      block (type $pair)
        local.get $selector
        br_table 0 1
      end
    end)

  (func $indirect_pair (type $pair) (param $left i32) (param $right i32) (result i32 i32)
    local.get $left
    local.get $right
    i32.const 0
    call_indirect (type $pair))

  ;; Successful execution is the semantic assertion: each tuple/path is checked and
  ;; any wrong value traps. The fixture is intentionally run in full LLVM-JIT mode once.
  (func $_start (export "_start") (type $void)
    (local $extern_is_null i32)
    i32.const 3
    i32.const 4
    call $return_pair
    i32.add
    i32.const 7
    i32.ne
    if unreachable end

    call $return_refs
    ref.is_null
    local.set $extern_is_null
    ref.is_null
    local.get $extern_is_null
    i32.and
    i32.eqz
    if unreachable end

    i32.const 5
    i32.const 6
    call $block_br
    i32.add
    i32.const 11
    i32.ne
    if unreachable end

    i32.const 7
    i32.const 8
    call $loop_br_if
    i32.add
    i32.const 15
    i32.ne
    if unreachable end

    i32.const 1
    i32.const 2
    i32.const 1
    call $if_else
    i32.add
    i32.const 13
    i32.ne
    if unreachable end

    i32.const 1
    i32.const 2
    i32.const 0
    call $if_else
    i32.add
    i32.const 23
    i32.ne
    if unreachable end

    i32.const 9
    i32.const 10
    i32.const 0
    call $br_table
    i32.add
    i32.const 19
    i32.ne
    if unreachable end

    i32.const 11
    i32.const 12
    i32.const 1
    call $br_table
    i32.add
    i32.const 23
    i32.ne
    if unreachable end

    i32.const 13
    i32.const 14
    call $indirect_pair
    i32.add
    i32.const 27
    i32.ne
    if unreachable end))
