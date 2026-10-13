(module
  (type $a (array anyref))
  (elem eqref (ref.null eq))
  (func (export "_start")
    i32.const 0
    i32.const 0
    array.new_elem $a 0
    drop))
