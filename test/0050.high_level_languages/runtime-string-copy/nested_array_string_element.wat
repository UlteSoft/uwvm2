(module
  (type $utf16 (array i16))
  (type $chars (array (mut i32)))
  (data $text "\61\00\62\00\63\00")
  (func (export "_start")
    (local $source (ref null $utf16))
    (local $target (ref null $chars))
    (local.set $source (array.new_data $utf16 $text (i32.const 0) (i32.const 3)))
    (local.set $target (array.new_default $chars (i32.const 4)))
    ;; Keep the destination reference/index live under the source operation.
    (array.set $chars (local.get $target) (i32.const 2)
      (array.get_u $utf16 (local.get $source) (i32.const 1)))
    (if (i32.ne (array.get $chars (local.get $target) (i32.const 2)) (i32.const 98))
      (then unreachable))))
