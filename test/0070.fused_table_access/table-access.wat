;; Genuine start executes table32/table64, defined/abstract/exn references.
(module
  (type $answer_type (func (result i32)))
  (type $record (struct (field i32)))
  (table $wide i64 2 funcref)
  (table $small 2 funcref)
  (table $external i64 1 externref)
  (table $internal i64 1 eqref)
  (table $exception i64 1 exnref)
  (table $defined i64 1 (ref null $record))
  (elem declare func $answer)
  (func $answer (type $answer_type) i32.const 77)
  (func $get32 (result i32) i32.const 0 table.get $small ref.is_null)
  (func $set32 (result i32)
    i32.const 0 ref.func $answer table.set $small
    i32.const 0 table.get $small ref.cast (ref $answer_type) call_ref $answer_type)
  (func $get64 (result i32) i64.const 0 table.get $wide ref.is_null)
  (func $set64 (result i32)
    i64.const 0 ref.func $answer table.set $wide
    i64.const 0 table.get $wide ref.cast (ref $answer_type) call_ref $answer_type)
  (func $external (result i32)
    i64.const 0 ref.null noextern table.set $external
    i64.const 0 table.get $external ref.is_null)
  (func $internal (result i32)
    i64.const 0 i32.const 123 ref.i31 table.set $internal
    i64.const 0 table.get $internal ref.cast (ref i31) i31.get_u)
  (func $defined (result i32)
    i64.const 0 i32.const 99 struct.new $record table.set $defined
    i64.const 0 table.get $defined ref.as_non_null struct.get $record 0)
  (func $exception (result i32)
    i64.const 0 ref.null noexn table.set $exception
    i64.const 0 table.get $exception ref.is_null)
  (func $typed_selection (result i32)
    i64.const 1 ref.null func ref.func $answer i32.const 0
    select (result funcref) table.set $wide
    i64.const 1 table.get $wide ref.cast (ref $answer_type) call_ref $answer_type)
  (func $param64 (param i64) (result i32) local.get 0 table.get $wide ref.is_null)
  (func $dead (result i32)
    block br 0 unreachable
      block unreachable table.get $defined drop end
      loop unreachable table.set $exception end
    end i32.const 42)
  (func $called_start (export "_start")
    call $get32 i32.const 1 i32.ne if unreachable end
    call $get64 i32.const 1 i32.ne if unreachable end
    call $set32 i32.const 77 i32.ne if unreachable end
    call $set64 i32.const 77 i32.ne if unreachable end
    call $external i32.const 1 i32.ne if unreachable end
    call $internal i32.const 123 i32.ne if unreachable end
    call $defined i32.const 99 i32.ne if unreachable end
    call $exception i32.const 1 i32.ne if unreachable end
    call $typed_selection i32.const 77 i32.ne if unreachable end
    i64.const 1 call $param64 i32.eqz if else unreachable end
    call $dead i32.const 42 i32.ne if unreachable end)
  (start $called_start))
