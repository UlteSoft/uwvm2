(module $m32
  (memory 1 1)
  (func (export "size") (result i32) memory.size)
  (func (export "grow-zero") (result i32) (local $r i32)
    i32.const 0 memory.grow local.tee $r
    i32.const 1 i32.eq local.get $r i32.const -1 i32.eq i32.or
    memory.size i32.const 1 i32.eq i32.and)
  (func (export "grow-fail") (result i32) i32.const 1 memory.grow))
(assert_return (invoke $m32 "size") (i32.const 1))
(assert_return (invoke $m32 "grow-zero") (i32.const 1))
(assert_return (invoke $m32 "grow-fail") (i32.const -1))
(module $m64
  (memory i64 1 1)
  (func (export "size") (result i64) memory.size)
  (func (export "grow-zero") (result i32) (local $r i64)
    i64.const 0 memory.grow local.tee $r
    i64.const 1 i64.eq local.get $r i64.const -1 i64.eq i32.or
    memory.size i64.const 1 i64.eq i32.and)
  (func (export "grow-fail") (result i64) i64.const 1 memory.grow))
(assert_return (invoke $m64 "size") (i64.const 1))
(assert_return (invoke $m64 "grow-zero") (i32.const 1))
(assert_return (invoke $m64 "grow-fail") (i64.const -1))
(module $mixed
  (memory $narrow 1 1) (memory $wide i64 1 1)
  (func (export "size-wide") (result i64) memory.size $wide)
  (func (export "grow-wide-zero") (result i32) (local $r i64)
    i64.const 0 memory.grow $wide local.tee $r
    i64.const 1 i64.eq local.get $r i64.const -1 i64.eq i32.or
    memory.size $wide i64.const 1 i64.eq i32.and)
  (func (export "size-narrow") (result i32) memory.size $narrow)
  (func (export "grow-narrow-zero") (result i32) (local $r i32)
    i32.const 0 memory.grow $narrow local.tee $r
    i32.const 1 i32.eq local.get $r i32.const -1 i32.eq i32.or
    memory.size $narrow i32.const 1 i32.eq i32.and)
  ;; These unused valid bodies check synthetic value Bot without execution.
  (func unreachable memory.grow $wide drop)
  (func unreachable memory.grow $narrow drop)
  (func unreachable memory.size $wide drop))
(assert_return (invoke $mixed "size-wide") (i64.const 1))
(assert_return (invoke $mixed "grow-wide-zero") (i32.const 1))
(assert_return (invoke $mixed "size-narrow") (i32.const 1))
(assert_return (invoke $mixed "grow-narrow-zero") (i32.const 1))
(assert_invalid (module (memory 1 1) (func memory.grow drop)) "type mismatch")
(assert_invalid (module (memory i64 1 1) (func memory.grow drop)) "type mismatch")
(assert_invalid (module (memory 1 1) (func i64.const 0 memory.grow drop)) "type mismatch")
(assert_invalid (module (memory i64 1 1) (func i32.const 0 memory.grow drop)) "type mismatch")
(assert_invalid (module (memory 1 1) (func unreachable f32.const 0 memory.grow drop)) "type mismatch")
(assert_invalid (module (memory i64 1 1) (func unreachable f64.const 0 memory.grow drop)) "type mismatch")
(assert_invalid (module (memory 1 1) (func unreachable ref.as_non_null memory.grow drop)) "type mismatch")
(assert_invalid (module (memory i64 1 1) (func unreachable ref.as_non_null memory.grow drop)) "type mismatch")
