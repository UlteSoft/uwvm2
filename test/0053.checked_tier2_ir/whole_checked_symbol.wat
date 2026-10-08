;; Genuine whole checked-IR owner: retained loop metadata, static direct call,
;; typed select and a memory64 declaration all come from actual fused admission.
(module
  (memory i64 1)
  (func $base (result i32) (local $iteration i32)
    loop $again
      local.get $iteration i32.const 1 i32.add local.tee $iteration
      i32.const 4 i32.lt_u br_if $again
    end
    i32.const 40)
  (func (export "answer") (result i32)
    call $base
    i32.const 2 i32.const 99 i32.const 1 select (result i32)
    i32.add))
