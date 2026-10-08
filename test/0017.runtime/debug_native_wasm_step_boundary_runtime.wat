;; Native-next runtime fixture, not a hand-made executable-code substitute.
;; Real Core 3 return_call plus an unused nondefaultable GC reference local.
;; Host must explicitly enable tail-call and GC before its owning initializer.
(module
  (memory 1)
  (data (i32.const 0) "WASMJIT!")
  (type $box (struct (field i32)))
  (global $i64 (mut i64) (i64.const 0))
  (global $f32 (mut f32) (f32.const 0))
  (global $f64 (mut f64) (f64.const 0))
  (global $v128 (mut v128) (v128.const i32x4 0 0 0 0))
  (func $callee (param i32) (result i32)
    local.get 0
    i32.const 13
    i32.add)
  (func $caller (param i32) (result i32)
    (local $uninitialized (ref $box))
    (local $numeric i32)
    ;; Observable dynamic arithmetic supplies a genuine public-code control.
    ;; A source-marked instruction with a host memory operand stays hidden.
    local.get 0
    i64.extend_i32_u
    i64.const 12345678901234567
    i64.add
    global.set $i64
    local.get 0
    f32.convert_i32_u
    f32.const 1.5
    f32.mul
    global.set $f32
    local.get 0
    f64.convert_i32_u
    f64.const 2.25
    f64.mul
    global.set $f64
    local.get 0
    i32x4.splat
    v128.const i32x4 1 2 3 4
    i32x4.add
    global.set $v128
    local.get 0
    i32.const 41
    i32.xor
    local.set $numeric
    local.get $numeric
    i32.const 1
    i32.rotl
    local.set $numeric
    local.get 0
    i32.const 0
    i32.lt_s
    if
      unreachable
    end
    local.get $numeric
    return_call $callee)
  (func $entry (param i32) (result i32)
    i32.const 0
    i64.const 0x2154494a4d534157
    i64.store
    local.get 0
    call $caller))
