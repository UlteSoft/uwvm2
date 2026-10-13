;; Actual modern Wasm fixture: roots, mem64 and cross-function EH are executed.
;; These source bytes are not a hand-built native code substitute.
(module
  (type $box (struct (field i32)))
  (tag $event (param i32))
  (memory i64 1 1)
  (func $callee (param i32) (result i32)
    local.get 0
    i32.const 13
    i32.add)
  (func $caller (param i32) (result i32)
    (local $root (ref $box))
    (local $numeric i32)
    local.get 0
    i32.const 41
    i32.xor
    local.set $numeric
    i32.const 5
    struct.new $box
    local.set $root
    local.get $numeric
    local.get $root
    struct.get $box 0
    i32.add
    local.set $numeric
    i64.const 0
    local.get $numeric
    i32.store
    i64.const 0
    i32.load
    call $callee
    call $tail
    local.set $numeric
    block $caught (result i32)
      try_table (result i32) (catch $event $caught)
        local.get 0
        call $raise
      end
    end
    local.get $numeric
    i32.add)
  (func $tail (param i32) (result i32)
    local.get 0
    return_call $callee)
  (func $raise (param i32) (result i32)
    local.get 0
    i32.const 0
    i32.lt_s
    if (result i32)
      i32.const 9
      throw $event
    else
      local.get 0
      i32.const 2
      i32.add
    end))
