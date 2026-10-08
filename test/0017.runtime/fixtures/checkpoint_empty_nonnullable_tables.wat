;; Core 3 explicit table initializer + nonnullable typed reference + table64.
;; Both empty tables have no element/default VALUE to retain after initialization.
;; Official reference-interpreter/assembler and product validation are pending.
(module
  (type $ft (func))
  (func $f (type $ft))
  (table $empty32 0 0 (ref $ft) (ref.func $f))
  (table $empty64 i64 0 0 (ref $ft) (ref.func $f))
  (elem declare func $f))
