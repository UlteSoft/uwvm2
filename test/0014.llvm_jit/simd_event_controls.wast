(module
  (memory 1)
  (func $lane (param v128) (result i32)
    local.get 0 i32.const -2147483648 i32x4.replace_lane 3 i32x4.extract_lane 3)
  (func $shuffle (param v128 v128) (result i32)
    local.get 0 local.get 1 i8x16.shuffle 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16
    i8x16.extract_lane_u 0)
  (func $memory (param v128) (result i32)
    i32.const 0 local.get 0 v128.store
    i32.const 0 v128.load i32x4.extract_lane 0)
  (func $memorylane (param v128) (result i32)
    i32.const 64 i32.const 255 i32.store8
    i32.const 64 local.get 0 v128.load8_lane 15
    i8x16.extract_lane_u 15)
  (func $math (param v128) (result i32)
    local.get 0 local.get 0 v128.xor v128.any_true)
  (func $run (export "run") (result i32)
    v128.const i32x4 17 0 0 0 call $memory i32.const 17 i32.ne if unreachable end
    v128.const i32x4 0 0 0 0 call $memorylane i32.const 255 i32.ne if unreachable end
    v128.const i32x4 0 0 0 0 call $lane i32.const -2147483648 i32.ne if unreachable end
    v128.const i32x4 0 0 0 0 v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
    call $shuffle i32.const 15 i32.ne if unreachable end
    v128.const i32x4 -1 0 1 2 call $math if unreachable end
    i32.const 17)
  (func (export "_start") call $run i32.const 17 i32.ne if unreachable end))
(assert_return (invoke "run") (i32.const 17))
(assert_return (invoke "_start"))

(module
  (memory i64 1)
  (func (export "_start")
    i64.const 0 v128.const i32x4 -1 2 3 4 v128.store
    i64.const 0 v128.load i32x4.extract_lane 0 i32.const -1 i32.ne if unreachable end))
(assert_return (invoke "_start"))

(module
  (memory 1) (memory 1)
  (func (export "_start")
    i32.const 0 v128.const i32x4 99 0 0 0 v128.store 1
    i32.const 0 v128.load 1 i32x4.extract_lane 0 i32.const 99 i32.ne if unreachable end))
(assert_return (invoke "_start"))

(module
  (func (export "_start")
    unreachable
    v128.const i32x4 1 2 3 4 i32.const 2 i32x4.shl drop))
(assert_trap (invoke "_start") "unreachable")

(assert_invalid (module (func v128.const i32x4 0 0 0 0 i8x16.extract_lane_u 16 drop)) "invalid lane index")
(assert_invalid (module (func v128.const i32x4 0 0 0 0 v128.const i32x4 0 0 0 0
  i8x16.shuffle 32 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 drop)) "invalid lane index")
(assert_invalid (module (memory 1) (func i32.const 0 v128.load align=32 drop)) "alignment must not be larger than natural")
(assert_invalid (module (memory i64 1) (func i32.const 0 v128.load drop)) "type mismatch")
(assert_invalid (module (func i32.const 0 v128.not drop)) "type mismatch")
(assert_invalid (module (func v128.const i32x4 0 0 0 0 f32.const 1 i32x4.replace_lane 0 drop)) "type mismatch")

(module
  (func (export "_start")
    v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
    v128.const i8x16 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
    i8x16.relaxed_swizzle i8x16.extract_lane_u 15 i32.const 15 i32.ne if unreachable end
    v128.const i32x4 2139095041 -1 0 0 i32x4.extract_lane 0
    i32.const 2139095041 i32.ne if unreachable end))
(assert_return (invoke "_start"))
