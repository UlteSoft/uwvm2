(module
  (type $target (func))
  (type $array (array (ref null $target)))
  (func $value (type $target))
  (elem $segment (ref $target) (ref.func $value))
  (func $probe
    unreachable ref.null $array i32.const 0 i32.const 0 i32.const 0
    array.init_elem $array $segment)
  (func (export "_start") call $probe))
