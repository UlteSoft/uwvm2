;; Real memory64 + GC i31 + typed select input. Both original LLVM-lazy
;; admission and whole owned-IR consumption use the same trusted fused walker.
(module
  (memory i64 1)
  (func $base (result i32) (local $iteration i32)
    i32.const 17 ref.i31 i31.get_u drop
    loop $again
      local.get $iteration i32.const 1 i32.add local.tee $iteration
      i32.const 4 i32.lt_u br_if $again
    end
    i32.const 40)
  (func (export "answer") (result i32)
    call $base
    i32.const 2 i32.const 99 i32.const 1 select (result i32)
    i32.add))
