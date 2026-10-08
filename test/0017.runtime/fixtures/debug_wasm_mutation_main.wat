(module
  (rec (type $node (struct (field (mut (ref null $node))) (field (mut i8)) (field v128))))
  (type $numbers (array (mut i64)))
  (type $node-array (array (mut (ref null $node))))
  (type $compact (struct (field i32)))
  (type $answer-type (func (result i32)))
  (type $saved-if (func (param (ref $node)) (result (ref $node))))
  (type $editable (struct (field (mut i32)) (field (mut i64)) (field (mut f32))
    (field (mut f64)) (field (mut v128)) (field (mut exnref)) (field (mut externref))
    (field (mut (ref null $answer-type))) (field (mut (ref null i31))) (field (mut (ref null $compact)))))
  (type $compact-array (array (mut (ref null $compact))))
  (type $packed-array (array (mut i16)))
  (import "gc-state-provider" "object" (global $object (mut (ref null $node))))
  (import "gc-state-provider" "alias" (global $alias (mut (ref null $node))))
  (import "gc-state-provider" "array" (global $array (mut (ref null $numbers))))
  (import "gc-state-provider" "exception" (global $exception (mut exnref)))
  (import "gc-state-provider" "wrapped" (global $wrapped (mut externref)))
  (import "gc-state-provider" "number" (global $number i64))
  (import "gc-state-provider" "vector" (global $vector v128))
  (import "gc-state-provider" "large" (global $large (mut (ref null $numbers))))
  (import "gc-state-provider" "distinct" (global $distinct (mut (ref null $node-array))))
  (import "gc-state-provider" "alias-array" (global $alias-array (mut (ref null $node-array))))
  (import "gc-state-provider" "compact" (global $compact (mut (ref null $compact))))
  (import "gc-state-provider" "nodes" (table $nodes i64 2 4 (ref null $node)))
  (import "gc-state-provider" "event" (tag $event (param i32 (ref null $node) v128)))
  (global $compact-copy (mut (ref null $compact)) (ref.null $compact))
  (global $node-copy (mut (ref null $node)) (ref.null $node))
  (global $array-copy (mut (ref null $numbers)) (ref.null $numbers))
  (global $exn-copy (mut exnref) (ref.null exn))
  (global $extern-copy (mut externref) (ref.null extern))
  (global $i32-copy (mut i32) (i32.const 0))
  (global $i64-copy (mut i64) (i64.const 0))
  (global $f32-copy (mut f32) (f32.const 0))
  (global $f64-copy (mut f64) (f64.const 0))
  (global $v128-copy (mut v128) (v128.const i32x4 0 0 0 0))
  (global $i31-copy (mut (ref null i31)) (ref.null i31))
  (global $function-copy (mut (ref null $answer-type)) (ref.null $answer-type))
  (global $mutation-armed (mut i32) (i32.const 0))
  (global $editable-root (mut (ref null $editable)) (ref.null $editable))
  (global $compact-array-root (mut (ref null $compact-array)) (ref.null $compact-array))
  (global $packed-array-root (mut (ref null $packed-array)) (ref.null $packed-array))
  (table $function-table i64 2 4 (ref null $answer-type))
  (func $setup
    struct.new_default $editable global.set $editable-root
    i32.const 2 array.new_default $compact-array global.set $compact-array-root
    i32.const 2 array.new_default $packed-array global.set $packed-array-root)
  (func $run (result i32)
    (local $n (ref $node)) (local $a (ref $numbers)) (local $e (ref exn))
    (local $x (ref extern)) (local $number i64)
    global.get $object ref.as_non_null local.set $n
    global.get $array ref.as_non_null local.set $a
    global.get $exception ref.as_non_null local.set $e
    global.get $wrapped ref.as_non_null local.set $x
    i64.const 1234 local.set $number
    local.get $number local.get $n
    i32.const 1 if (type $saved-if)
    ;; Finite scheduling window; retain all complete-cohort and mutation assertions.
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    else
      ;; Actual saved nondefaultable entry parameter is also the else result.
    end
    drop drop
    ;; Scheduler misses are allowed; arm only AFTER an actual whole write batch.
    global.get $mutation-armed if
    ;; Resume MUST consume the actual changed typed global/table function refs.
    global.get $function-copy call_ref $answer-type i32.const 99 i32.ne
    if unreachable end
    i64.const 1 call_indirect $function-table (type $answer-type) i32.const 99 i32.ne
    if unreachable end
    global.get $compact-copy ref.as_non_null struct.get $compact 0 i32.const 37 i32.ne
    if unreachable end
    ;; Genuine later guest execution consumes the debugger-written GC fields.
    ;; The native test arms this only AFTER an authenticated complete edit batch.
    global.get $editable-root ref.as_non_null struct.get $editable 7
    call_ref $answer-type i32.const 99 i32.ne if unreachable end
    global.get $editable-root ref.as_non_null struct.get $editable 9
    ref.as_non_null struct.get $compact 0 i32.const 37 i32.ne if unreachable end
    global.get $compact-array-root ref.as_non_null i32.const 0 array.get $compact-array
    ref.as_non_null struct.get $compact 0 i32.const 37 i32.ne if unreachable end
    global.get $packed-array-root ref.as_non_null i32.const 1 array.get_u $packed-array
    i32.const 9029 i32.ne if unreachable end
    end
    i32.const 42)
  (func $answer (type $answer-type) i32.const 99)
)
