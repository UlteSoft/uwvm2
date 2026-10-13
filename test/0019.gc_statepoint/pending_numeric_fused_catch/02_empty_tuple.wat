(module
  (tag $empty)
  (func $throw_empty (result i32) throw $empty)
  (func (export "run") (result i32)
    (block $done
      (try_table (catch $empty $done) call $throw_empty drop))
    i32.const 31415)
)
