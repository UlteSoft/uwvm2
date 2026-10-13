;; All22 integer comparisons execute from the genuine called start.
(module
  (memory $wide i64 1)
  (type $record (struct (field i64)))
  (func $i32_eqz (result i32) i32.const 0 i32.eqz)
  (func $i32_eq (result i32) i32.const -1 i32.const -1 i32.eq)
  (func $i32_ne (result i32) i32.const 0 i32.const 1 i32.ne)
  (func $i32_lt_s (result i32) i32.const -2147483648 i32.const 0 i32.lt_s)
  (func $i32_lt_u (result i32) i32.const -1 i32.const 0 i32.lt_u)
  (func $i32_gt_s (result i32) i32.const -2147483648 i32.const 0 i32.gt_s)
  (func $i32_gt_u (result i32) i32.const -1 i32.const 0 i32.gt_u)
  (func $i32_le_s (result i32) i32.const -2147483648 i32.const -2147483648 i32.le_s)
  (func $i32_le_u (result i32) i32.const -1 i32.const 0 i32.le_u)
  (func $i32_ge_s (result i32) i32.const -2147483648 i32.const 0 i32.ge_s)
  (func $i32_ge_u (result i32) i32.const -1 i32.const 0 i32.ge_u)
  (func $i64_eqz (result i32) i64.const 1 i64.eqz)
  (func $i64_eq (result i32) i64.const -1 i64.const -1 i64.eq)
  (func $i64_ne (result i32) i64.const 0 i64.const 1 i64.ne)
  (func $i64_lt_s (result i32) i64.const -9223372036854775808 i64.const 0 i64.lt_s)
  (func $i64_lt_u (result i32) i64.const -1 i64.const 0 i64.lt_u)
  (func $i64_gt_s (result i32) i64.const -9223372036854775808 i64.const 0 i64.gt_s)
  (func $i64_gt_u (result i32) i64.const -1 i64.const 0 i64.gt_u)
  (func $i64_le_s (result i32) i64.const -9223372036854775808 i64.const -9223372036854775808 i64.le_s)
  (func $i64_le_u (result i32) i64.const -1 i64.const 0 i64.le_u)
  (func $i64_ge_s (result i32) i64.const -9223372036854775808 i64.const 0 i64.ge_s)
  (func $i64_ge_u (result i32) i64.const -1 i64.const 0 i64.ge_u)
  (func $param32 (param i32) (result i32) local.get 0 i32.eqz)
  (func $param64 (param i64) (result i32) local.get 0 i64.eqz)
  (func $pair64 (param i64 i64) (result i32) local.get 0 local.get 1 i64.gt_u)
  (func $modern_i31 (result i32) i32.const 123 ref.i31 drop i32.const 42)
  (func $nested_dead (result i32)
    block br 0 unreachable
      block unreachable i64.eq drop end
      loop unreachable i32.eqz drop end
    end i32.const 42)
  (func $tail (result i32) return_call $i32_eqz)
  (func $memory64 (result i32) i64.const 0 i32.const 42 i32.store i64.const 0 i32.load)
  (func $called_start (export "_start")
    call $i32_eqz i32.const 1 i32.ne if unreachable end
    call $i32_eq i32.const 1 i32.ne if unreachable end
    call $i32_ne i32.const 1 i32.ne if unreachable end
    call $i32_lt_s i32.const 1 i32.ne if unreachable end
    call $i32_lt_u i32.const 0 i32.ne if unreachable end
    call $i32_gt_s i32.const 0 i32.ne if unreachable end
    call $i32_gt_u i32.const 1 i32.ne if unreachable end
    call $i32_le_s i32.const 1 i32.ne if unreachable end
    call $i32_le_u i32.const 0 i32.ne if unreachable end
    call $i32_ge_s i32.const 0 i32.ne if unreachable end
    call $i32_ge_u i32.const 1 i32.ne if unreachable end
    call $i64_eqz i32.const 0 i32.ne if unreachable end
    call $i64_eq i32.const 1 i32.ne if unreachable end
    call $i64_ne i32.const 1 i32.ne if unreachable end
    call $i64_lt_s i32.const 1 i32.ne if unreachable end
    call $i64_lt_u i32.const 0 i32.ne if unreachable end
    call $i64_gt_s i32.const 0 i32.ne if unreachable end
    call $i64_gt_u i32.const 1 i32.ne if unreachable end
    call $i64_le_s i32.const 1 i32.ne if unreachable end
    call $i64_le_u i32.const 0 i32.ne if unreachable end
    call $i64_ge_s i32.const 0 i32.ne if unreachable end
    call $i64_ge_u i32.const 1 i32.ne if unreachable end
    i32.const -2147483648 call $param32 i32.const 0 i32.ne if unreachable end
    i64.const -9223372036854775808 call $param64 i32.const 0 i32.ne if unreachable end
    i64.const -1 i64.const 0 call $pair64 i32.const 1 i32.ne if unreachable end
    call $modern_i31 i32.const 42 i32.ne if unreachable end
    call $nested_dead i32.const 42 i32.ne if unreachable end
    call $tail i32.const 1 i32.ne if unreachable end
    call $memory64 i32.const 42 i32.ne if unreachable end)
  (start $called_start)
)
