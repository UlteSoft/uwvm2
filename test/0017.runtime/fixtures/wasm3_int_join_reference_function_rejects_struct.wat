(module
  (type $box (struct (field i32)))
  (func $bad (param funcref) (result i32)
    local.get 0 i32.const 0 if else end struct.get $box 0)
  (func (export "_start") ref.null func call $bad drop))
