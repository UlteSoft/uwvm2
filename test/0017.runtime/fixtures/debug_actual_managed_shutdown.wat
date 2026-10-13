;; Actual nested native infinite Wasm; cancellation is a private foreign
;; control signal and MUST bypass this real Wasm Core 3 catch_all clause.
(module
  (func $main (export "main") (result i32)
    (block $guest_catch
      (try_table (catch_all $guest_catch)
        (call $spin)))
    ;; Reaching this result means a false guest catch or an incorrect return.
    i32.const 77)
  (func $spin
    (loop $again
      nop
      br $again)))
