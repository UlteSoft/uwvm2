;; Core 3 checkpoint reference identity fixture. The host adapter supplies a
;; genuine nonnull ref.host (any); the host must compare returned identity.
;; Parsing/validating alone does not qualify checkpoint restore or host replay.
(module
  (type $node (struct (field (mut i32))))
  (type $nodes (array (mut (ref null $node))))
  (import "checkpoint-host" "identity" (global $host (ref any)))
  (func (export "host_roundtrip") (result (ref any))
    global.get $host
    extern.convert_any
    any.convert_extern)
  (func (export "host_external") (result (ref extern))
    global.get $host
    extern.convert_any)
  (func (export "struct_roundtrip") (result i32)
    (local $node (ref null $node))
    (local.set $node (struct.new $node (i32.const 123)))
    (ref.eq (local.get $node)
      (ref.cast (ref $node) (any.convert_extern (extern.convert_any (local.get $node))))))
  (func (export "array_roundtrip") (result i32)
    (local $array (ref null $nodes))
    (local.set $array (array.new $nodes (ref.null $node) (i32.const 2)))
    (ref.eq (local.get $array)
      (ref.cast (ref $nodes) (any.convert_extern (extern.convert_any (local.get $array))))))
  (func (export "i31_roundtrip") (result i32)
    (i31.get_u (ref.cast (ref i31)
      (any.convert_extern (extern.convert_any (ref.i31 (i32.const -1)))))))
  (func (export "null_roundtrip") (result i32)
    (ref.is_null (any.convert_extern (extern.convert_any (ref.null any))))))
