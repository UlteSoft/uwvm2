;; The sibling with an i64 field is not canonically equivalent to the
;; otherwise identical i32-field siblings; ref.cast must trap.
(module
  (type $base (sub (struct)))
  (type $left (sub $base (struct (field i32))))
  (type $right (sub $base (struct (field i32))))
  (type $different (sub $base (struct (field i64))))
  (table 1 (ref null struct))
  (func (export "_start")
    (table.set (i32.const 0) (struct.new_default $left))
    (drop (ref.cast (ref $different) (table.get (i32.const 0))))
  )
)
