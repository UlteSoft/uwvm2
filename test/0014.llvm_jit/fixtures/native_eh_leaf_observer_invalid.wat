(module
  (tag $a (param i32))
  (func $leaf (param i32) (result i32)
    (local.get 0) (throw $a))
  (func (export "invalid_later") (result i32)
    (block $out (result i32)
      (try_table (result i32) (catch $a $out)
        (i32.const 7) (call $leaf)))
    (drop)))
