(module
  (type $array (array i16))
  (data $segment "\01\02\03\04")
  (func $probe (result i32)
    i32.const 1 i32.const 1 array.new_data $array $segment
    i32.const 0 array.get_u $array)
  (func (export "_start") call $probe i32.const 770 i32.ne if unreachable end))
