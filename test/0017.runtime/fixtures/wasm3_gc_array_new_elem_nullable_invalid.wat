(module
  (type $base (sub (func (result i32))))
  (type $child (sub final $base (func (result i32))))
  (type $array (array (ref $base)))
  (func $target (type $child) i32.const 42)
  ;; Segment typing, rather than this particular non-null initializer, governs compatibility.
  (elem $segment (ref null $child) (ref.func $target))
  (func $probe
    unreachable i32.const 0 i32.const 1 array.new_elem $array $segment drop)
  (func (export "_start") call $probe))
