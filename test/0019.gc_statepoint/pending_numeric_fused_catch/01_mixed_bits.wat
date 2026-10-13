(module
  (tag $mixed (param i32 i64 f32 f64))
  (func $throw_mixed (result i32 i64 f32 f64)
    i32.const 0xfedcba98 i64.const 0xfedcba9876543210
    i32.const 0x7fa12345 f32.reinterpret_i32
    i64.const 0x7ff123456789abcd f64.reinterpret_i64
    throw $mixed)
  (func $caught (result i32 i64 f32 f64)
    (block $hit (result i32 i64 f32 f64)
      (try_table (result i32 i64 f32 f64) (catch $mixed $hit)
        call $throw_mixed)))
  (func (export "caught_bits") (result i32 i64 i32 i64)
    (local $a i32) (local $b i64) (local $c f32) (local $d f64)
    call $caught
    local.set $d local.set $c local.set $b local.set $a
    local.get $a local.get $b
    local.get $c i32.reinterpret_f32
    local.get $d i64.reinterpret_f64)
  (func (export "run") (result i32)
    (local $a i32) (local $b i64) (local $c f32) (local $d f64)
    call $caught
    local.set $d local.set $c local.set $b local.set $a
    local.get $a i32.const 0xfedcba98 i32.eq
    local.get $b i64.const 0xfedcba9876543210 i64.eq i32.and
    local.get $c i32.reinterpret_f32 i32.const 0x7fa12345 i32.eq i32.and
    local.get $d i64.reinterpret_f64 i64.const 0x7ff123456789abcd i64.eq i32.and)
)
