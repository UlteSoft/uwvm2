(module
  (global $keep (mut i32) (i32.const 1))
  (global $count (mut i32) (i32.const 0))
  (func $leaf (param i32) (result i32) local.get 0 i32.const 7 i32.add)
  (func $recursive (param i32) (result i32)
    local.get 0 i32.eqz
    if (result i32) i32.const 5 call $leaf
    else local.get 0 i32.const 1 i32.sub call $recursive i32.const 1 i32.add end)
  (func $root (result i32) i32.const 2 call $recursive)
  (func $spin_root (result i32) call $spin)
  (func $spin (result i32)
    (loop $again
      ;; A real typed Wasm call retains a NI event before cancelled finish.
      i32.const 0 call $leaf drop
      global.get $count i32.const 1 i32.add global.set $count
      global.get $keep br_if $again)
    i32.const 77))
