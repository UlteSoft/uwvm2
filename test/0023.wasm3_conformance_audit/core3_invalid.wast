;; Each case requires a validation rejection, including unreachable bodies.

(assert_invalid
  (module (func (export "_start") unreachable ref.as_non_null i32.const 0 i32.add drop))
  "type mismatch")

;; A catch label is looked up before try_table pushes its own control frame.
(assert_invalid
  (module (func (export "_start") (try_table (catch_all 1))))
  "unknown label")

(assert_invalid
  (module
    (type $f (func))
    (tag $e (param (ref null $f)))
    (func (export "_start")
      (block $handler (result (ref $f))
        (try_table (catch $e $handler))
        unreachable)
      drop))
  "type mismatch")

;; Core 3 local initialization is scoped to the control frame; assigning in
;; both arms does not initialize a nondefaultable local outside that if.
(assert_invalid
  (module
    (type $s (struct))
    (func (export "_start") (local $r (ref $s))
      i32.const 1
      (if (then struct.new $s local.set $r) (else struct.new $s local.set $r))
      local.get $r
      drop))
  "uninitialized local")

(assert_invalid
  (module
    (table $small 1 funcref)
    (table $large i64 1 funcref)
    (func (export "_start") i32.const 0 i64.const 0 i64.const 0 table.copy $small $large))
  "type mismatch")

(assert_invalid
  (module (memory i64 1) (func (export "_start") i32.const 0 i32.load drop))
  "type mismatch")

;; Both table types have the same legacy 0x70 carrier, but unrelated heaps.
(assert_invalid
  (module
    (table $f 1 funcref)
    (table $g 1 anyref)
    (func (export "_start") i32.const 0 i32.const 0 i32.const 0 table.copy $f $g))
  "type mismatch")

;; A recursive projection's identity includes its enclosing recursive group.
(assert_invalid
  (module
    (rec (type $grouped (func)) (type (func)))
    (func $f)
    (global (ref $grouped) (ref.func $f)))
  "type mismatch")

(assert_invalid
  (module
    (type $a (array i8))
    (func (export "_start") unreachable array.get $a drop))
  "array is packed")

(assert_invalid
  (module
    (type $s (struct))
    (func (export "_start") unreachable call_ref $s))
  "type mismatch")

;; Conditional branch fallthrough reifies the label's declared prefix type.
;; It must not retain ref.func's narrower type and thereby permit this call.
(assert_invalid
  (module
    (type $f (func))
    (func $target (type $f))
    (elem declare func $target)
    (func $typed_sink (param (ref $f)))
    (func (export "_start")
      (block (result funcref)
        ref.func $target
        ref.null func
        br_on_null 0
        drop
        call $typed_sink
        ref.null func)
      drop))
  "type mismatch")

(assert_invalid
  (module
    (type $base (sub (struct (field (mut i32)))))
    (type $child (sub $base (struct (field (mut i64))))))
  "type mismatch")
