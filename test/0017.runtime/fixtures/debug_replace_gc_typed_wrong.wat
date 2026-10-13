;; Valid on its own, but its body requires $other. The original target's $s
;; parameter has the same raw reference carrier and a different Core 3 heap type.
(module
  (type $s (struct (field i32)))
  (type $other (struct (field i64)))
  (func $target (param (ref null $other)) (result i32)
    local.get 0
    struct.get $other 0
    i32.wrap_i64))
