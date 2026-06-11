(module
  (type $v (func))
  (type $i (func (param i32)))
  (memory 1)
  (table 1 funcref)
  (elem (i32.const 0) $leaf_trap)

  (func $leaf_trap (type $i) (param $p i32)
    local.get $p
    i32.load
    drop)

  (func $loop_then_indirect_trap (type $i) (param $p i32)
    (local $i i32)
    ;; UWVM_TIERED_OSR_PAD_NOPS
    i32.const 0
    local.set $i

    block $exit
      loop $hot
        local.get $i
        i32.const 350000
        i32.ge_u
        br_if $exit

        local.get $i
        i32.const 1
        i32.add
        local.set $i
        br $hot
      end
    end

    local.get $p
    i32.const 0
    call_indirect (type $i))

  (func $_start (export "_start") (type $v)
    i32.const -1
    call $loop_then_indirect_trap))
