(module $tiered_cold_provider
  (func $answer (export "answer") (param i32) (result i32)
    local.get 0
    i32.const 1
    i32.add))
