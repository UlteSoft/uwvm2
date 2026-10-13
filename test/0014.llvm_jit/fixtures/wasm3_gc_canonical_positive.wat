;; Same-module canonical equivalence: $left and $right have identical supertypes
;; and fields, while $different has a distinct field type. Derived from the
;; Core 3 testsuite ref_cast.wast canonical-cast case.
(module
  (type $base (sub (struct)))
  (type $left (sub $base (struct (field i32))))
  (type $right (sub $base (struct (field i32))))
  (type $different (sub $base (struct (field i64))))
  (table 1 (ref null struct))
  (func (export "_start")
    (table.set (i32.const 0) (struct.new_default $left))
    ;; A value allocated as $left must cast to equivalent $right.
    (drop (ref.cast (ref $right) (table.get (i32.const 0))))
    ;; ref.test follows the same canonical equivalence relation.
    (if (i32.ne
          (ref.test (ref $right) (table.get (i32.const 0)))
          (i32.const 1))
      (then unreachable))
    ;; An inequivalent sibling remains rejected by ref.test.
    (if (ref.test (ref $different) (table.get (i32.const 0)))
      (then unreachable))
  )
)
