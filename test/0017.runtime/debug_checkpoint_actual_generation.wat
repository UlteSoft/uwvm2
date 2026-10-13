(module
  ;; Ensures the real managed aggregate cohort is selected; no host imports.
  (type $box (struct (field i32)))
  (func $entry (export "entry") (result i32)
    nop
    call $worker)
  (func $worker (result i32)
    i32.const 55))
