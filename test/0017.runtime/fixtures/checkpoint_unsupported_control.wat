;; Legal Core3 module: the active nested control is temporarily unavailable
;; to the checkpoint producer, while normal Wasm validation/execution succeeds.
(module
  (func (export "run") (result i32)
    (local (ref i31)) (local i64)
    block (result i32)
      i32.const 42
    end))
