;; Exercise both normal and exceptional joins with independent integer and FP rings.
(module
  (tag $integer_event (param i64))
  (tag $float_event (param f64))
  (func $integer_step (param $raise i32) (result i64)
    local.get $raise
    if
      i64.const 42
      throw $integer_event
    end
    i64.const 42)
  (func $float_step (param $raise i32) (result f64)
    local.get $raise
    if
      f64.const 42
      throw $float_event
    end
    f64.const 42)
  (func $integer_join (param $raise i32) (result i64)
    block $after (result i64)
      block $caught (result i64)
        try_table (result i64) (catch $integer_event $caught)
          local.get $raise
          call $integer_step
        end
        br $after
      end
      i64.const 1
      i64.add
      i64.const 1
      i64.sub
    end)
  (func $float_join (param $raise i32) (result f64)
    block $after (result f64)
      block $caught (result f64)
        try_table (result f64) (catch $float_event $caught)
          local.get $raise
          call $float_step
        end
        br $after
      end
      f64.const 1
      f64.add
      f64.const 1
      f64.sub
    end)
  (func (export "_start")
    i32.const 0
    call $integer_join
    i64.const 42
    i64.ne
    if unreachable end
    i32.const 1
    call $integer_join
    i64.const 42
    i64.ne
    if unreachable end
    i32.const 0
    call $float_join
    f64.const 42
    f64.ne
    if unreachable end
    i32.const 1
    call $float_join
    f64.const 42
    f64.ne
    if unreachable end))
