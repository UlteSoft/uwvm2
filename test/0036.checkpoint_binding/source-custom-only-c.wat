(module
  (rec (type $node (struct (field (mut (ref null $node))) (field i8))))
  (memory i64 1 2)
  (table i64 1 2 (ref null $node))
  (data "\01\02\03\04")
  (@custom ".debug_identity" "build-and-Wasm-C")
  (func (export "run") (result i32) (local $x (ref i31))
    i32.const 7 ref.i31 local.set $x i32.const 42))
