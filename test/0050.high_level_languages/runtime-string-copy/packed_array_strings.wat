(module
  ;; A producer-style immutable UTF-16 source copied into a mutable string buffer.
  (type $utf16 (array i16))
  (type $buffer (array (mut i16)))
  (data $text "\61\00\62\00\63\00")
  (func $require (param $ok i32)
    (if (i32.eqz (local.get $ok)) (then unreachable)))
  (func (export "_start")
    (local $source (ref null $utf16))
    (local $target (ref null $buffer))
    (local.set $source (array.new_data $utf16 $text (i32.const 0) (i32.const 3)))
    (call $require (i32.eq (array.len (local.get $source)) (i32.const 3)))
    (call $require (i32.eq (array.get_u $utf16 (local.get $source) (i32.const 1)) (i32.const 98)))
    (local.set $target (array.new_default $buffer (i32.const 4)))
    (array.copy $buffer $utf16
      (local.get $target) (i32.const 1) (local.get $source) (i32.const 0) (i32.const 3))
    (call $require (i32.eq (array.get_u $buffer (local.get $target) (i32.const 2)) (i32.const 98)))
    ;; Data offsets are bytes; destination offsets and counts are elements.
    (array.init_data $buffer $text (local.get $target) (i32.const 0) (i32.const 4) (i32.const 1))
    (call $require (i32.eq (array.get_u $buffer (local.get $target) (i32.const 0)) (i32.const 99)))
    (array.fill $buffer (local.get $target) (i32.const 3) (i32.const 123) (i32.const 1))
    (call $require (i32.eq (array.get_u $buffer (local.get $target) (i32.const 3)) (i32.const 123)))
    (array.copy $buffer $buffer
      (local.get $target) (i32.const 4) (local.get $target) (i32.const 4) (i32.const 0))))
