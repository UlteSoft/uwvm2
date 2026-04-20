;; LLVM JIT smoke module that exercises control flow, locals, integer ops,
;; and linear-memory load/store from a `_start` entrypoint.
(module
  (memory 1)

  (func $main (export "_start")
    (local $x i32)

    i32.const 0
    local.set $x

    block
      loop
        local.get $x
        i32.const 5
        i32.lt_s
        i32.eqz
        br_if 1

        local.get $x
        i32.const 1
        i32.add
        local.set $x
        br 0
      end
    end

    local.get $x
    i32.const 5
    i32.ne
    if
      unreachable
    end

    i32.const 0
    i32.const 42
    i32.store

    i32.const 0
    i32.load
    i32.const 42
    i32.ne
    if
      unreachable
    end))
