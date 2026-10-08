;; Official parser may produce bytes; validation MUST reject the unused last
;; function because memory64 needs an i64 address. _start MUST NOT execute.
(module
  (memory i64 1)
  (func $warm (result i32) i64.const 0 i32.load)
  (func (export "_start") call $warm drop unreachable)
  (func $unused i32.const 0 i32.load drop))
