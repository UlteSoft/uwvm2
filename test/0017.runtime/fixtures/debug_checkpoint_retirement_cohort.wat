;; Fresh full census input; setup writes, two guests subsequently read only.
;; Modern memory64/table64, nonnullable typed initializer, GC cycles, EH, SIMD,
;; extern wrappers and live/dropped passive data/element segments.
(module
  (rec (type $node (struct (field (mut (ref null $node))) (field (mut i8)) (field v128))))
  (type $numbers (array (mut i64)))
  (type $node-array (array (mut (ref null $node))))
  (type $callback-type (func (result i32)))
  (tag $event (param i32 (ref null $node) v128))
  (table $nodes i64 2 4 (ref null $node))
  (table $empty i64 0 0 (ref $callback-type) (ref.func $callback))
  (memory $m i64 3 5)
  (data $alive "alive\00")
  (data $gone "gone")
  (elem $alive-elements (ref $callback-type) (ref.func $callback))
  (elem $gone-elements (ref $callback-type) (ref.func $callback))
  (global $object (mut (ref null $node)) (ref.null $node))
  (global $alias (mut (ref null $node)) (ref.null $node))
  (global $array (mut (ref null $numbers)) (ref.null $numbers))
  (global $exception (mut exnref) (ref.null exn))
  (global $wrapped (mut externref) (ref.null extern))
  (global $number i64 (i64.const -1))
  (global $vector v128 (v128.const i32x4 1 2 3 4))
  (global $large (mut (ref null $numbers)) (ref.null $numbers))
  (global $distinct (mut (ref null $node-array)) (ref.null $node-array))
  (global $alias-array (mut (ref null $node-array)) (ref.null $node-array))
  (global $exception-again (mut exnref) (ref.null exn))
  (func $setup
    (local $n (ref $node)) (local $a (ref $numbers))
    (local $children (ref $node-array)) (local $index i32)
    i64.const 9 i32.const 171 i32.store8 $m
    i64.const 131089 i32.const 205 i32.store8 $m
    data.drop $gone elem.drop $gone-elements
    ref.null $node i32.const 127 v128.const i32x4 1 2 3 4 struct.new $node local.set $n
    local.get $n local.get $n struct.set $node 0
    local.get $n global.set $object local.get $n global.set $alias
    i64.const 0 local.get $n table.set $nodes i64.const 1 local.get $n table.set $nodes
    i64.const 10 i64.const 20 i64.const 30 array.new_fixed $numbers 3 local.set $a
    local.get $a global.set $array
    local.get $n extern.convert_any global.set $wrapped
    i64.const 12345 i32.const 1024 array.new $numbers global.set $large
    i32.const 64 array.new_default $node-array local.set $children
    loop $initialize-children
      local.get $children local.get $index
      ref.null $node local.get $index v128.const i32x4 0 0 0 0 struct.new $node
      array.set $node-array
      local.get $index i32.const 1 i32.add local.tee $index i32.const 64 i32.lt_u
      br_if $initialize-children
    end
    local.get $children global.set $distinct
    ;; Keep a real non-default table run, not two identical default slots.
    i64.const 1 local.get $children i32.const 0 array.get $node-array table.set $nodes
    local.get $n i32.const 1024 array.new $node-array global.set $alias-array
    block $caught (result exnref)
      try_table (catch_all_ref $caught)
        i32.const 77 local.get $n v128.const i32x4 5 6 7 8 throw $event
      end
      unreachable
    end
    global.set $exception
    ;; Keep the first actual exception root. Re-catching throw_ref can issue a
    ;; new native carrier token for this SAME immutable Core exception record.
    block $caught-again (result exnref)
      try_table (catch_all_ref $caught-again)
        global.get $exception throw_ref
      end
      unreachable
    end
    global.set $exception-again)
  (func $run (result i32)
    (local $n (ref $node)) (local $a (ref $numbers)) (local $e (ref exn))
    (local $x (ref extern)) (local $number i64)
    (local $again (ref exn))
    (local $retirement-turns i32)
    global.get $object ref.as_non_null local.set $n
    global.get $array ref.as_non_null local.set $a
    global.get $exception ref.as_non_null local.set $e
    global.get $wrapped ref.as_non_null local.set $x
    i64.const 1234 local.set $number
    global.get $exception-again ref.as_non_null local.set $again
    local.get $number local.get $n
    ;; Bounded looping keeps both genuine guests available despite OS scheduling.
    ;; Small static site count avoids thousands of unfolded LLVM EH/debug sites.
    loop $retirement-spin
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      nop
      local.get $retirement-turns i32.const 1 i32.add local.tee $retirement-turns
      i32.const 65536 i32.lt_u br_if $retirement-spin
    end
    drop drop i32.const 42)
  (func $callback (type $callback-type) i32.const 93)
)
