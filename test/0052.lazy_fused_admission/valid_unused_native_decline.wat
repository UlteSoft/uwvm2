;; A real reachable throw is valid Core3 syntax. The component deliberately
;; supplies no qualified native-EH TargetMachine, so the original LLVM emitter
;; declines $unused while typing continues. This is NOT disabled EH validation.
(module
  (tag $problem)
  (func $unused (throw $problem))
  (func (export "answer") (result i32)
    ;; Still type/check the direct dependency; it is never actually executed.
    i32.const 0
    if
      call $unused
    end
    i32.const 42))
