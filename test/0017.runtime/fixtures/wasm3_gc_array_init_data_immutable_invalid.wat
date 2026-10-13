(module
  (type $array (array i16))
  (data $segment "\01\02\03\04")
  (func $probe
    unreachable ref.null $array i32.const 0 i32.const 0 i32.const 0
    array.init_data $array $segment)
  (func (export "_start") call $probe))
