;; Valid wide offset in unreachable code; runtime overflow must trap, never wrap.
(module $wide
 (memory i64 1 1)
 (func (export "wide_unused") unreachable i32.load offset=18446744073709551615 drop)
 (func (export "overflow") (result i32) i64.const 1 i32.load offset=18446744073709551615)
 (func (export "oob") (result i32) i64.const 65533 i32.load))
(assert_trap (invoke $wide "overflow") "out of bounds memory access")
(assert_trap (invoke $wide "oob") "out of bounds memory access")
(assert_invalid (module (memory i64 1) (func i32.const 0 i32.load drop)) "type mismatch")
(assert_invalid (module (memory i64 1) (func i64.const 0 i64.const 0 i32.store)) "type mismatch")
(assert_invalid (module (memory i64 1) (func i64.const 0 i32.load align=8 drop)) "alignment must not be larger than natural")
(assert_invalid (module (memory 1) (func unreachable i32.load offset=4294967296 drop)) "offset out of range")
