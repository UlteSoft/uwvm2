(module $tiered_cold_provider
  ;; No start function and no loop: this function is still cold when the
  ;; consumer's already-native dispatch function reaches its first import call.
  (func $answer (export "answer") (param i32) (result i32)
    unreachable))
