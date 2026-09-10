(module $tiered_cold_consumer
  (import "P" "answer" (func $answer (param i32) (result i32)))
  ;; The original Tiered small-hot-loop heuristic compiles this function on
  ;; the calling thread: <=8 local functions, 96..640 source bytes, a loop,
  ;; and no FP kernel shape. Nops pad only the original Wasm code size; LLVM
  ;; removes them. T0 remains enabled; this is not the no-T0 demand path.
  (func $dispatch (param $call_provider i32) (result i32) (local $n i32)
    (loop $one_iteration
      local.get $n
      i32.const 1
      i32.add
      local.tee $n
      i32.const 1
      i32.lt_u
      br_if $one_iteration)
    nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop
    nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop
    nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop
    nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop
    nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop
    nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop
    nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop
    nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop
    local.get $call_provider
    if (result i32)
      i32.const 41
      call $answer
    else
      i32.const 42
    end)
  (func $_start (export "_start")
    ;; First complete a native caller invocation without calling the provider.
    i32.const 0
    call $dispatch
    i32.const 42
    i32.ne
    if unreachable end
    ;; Its next invocation calls a still-cold function in another module.
    i32.const 1
    call $dispatch
    i32.const 42
    i32.ne
    if unreachable end))
