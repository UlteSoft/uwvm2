;; Source-only cross-generation regression. Official wasm-tools parse/validate
;; must qualify both files before any guest execution. No function imports.
(module
  (type $leaf (func (param i32) (result i32)))
  ;; Explicit duplicate canonical function type, used by every dynamic call_ref.
  (type $leaf_alias (func (param i32) (result i32)))
  (type $payload (struct (field (mut i32))))
  (type $refs (array (mut (ref $leaf))))
  (tag $wrong (param (ref $payload)))
  (tag $right (param (ref $payload)))
  (memory (export "memory") 1)
  (global $root (mut (ref null $refs)) (ref.null $refs))
  (global $saved (mut (ref null $leaf)) (ref.null $leaf))
  (global $payload_root (mut (ref null $payload)) (ref.null $payload))
  (elem declare func $numeric_leaf $throwing_leaf)
  (func $numeric_leaf (export "numeric_leaf") (type $leaf)
    local.get 0 i32.const 12 i32.add)
  (func $throwing_leaf (export "throwing_leaf") (type $leaf)
    global.get $payload_root ref.as_non_null
    local.get 0 i32.const 22 i32.add struct.set $payload 0
    global.get $payload_root ref.as_non_null throw $right)
  ;; A real generated nop safe point after both leaves have returned/unwound.
  (func $checkpoint (export "replace_checkpoint") nop)
  (func $tag_caller (export "tag_caller") (result i32)
    (block $done (result i32)
      (block $wrong_exit (result (ref $payload))
        (block $right_exit (result (ref $payload))
          ;; Equal signatures must never let the earlier wrong tag consume right.
          (try_table (catch $wrong $wrong_exit) (catch $right $right_exit)
            i32.const 0
            global.get $root ref.as_non_null i32.const 1 array.get $refs
            call_ref $leaf_alias drop)
          unreachable)
        struct.get $payload 0
        br $done)
      drop
      unreachable))
  (func (export "_start")
    ;; One real GC payload is rooted once; this transport test does not grow
    ;; the GC heap while waiting for a manager to attach or handle an error.
    i32.const 0 struct.new $payload global.set $payload_root
    ;; Capture these identities once in generation one, outside the loop.
    ref.func $numeric_leaf global.set $saved
    ref.func $numeric_leaf ref.func $throwing_leaf array.new_fixed $refs 2
    global.set $root
    (loop $again
      i32.const 0 i32.const 0 global.get $saved ref.as_non_null
      call_ref $leaf_alias i32.store
      i32.const 4 i32.const 0 global.get $root ref.as_non_null
      i32.const 0 array.get $refs call_ref $leaf_alias i32.store
      i32.const 8 call $tag_caller i32.store
      call $checkpoint
      br $again)))
