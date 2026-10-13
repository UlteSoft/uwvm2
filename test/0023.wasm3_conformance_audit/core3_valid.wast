;; Independent Core 3 probes. Every nontrapping module has a _start that reaches
;; every defined probe; do not validate lazy modes by merely loading the module.

(module $gc_gate_baseline
  (func (export "_start")))
(assert_return (invoke $gc_gate_baseline "_start"))

(module $reference_bottom
  (func (export "_start")
    unreachable
    ref.as_non_null
    throw_ref))
(assert_trap (invoke $reference_bottom "_start") "unreachable")

(module $nondefaultable_local
  (type $s (struct))
  (func $probe (export "probe") (result i32) (local $r (ref $s))
    (block
      struct.new $s
      local.set $r
      local.get $r
      drop)
    i32.const 17)
  (func (export "_start") call $probe drop))
(assert_return (invoke $nondefaultable_local "probe") (i32.const 17))
(assert_return (invoke $nondefaultable_local "_start"))

(module $recursive_identity
  (rec (type $a (func)) (type (struct (field (ref $a)))))
  (rec (type $b (func)) (type (struct (field (ref $b)))))
  (func $f (type $b))
  (global $r (ref $a) (ref.func $f))
  (func $probe (export "probe") (result i32) global.get $r ref.is_null)
  (func (export "_start") call $f call $probe drop))
(assert_return (invoke $recursive_identity "probe") (i32.const 0))
(assert_return (invoke $recursive_identity "_start"))

(module $common_branch_subtype
  (func $probe (export "probe") (param $which i32) (result i32)
    (block $outer (result (ref eq))
      (block $inner (result (ref i31))
        i32.const 73
        ref.i31
        local.get $which
        br_table $inner $outer))
    ref.cast (ref i31)
    i31.get_u)
  (func (export "_start") i32.const 0 call $probe drop i32.const 1 call $probe drop))
(assert_return (invoke $common_branch_subtype "probe" (i32.const 0)) (i32.const 73))
(assert_return (invoke $common_branch_subtype "probe" (i32.const 1)) (i32.const 73))
(assert_return (invoke $common_branch_subtype "_start"))

(module $sibling_cast
  (type $left (struct (field i32)))
  (type $right (struct (field i64)))
  (func $probe (export "probe") (result i32)
    ref.null $left
    ref.cast (ref null $right)
    ref.is_null)
  (func (export "_start") call $probe drop))
(assert_return (invoke $sibling_cast "probe") (i32.const 1))
(assert_return (invoke $sibling_cast "_start"))

(module $aggregate_subtype
  (type $base (sub (struct (field i32))))
  (type $child (sub $base (struct (field i32) (field i64))))
  (func $probe (export "probe") (result i32)
    i32.const 29
    i64.const 5
    struct.new $child
    ref.cast (ref $base)
    struct.get $base 0)
  (func (export "_start") call $probe drop))
(assert_return (invoke $aggregate_subtype "probe") (i32.const 29))
(assert_return (invoke $aggregate_subtype "_start"))

(module $immutable_packed_data
  (type $bytes (array i8))
  (memory 1)
  (data $d "\ff\80")
  (func $probe (export "probe") (result i32)
    i32.const 0
    i32.const 2
    array.new_data $bytes $d
    i32.const 0
    array.get_u $bytes)
  (func (export "_start") call $probe drop))
(assert_return (invoke $immutable_packed_data "probe") (i32.const 255))
(assert_return (invoke $immutable_packed_data "_start"))

(module $wide_storage
  (memory $m32 1)
  (memory $m64 i64 1)
  (table $t32 2 funcref)
  (table $t64 i64 2 funcref)
  (func $probe (export "probe") (result i32)
    i32.const 0 i64.const 0 i32.const 0 memory.copy $m32 $m64
    i64.const 0 i32.const 0 i32.const 0 memory.copy $m64 $m32
    i64.const 0 i64.const 0 i64.const 0 memory.copy $m64 $m64
    i32.const 0 i64.const 0 i32.const 0 table.copy $t32 $t64
    i64.const 0 i32.const 0 i32.const 0 table.copy $t64 $t32
    i64.const 0 i64.const 0 i64.const 0 table.copy $t64 $t64
    i64.const 8 i32.const 61 i32.store $m64
    i64.const 8 i32.load $m64)
  (func (export "_start") call $probe drop))
(assert_return (invoke $wide_storage "probe") (i32.const 61))
(assert_return (invoke $wide_storage "_start"))

(module $typed_exception_payload
  (type $f (func))
  (func $target (type $f))
  (elem declare func $target)
  (tag $e (param (ref $f)))
  (func $probe (export "probe") (result i32)
    (block $handler (result (ref null func) (ref null exn))
      (try_table (catch_ref $e $handler)
        ref.func $target
        throw $e)
      unreachable)
    drop
    ref.is_null)
  (func (export "_start") call $target call $probe drop))
(assert_return (invoke $typed_exception_payload "probe") (i32.const 0))
(assert_return (invoke $typed_exception_payload "_start"))

(module $tail_reference_covariance
  (type $f (func))
  (func $target (type $f))
  (elem declare func $target)
  (func $callee (result (ref $f)) ref.func $target)
  (func $tail (export "probe") (result (ref null func)) return_call $callee)
  (func (export "_start") call $target call $callee drop call $tail drop))
(assert_return (invoke $tail_reference_covariance "probe") (ref.func))
(assert_return (invoke $tail_reference_covariance "_start"))

(module $unreachable_fixed_array
  (type $a (array (mut i32)))
  (func (export "_start") unreachable array.new_fixed $a 4294967295 drop))
(assert_trap (invoke $unreachable_fixed_array "_start") "unreachable")

(module $relaxed_simd
  (func $probe (export "probe") (result i32)
    v128.const i32x4 1 2 3 4
    v128.const i32x4 0 0 0 0
    i8x16.relaxed_swizzle
    i8x16.extract_lane_u 0)
  (func (export "_start") call $probe drop))
(assert_return (invoke $relaxed_simd "probe") (i32.const 1))
(assert_return (invoke $relaxed_simd "_start"))
