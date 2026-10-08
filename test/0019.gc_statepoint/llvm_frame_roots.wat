;; Actual Core 3 syntax for the precise-frame lowering probe. Collection is
;; forced by a test-owned native debug callback; these are actual compiled
;; Wasm functions, with actual product GC objects, not hand-written shadow IR.
(module
  (type $box (struct (field (mut i32))))
  (type $read-type (func (param (ref $box)) (result i32)))
  (func $read (type $read-type)
    local.get 0
    struct.get $box 0)
  ;; The first box must survive only in the operand prefix during another
  ;; allocation and a nested call; the callee argument is a different box.
  (func (export "operand-prefix") (result i32)
    i32.const 37
    struct.new $box
    i32.const 13
    struct.new $box
    call $read
    drop
    struct.get $box 0)
  (func $tail (param (ref $box)) (result i32)
    local.get 0
    return_call $read)
  (func (export "cross-tail") (result i32)
    i32.const 41
    struct.new $box
    return_call $tail)
  (func $self (param (ref $box)) (param i32) (result i32)
    local.get 1
    i32.eqz
    if (result i32)
      local.get 0
      struct.get $box 0
    else
      local.get 0
      local.get 1
      i32.const 1
      i32.sub
      return_call $self
    end)
  (func (export "self-tail") (result i32)
    i32.const 43
    struct.new $box
    i32.const 9
    call $self)
  ;; A changed reference is carried by actual loop PHIs. Reusing an entry SSA
  ;; root on a backedge would free the new box before struct.get.
  (func (export "loop-phis") (result i32)
    (local $box (ref null $box))
    (local $remaining i32)
    i32.const 1
    struct.new $box
    i32.const 3
    loop (param (ref $box) i32) (result i32)
      local.set $remaining
      local.set $box
      local.get $remaining
      i32.eqz
      if
        local.get $box
        ref.as_non_null
        struct.get $box 0
        return
      end
      local.get $box
      ref.as_non_null
      struct.get $box 0
      i32.const 1
      i32.add
      struct.new $box
      local.get $remaining
      i32.const 1
      i32.sub
      br 0
    end)
  ;; The debugger callback immediately after struct.new must see the new SSA
  ;; value, even before local.set updates the local-slot copy.
  (func (export "instruction-new-root") (result i32)
    (local $box (ref null $box))
    i32.const 47
    struct.new $box
    local.set $box
    local.get $box
    ref.as_non_null
    struct.get $box 0)
  ;; Enabling collection must add no root record or root ABI calls to a
  ;; function whose complete typed snapshots contain no reference at all.
  (func (export "pure-numeric") (result i32)
    i32.const 9
    i32.const 8
    i32.add)
)
