;; Real called start; every positive trap-containing lexical block is bypassed.
(module
  (memory $wide i64 1)
  (type $record (struct (field i64)))
  (func $wrap (result i32) i64.const 1311768469162688511 i32.wrap_i64)
  (func $sign (result i64) i32.const -2147483648 i64.extend_i32_s)
  (func $zero (result i64) i32.const -1 i64.extend_i32_u)
  (func $wrap_param (param i64) (result i32) local.get 0 i32.wrap_i64)
  (func $sign_param (param i32) (result i64) local.get 0 i64.extend_i32_s)
  (func $zero_param (param i32) (result i64) local.get 0 i64.extend_i32_u)
  (func $roundtrip (result i64) i64.const -1 i32.wrap_i64 i64.extend_i32_u)
  (func $mixed (param i32 i64 i32 i64 i32 i64) (result i64)
    local.get 0 i64.extend_i32_u
    local.get 1 i32.wrap_i64 i64.extend_i32_u
    local.get 2 i64.extend_i32_s
    local.get 3 i32.wrap_i64 i64.extend_i32_s
    local.get 4 i64.extend_i32_u
    local.get 5 i32.wrap_i64 i64.extend_i32_u
    i64.add i64.add i64.add i64.add i64.add)
  (func $default_local (result i64) (local i32 i64)
    local.get 1 i32.wrap_i64 i64.extend_i32_u)
  (func $modern_i31 (result i64) i32.const 123 ref.i31 drop i64.const 42)
  (func $nested_dead (result i64)
    block br 0 unreachable
      block unreachable i32.wrap_i64 drop end
      loop unreachable i64.extend_i32_s drop end
    end i64.const 42)
  (func $tail (result i64) return_call $zero)
  (func $memory64 (result i64) i64.const 0 i64.const 42 i64.store i64.const 0 i64.load)
  (func $called_start (export "_start")
    call $wrap i32.const -1 i32.ne if unreachable end
    call $sign i64.const -2147483648 i64.ne if unreachable end
    call $zero i64.const 4294967295 i64.ne if unreachable end
    i64.const -9223372036854775808 call $wrap_param i32.const 0 i32.ne if unreachable end
    i32.const -2147483648 call $sign_param i64.const -2147483648 i64.ne if unreachable end
    i32.const -1 call $zero_param i64.const 4294967295 i64.ne if unreachable end
    call $roundtrip i64.const 4294967295 i64.ne if unreachable end
    i32.const -1 i64.const 4294967298 i32.const -2 i64.const 4294967295 i32.const 3 i64.const 4294967300
    call $mixed i64.const 4294967301 i64.ne if unreachable end
    call $default_local i64.const 0 i64.ne if unreachable end
    call $modern_i31 i64.const 42 i64.ne if unreachable end
    call $nested_dead i64.const 42 i64.ne if unreachable end
    call $tail i64.const 4294967295 i64.ne if unreachable end
    call $memory64 i64.const 42 i64.ne if unreachable end)
  (start $called_start)
)
