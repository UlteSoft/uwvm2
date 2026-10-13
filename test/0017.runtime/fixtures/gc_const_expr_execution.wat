(module
  (type $cell (struct (field i32)))
  (type $arr (array i31ref))
  (type $ints (array i32))
  (elem $e i31ref
    (ref.i31 (i32.const 170))
    (ref.i31 (i32.const 187)))
  (global $cell_value (ref $cell)
    (struct.new $cell (i32.const 41)))
  (global $default_cell (ref $cell)
    (struct.new_default $cell))
  (global $fixed_value (ref $arr)
    (array.new_fixed $arr 2
      (ref.i31 (i32.const 2))
      (ref.i31 (i32.const 3))))
  (global $filled_value (ref $ints)
    (array.new $ints (i32.const 23) (i32.const 2)))
  (global $default_value (ref $ints)
    (array.new_default $ints (i32.const 2)))
  (global $external_value externref
    (extern.convert_any (ref.i31 (i32.const 29))))
  (global $roundtrip_value anyref
    (any.convert_extern
      (extern.convert_any (ref.i31 (i32.const 31)))))
  (func (export "_start")
    (if (i32.ne (struct.get $cell 0 (global.get $cell_value)) (i32.const 41))
      (then unreachable))
    (if (i32.ne (i31.get_u (array.get $arr (global.get $fixed_value) (i32.const 1))) (i32.const 3))
      (then unreachable))
    (if (i32.ne (struct.get $cell 0 (global.get $default_cell)) (i32.const 0))
      (then unreachable))
    (if (i32.ne (array.get $ints (global.get $filled_value) (i32.const 1)) (i32.const 23))
      (then unreachable))
    (if (i32.ne (array.get $ints (global.get $default_value) (i32.const 0)) (i32.const 0))
      (then unreachable))
    (if (i32.ne
          (i31.get_u (ref.cast (ref i31) (any.convert_extern (global.get $external_value))))
          (i32.const 29))
      (then unreachable))
    (if (i32.ne
          (i31.get_u (ref.cast (ref i31) (global.get $roundtrip_value)))
          (i32.const 31))
      (then unreachable))
    (if (i32.ne
          (i31.get_u (array.get $arr
            (array.new_elem $arr $e (i32.const 0) (i32.const 2))
            (i32.const 0)))
          (i32.const 170))
      (then unreachable))))
