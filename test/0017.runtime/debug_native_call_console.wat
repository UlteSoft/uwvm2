(module
  (global $keep (mut i32) (i32.const 1))
  (func $recursive (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    local.get 0 i32.eqz
    if (result i32) i32.const 42
    else local.get 0 i32.const 1 i32.sub i32.const 0 i32.const 0 i32.const 0 i32.const 0 i32.const 0 i32.const 0 i32.const 0 i32.const 0 i32.const 0 call $recursive i32.const 1 i32.add end)
  (func $entry (result i32) i32.const 6 i32.const 0 i32.const 0 i32.const 0 i32.const 0 i32.const 0 i32.const 0 i32.const 0 i32.const 0 i32.const 0 call $recursive)
  (func $spin (result i32)
    (loop $again global.get $keep br_if $again)
    i32.const 77)
  (func $spin_entry (result i32) call $spin))
