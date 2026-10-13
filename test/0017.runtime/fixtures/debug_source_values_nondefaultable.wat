;; Actual full/native source-value capture regression. Original local indices:
;; parameter0, nondefaultable GC ref1, defaultable i32 value2, nullable GC ref3.
;; Capturing before first local.set must not load ref1's native alloca. Its
;; readability flag changes after the set; other original slots stay intact.
(module
  (type $node (struct (field i32)))
  (func $helper (param i32) (result i32)
    (local $unset (ref $node))
    (local $value i32)
    (local $nullable (ref null $node))
    nop
    (local.set $value (i32.add (local.get 0) (i32.const 2)))
    (local.set $unset (struct.new $node (i32.const 42)))
    nop
    (local.get $value))
  (func (export "_start") (result i32)
    (call $helper (i32.const 5))))
