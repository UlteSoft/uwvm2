(module
  (global $keep (mut i32) (i32.const 1))
  (global $count (mut i32) (i32.const 0))
  (global $last (mut i32) (i32.const 0))
  (func $leaf (export "leaf") (param i32) (result i32)
    local.get 0 i32.const 7 i32.add)
  (func $recursive (export "recursive") (param i32) (result i32)
    local.get 0 i32.eqz
    if (result i32) i32.const 5 call $leaf
    else local.get 0 i32.const 1 i32.sub call $recursive i32.const 1 i32.add end)
  (func (export "_start")
    ;; Repeated entry lets the real launcher attach before a finite return.
    (loop $again
      i32.const 2 call $recursive global.set $last
      global.get $count i32.const 1 i32.add global.set $count
      global.get $keep br_if $again)))
