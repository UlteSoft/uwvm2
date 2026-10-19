(module
  (global $left (mut v128) (v128.const i64x2 123456789 9223372036854775807))
  (global $right (mut v128) (v128.const i64x2 987654321 123456789))
  (func (export "_start")
    global.get $left
    global.get $right
    i64x2.mul
    drop))
