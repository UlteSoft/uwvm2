(module
  (type $array (array v128))
  (func $probe (param $execute i32)
    local.get $execute if
      ;; Polymorphism cannot hide the known i32 at the top of a v128 operand sequence.
      unreachable select i32.const 7 array.new_fixed $array 65536 drop
    end)
  (func (export "_start") i32.const 0 call $probe))
