;; The immutable-input resource slice copies original source and resolves live/
;; dropped data; mutable memories/tables/globals and tag/GC objects are explicitly
;; not exported by this first producer. All source features below are modern.
(module
  (rec (type $node (struct (field (mut (ref null $node))) (field i8))))
  (type $event (func (param i64)))
  (tag $tag (type $event))
  (memory $m (export "memory64") i64 1 3)
  (table $t (export "table64") i64 1 3 (ref null $node))
  (global $bits (mut i64) (i64.const -1))
  (global $vector v128 (v128.const i32x4 0x01020304 0x05060708 0x090a0b0c 0x0d0e0f10))
  (data $passive "\01\02\ff\00")
  (data $active (memory $m) (i64.const 16) "\a5\5a\00\ff")
  (@custom ".debug_checkpoint_inputs" "owned-source-then-data-drop")
  (func (export "run") (result i32)
    (local $nondefaultable (ref i31))
    (local $numeric i64)
    i64.const 4
    local.set $numeric
    i32.const 7
    ref.i31
    local.set $nondefaultable
    nop
    data.drop $passive
    nop
    i32.const 42))
