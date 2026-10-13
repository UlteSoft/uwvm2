(module
  (type $destination (array (mut i16)))
  (type $source (array i8))
  (func $probe
    ;; i8 and i16 both unpack to i32 but their storage types do not match.
    unreachable ref.null $destination i32.const 0 ref.null $source i32.const 0 i32.const 0
    array.copy $destination $source)
  (func (export "_start") call $probe))
