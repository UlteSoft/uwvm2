;; Genuine start executes numeric, SIMD, GC, defined and exception typed-select.
(module
  (type $record (struct (field i32)))
  (func $narrow (result i32) i32.const 11 i32.const 22 i32.const 1 select (result i32))
  (func $wide (result i64) i64.const -1 i64.const 42 i32.const 0 select (result i64))
  (func $cached32 (param i32 i32 i32) (result i32)
    local.get 0 local.get 1 local.get 2 select (result i32))
  (func $cached64 (param i64 i64 i32) (result i64)
    local.get 0 local.get 1 local.get 2 select (result i64))
  (func $gc (result i32)
    i32.const 123 ref.i31 i32.const 456 ref.i31 i32.const 1
    select (result eqref) ref.cast (ref i31) i31.get_u)
  (func $exception (result i32)
    ref.null noexn ref.null exn i32.const 0 select (result exnref) ref.is_null)
  (func $vector (result i32)
    v128.const i32x4 1 2 3 4 v128.const i32x4 9 8 7 6 i32.const 1
    select (result v128) i32x4.extract_lane 0)
  (func $defined (result i32)
    i32.const 42 struct.new $record ref.null $record i32.const 1
    select (result (ref null $record)) ref.as_non_null struct.get $record 0)
  (func $dead (result i32)
    block br 0 unreachable
      block unreachable select (result exnref) drop end
      loop unreachable select (result i64) drop end
    end i32.const 42)
  (func $called_start (export "_start")
    call $narrow i32.const 11 i32.ne if unreachable end
    call $wide i64.const 42 i64.ne if unreachable end
    i32.const 11 i32.const 22 i32.const 1 call $cached32 i32.const 11 i32.ne if unreachable end
    i64.const -1 i64.const 42 i32.const 0 call $cached64 i64.const 42 i64.ne if unreachable end
    call $gc i32.const 123 i32.ne if unreachable end
    call $exception i32.const 1 i32.ne if unreachable end
    call $vector i32.const 1 i32.ne if unreachable end
    call $defined i32.const 42 i32.ne if unreachable end
    call $dead i32.const 42 i32.ne if unreachable end)
  (start $called_start))
