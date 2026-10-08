;; Actual emitted Wasm callers, including physically distinct recursive frames.
(module
  (func $leaf (param i32) (result i32)
    local.get 0 i32.const 7 i32.add)
  (func $recursive (param i32) (result i32)
    local.get 0 i32.eqz
    if (result i32)
      i32.const 5 call $leaf
    else
      local.get 0 i32.const 1 i32.sub call $recursive
      i32.const 1 i32.add
    end)
  (func $root (result i32)
    i32.const 2 call $recursive)
  (func $deep_root (result i32)
    i32.const 36 call $recursive))
