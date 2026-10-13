(module
  (type $array (array v128))
  (func $probe (param $execute i32)
    local.get $execute if
      ;; One materialized Bot and missing Bot operands, not 65536 concrete values.
      unreachable select array.new_fixed $array 65536 drop
    end)
  (func (export "_start") i32.const 0 call $probe))
