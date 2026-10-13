(module
  (type $choice (func (param (ref null any)) (result (ref null any))))
  (func $bad (param (ref i31)) (result i32)
    local.get 0 i32.const 0 if (type $choice) else end i31.get_s)
  (func (export "_start") i32.const 47 ref.i31 call $bad drop))
