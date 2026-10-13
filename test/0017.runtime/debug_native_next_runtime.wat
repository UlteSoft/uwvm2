;; Native-next runtime fixture, not a hand-made executable-code substitute.
;; Real Core 3 return_call plus an unused nondefaultable GC reference local.
;; Host must explicitly enable tail-call and GC before its owning initializer.
(module
  (type $box (struct (field i32)))
  (func $callee (param i32) (result i32)
    local.get 0
    i32.const 13
    i32.add)
  (func $caller (param i32) (result i32)
    (local $uninitialized (ref $box))
    (local $numeric i32)
    local.get 0
    i32.const 41
    i32.xor
    local.set $numeric
    local.get $numeric
    i32.const 1
    i32.rotl
    local.set $numeric
    ;; The negative input traps: this input-dependent edge cannot be replaced
    ;; by an arithmetic select while preserving Wasm trap semantics. The actual
    ;; positive case MUST execute a real conditional NI witness in the harness;
    ;; DATA decoding or an ordinary ADD cannot satisfy that required count.
    local.get 0
    i32.const 0
    i32.lt_s
    if
      unreachable
    end
    local.get $numeric
    return_call $callee))
