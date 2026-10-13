(module
  (type $result (func (result i32)))
  (memory 1)
  (global $sum i32 (i32.mul (i32.add (i32.const -1) (i32.const 2)) (i32.const 3)))
  (global $previous i32 (i32.sub (global.get $sum) (i32.const 7)))
  (global $wrap i64 (i64.mul (i64.const -1) (i64.const 2)))
  (func $leaf (type $result) global.get $previous)
  (table 3 funcref (ref.func $leaf))
  (data (i32.mul (global.get $sum) (i32.const 7)) "\ab")
  (func (export "_start")
    i32.const 2 call_indirect (type $result) i32.const -4 i32.ne
    if unreachable end
    i32.const 21 i32.load8_u i32.const 171 i32.ne
    if unreachable end
    global.get $wrap i64.const -2 i64.ne
    if unreachable end
    ;; The initializer alone declares this function reference for body validation.
    ref.func $leaf drop))
