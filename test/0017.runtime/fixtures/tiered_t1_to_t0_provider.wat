(module
  (tag $event (export "event"))
  (func (export "cold") (param $raise i32) (result i32)
    local.get $raise
    if
      throw $event
    end
    i32.const 0))
