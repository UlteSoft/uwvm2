(module
  (type $box (struct (field i32)))
  (memory i64 1 2)
  (global $sum i32 (i32.add (i32.const 19) (i32.const 23)))
  (func $leaf (result i32) i32.const 9)
  (table i64 1 (ref func) (ref.func $leaf))
  (func $run (param i32) (result i32) local.get 0 i32.const 40 i32.add)
  (func $gc (result i32) i32.const 42 struct.new $box struct.get $box 0)
)
