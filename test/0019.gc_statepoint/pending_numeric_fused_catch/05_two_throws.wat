(module
  (tag $event (param i32))
  (func $thrower (param i32) (result i32) local.get 0 throw $event)
  (func $catcher (param i32) (result i32)
    (block $hit (result i32)
      (try_table (result i32) (catch $event $hit)
        local.get 0 call $thrower)))
  (func (export "run") (result i32)
    i32.const 11 call $catcher
    i32.const 22 call $catcher
    i32.add)
)
