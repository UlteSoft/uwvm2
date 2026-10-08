;; Native-next runtime fixture, not a hand-made executable-code substitute.
;; Real Core 3 return_call plus an unused nondefaultable GC reference local.
;; Host must explicitly enable tail-call and GC before its owning initializer.
(module
  (type $box (struct (field i32)))
  (memory 1 1)
  (func $callee (param i32) (result i32)
    local.get 0
    i32.const 13
    i32.add)
  (func $caller (param i32) (result i32)
    (local $uninitialized (ref $box))
    (local $numeric i32)
    (local $iterations i32)
    local.get 0
    i32.const 41
    i32.xor
    local.set $numeric
    ;; Real fixed-size Wasm memory accesses and a bounded observable loop give
    ;; the test actual opcode stops; no byte row is an execution selector.
    ;; Numeric keeps (17 xor 41)==56, so the final result remains 125.
    i32.const 32
    local.set $iterations
    loop $observe
      i32.const 0
      local.get $numeric
      i32.store
      i32.const 0
      i32.load
      local.set $numeric
      local.get $iterations
      i32.const 1
      i32.sub
      local.tee $iterations
      br_if $observe
    end
    local.get $numeric
    i32.const 1
    i32.rotl
    local.set $numeric
    local.get $numeric
    return_call $callee))
