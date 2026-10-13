(module
  (tag $a (param i32))
  (tag $b (param i32))
  (func $throw_b (result i32) i32.const 73 throw $b)
  (func $caught (result i32)
    (block $wrong (result i32)
      (block $all
        (try_table (catch $a $wrong) (catch_all $all)
          call $throw_b drop))
      i32.const 777))
  (func (export "run") (result i32)
    call $caught
    i32.const 777 i32.eq)
)
