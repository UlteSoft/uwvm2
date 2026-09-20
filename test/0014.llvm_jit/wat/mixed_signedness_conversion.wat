;; Each call must preserve the integer bit pattern between independently
;; signed/unsigned Wasm instructions. No inline policy may erase this boundary.
(module
 (func $f32_i32_su (param f32) (result f32) local.get 0 i32.trunc_f32_s f32.convert_i32_u)
 (func $f64_i32_su (param f64) (result f64) local.get 0 i32.trunc_f64_s f64.convert_i32_u)
 (func $f32_i64_su (param f32) (result f32) local.get 0 i64.trunc_f32_s f32.convert_i64_u)
 (func $f64_i64_su (param f64) (result f64) local.get 0 i64.trunc_f64_s f64.convert_i64_u)
 (func $f32_i32_us (param f32) (result f32) local.get 0 i32.trunc_f32_u f32.convert_i32_s)
 (func $f64_i32_us (param f64) (result f64) local.get 0 i32.trunc_f64_u f64.convert_i32_s)
 (func $f32_i64_us (param f32) (result f32) local.get 0 i64.trunc_f32_u f32.convert_i64_s)
 (func $f64_i64_us (param f64) (result f64) local.get 0 i64.trunc_f64_u f64.convert_i64_s)
 (func (export "_start")
  f32.const -1.5 call $f32_i32_su i32.reinterpret_f32 i32.const 0x4f800000 i32.ne if unreachable end
  f64.const -1.5 call $f64_i32_su i64.reinterpret_f64 i64.const 0x41efffffffe00000 i64.ne if unreachable end
  f32.const -1.5 call $f32_i64_su i32.reinterpret_f32 i32.const 0x5f800000 i32.ne if unreachable end
  f64.const -1.5 call $f64_i64_su i64.reinterpret_f64 i64.const 0x43f0000000000000 i64.ne if unreachable end
  f32.const 2147483648 call $f32_i32_us i32.reinterpret_f32 i32.const 0xcf000000 i32.ne if unreachable end
  f64.const 2147483648 call $f64_i32_us i64.reinterpret_f64 i64.const 0xc1e0000000000000 i64.ne if unreachable end
  f32.const 9223372036854775808 call $f32_i64_us i32.reinterpret_f32 i32.const 0xdf000000 i32.ne if unreachable end
  f64.const 9223372036854775808 call $f64_i64_us i64.reinterpret_f64 i64.const 0xc3e0000000000000 i64.ne if unreachable end))
