;; X and Y are two export names for exactly one tag INSTANCE.
(module
  (tag $event (param i32))
  (export "X" (tag $event))
  (export "Y" (tag $event))
  (func $raise (export "raise")
    i32.const 41
    throw $event))
