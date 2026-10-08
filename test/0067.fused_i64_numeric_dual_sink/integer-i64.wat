;; Actual called start asserts all18 first-walk integer results. The module also
;; contains memory64, GC and tail-call syntax, plus genuinely unused trap bodies.
(module
  (memory $wide i64 1)
  (type $record (struct (field i64)))
  (func $empty_start)
  (func $clz (result i64) i64.const 0 i64.clz)
  (func $ctz (result i64) i64.const 0 i64.ctz)
  (func $popcnt (result i64) i64.const -1 i64.popcnt)
  (func $add (result i64) i64.const 9223372036854775807 i64.const 1 i64.add)
  (func $sub (result i64) i64.const 0 i64.const 1 i64.sub)
  (func $mul (result i64) i64.const 4611686018427387904 i64.const 4 i64.mul)
  (func $div_s (result i64) i64.const -17 i64.const 5 i64.div_s)
  (func $div_u (result i64) i64.const -1 i64.const 2 i64.div_u)
  (func $rem_s (result i64) i64.const -9223372036854775808 i64.const -1 i64.rem_s)
  (func $rem_u (result i64) i64.const -1 i64.const 2 i64.rem_u)
  (func $and (result i64) i64.const -1 i64.const 6148914691236517205 i64.and)
  (func $or (result i64) i64.const 6148914691236517205 i64.const -6148914691236517206 i64.or)
  (func $xor (result i64) i64.const -1 i64.const 6148914691236517205 i64.xor)
  (func $shl (result i64) i64.const 1 i64.const 65 i64.shl)
  (func $shr_s (result i64) i64.const -9223372036854775808 i64.const 65 i64.shr_s)
  (func $shr_u (result i64) i64.const -9223372036854775808 i64.const 65 i64.shr_u)
  (func $rotl (result i64) i64.const -9223372036854775808 i64.const 65 i64.rotl)
  (func $rotr (result i64) i64.const 1 i64.const 65 i64.rotr)
  (func $cached (param i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64) (result i64)
    local.get 0 i64.const 0 i64.add local.get 1 i64.const 0 i64.add local.get 2 i64.const 0 i64.add local.get 3 i64.const 0 i64.add local.get 4 i64.const 0 i64.add local.get 5 i64.const 0 i64.add local.get 6 i64.const 0 i64.add local.get 7 i64.const 0 i64.add local.get 8 i64.const 0 i64.add local.get 9 i64.const 0 i64.add local.get 10 i64.const 0 i64.add local.get 11 i64.const 0 i64.add local.get 12 i64.const 0 i64.add local.get 13 i64.const 0 i64.add local.get 14 i64.const 0 i64.add local.get 15 i64.const 0 i64.add local.get 16 i64.const 0 i64.add local.get 17 i64.const 0 i64.add local.get 18 i64.const 0 i64.add local.get 19 i64.const 0 i64.add local.get 20 i64.const 0 i64.add local.get 21 i64.const 0 i64.add local.get 22 i64.const 0 i64.add local.get 23 i64.const 0 i64.add
    i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add)
  (func $default_local (result i64) (local i64) local.get 0 i64.clz)
  (func $modern_i31 (result i64) i32.const 123 ref.i31 drop i64.const 42)
  (func $nested_dead (result i64)
    block br 0 unreachable
      block unreachable i64.add drop end
      loop unreachable i64.clz drop end
    end i64.const 42)
  (func $tail (result i64) return_call $clz)
  (func $called_start (export "_start")
    call $clz i64.const 64 i64.ne if unreachable end
    call $ctz i64.const 64 i64.ne if unreachable end
    call $popcnt i64.const 64 i64.ne if unreachable end
    call $add i64.const -9223372036854775808 i64.ne if unreachable end
    call $sub i64.const -1 i64.ne if unreachable end
    call $mul i64.const 0 i64.ne if unreachable end
    call $div_s i64.const -3 i64.ne if unreachable end
    call $div_u i64.const 9223372036854775807 i64.ne if unreachable end
    call $rem_s i64.const 0 i64.ne if unreachable end
    call $rem_u i64.const 1 i64.ne if unreachable end
    call $and i64.const 6148914691236517205 i64.ne if unreachable end
    call $or i64.const -1 i64.ne if unreachable end
    call $xor i64.const -6148914691236517206 i64.ne if unreachable end
    call $shl i64.const 2 i64.ne if unreachable end
    call $shr_s i64.const -4611686018427387904 i64.ne if unreachable end
    call $shr_u i64.const 4611686018427387904 i64.ne if unreachable end
    call $rotl i64.const 1 i64.ne if unreachable end
    call $rotr i64.const -9223372036854775808 i64.ne if unreachable end
    i64.const 1 i64.const 2 i64.const 3 i64.const 4 i64.const 5 i64.const 6 i64.const 7 i64.const 8 i64.const 9 i64.const 10 i64.const 11 i64.const 12 i64.const 13 i64.const 14 i64.const 15 i64.const 16 i64.const 17 i64.const 18 i64.const 19 i64.const 20 i64.const 21 i64.const 22 i64.const 23 i64.const 24 call $cached i64.const 300 i64.ne if unreachable end
    i64.const 1 i64.const 2 i64.const 3 i64.const 4 i64.const 5 i64.const 6 i64.const 7 i64.const 8 i64.const 9 i64.const 10 i64.const 11 i64.const 12 i64.const 13 i64.const 14 i64.const 15 i64.const 16 i64.const 17 i64.const 18 i64.const 19 i64.const 20 i64.const 21 i64.const 22 i64.const 23 i64.const 24 call $heavy24 i64.const 300 i64.ne if unreachable end
    i64.const 1 i64.const 2 i64.const 3 i64.const 4 i64.const 5 i64.const 6 i64.const 7 i64.const 8 call $heavy8 i64.const 36 i64.ne if unreachable end
    i32.const 1 i32.const 2 i32.const 3 i32.const 4 i32.const 5 i32.const 6 i32.const 7 i32.const 8 call $heavy8_i32 i32.const 36 i32.ne if unreachable end
    call $default_local i64.const 64 i64.ne if unreachable end
    call $modern_i31 i64.const 42 i64.ne if unreachable end
    call $nested_dead i64.const 42 i64.ne if unreachable end
    call $tail i64.const 64 i64.ne if unreachable end)
  (start $called_start)
  (func $div_zero_s (result i64) i64.const 1 i64.const 0 i64.div_s)
  (func $div_overflow_s (result i64) i64.const -9223372036854775808 i64.const -1 i64.div_s)
  (func $div_zero_u (result i64) i64.const 1 i64.const 0 i64.div_u)
  (func $rem_zero_s (result i64) i64.const 1 i64.const 0 i64.rem_s)
  (func $rem_zero_u (result i64) i64.const 1 i64.const 0 i64.rem_u)
  (func $heavy24 (param i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64) (result i64)
    local.get 0 local.get 1 local.get 2 local.get 3 local.get 4 local.get 5 local.get 6 local.get 7 local.get 8 local.get 9 local.get 10 local.get 11 local.get 12 local.get 13 local.get 14 local.get 15 local.get 16 local.get 17 local.get 18 local.get 19 local.get 20 local.get 21 local.get 22 local.get 23
    i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add i64.add)
  (func $heavy8 (param i64 i64 i64 i64 i64 i64 i64 i64) (result i64)
    local.get 0 local.get 1 local.get 2 local.get 3 local.get 4 local.get 5 local.get 6 local.get 7
    i64.add i64.add i64.add i64.add i64.add i64.add i64.add)
  (func $heavy8_i32 (param i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    local.get 0 local.get 1 local.get 2 local.get 3 local.get 4 local.get 5 local.get 6 local.get 7
    i32.add i32.add i32.add i32.add i32.add i32.add i32.add)
)
