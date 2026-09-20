(module
  (type $result_i32 (func (result i32)))
  (table $table0 1 funcref)
  (table $table1 1 funcref)

  (func $answer (type $result_i32)
    i32.const 42)

  (elem (table $table1) (i32.const 0) func $answer)

  (func $_start (export "_start")
    i32.const 0
    call_indirect $table1 (type $result_i32)
    i32.const 42
    i32.ne
    if
      unreachable
    end))
