;; Genuine producer/runtime display witness: the dynamic loop preserves a
;; physical direct branch within this function. No mock executable code.
(module
  (type $box (struct (field i32)))
  (func $callee (param i32) (result i32)
    local.get 0 i32.const 13 i32.add)
  (func $caller (param i32) (result i32)
    (local $uninitialized (ref $box))
    (local $numeric i32) (local $remaining i32)
    local.get 0 i32.const 41 i32.xor local.set $numeric
    local.get 0 i32.const 7 i32.and local.set $remaining
    loop $rotate
      local.get $numeric i32.const 1 i32.rotl local.set $numeric
      local.get $remaining i32.const 1 i32.sub local.tee $remaining br_if $rotate
    end
    local.get $numeric return_call $callee))
