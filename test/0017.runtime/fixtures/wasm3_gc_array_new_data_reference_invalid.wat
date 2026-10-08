(module
  (type $array (array (mut eqref)))
  (data $segment "\01\02\03\04")
  (func $probe
    unreachable i32.const 0 i32.const 0 array.new_data $array $segment drop)
  (func (export "_start") call $probe))
