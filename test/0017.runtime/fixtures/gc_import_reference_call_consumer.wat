;; The consumer's $record is index 0, while the provider's is index 1.
(module
  (type $record (struct (field i32)))
  (type $take (func (param (ref $record)) (result i32)))
  (import "P" "t" (table 1 funcref))
  (import "P" "take" (func $take (type $take)))
  (elem declare func $take)
  (func $tail_indirect (param (ref $record)) (result i32)
    local.get 0
    i32.const 0
    return_call_indirect (type $take))
  (func $tail_ref (param (ref $record)) (result i32)
    local.get 0
    ref.func $take
    return_call_ref $take)
  (func (export "_start")
    i32.const 42
    struct.new $record
    i32.const 0
    call_indirect (type $take)
    i32.const 42
    i32.ne
    if unreachable end
    i32.const 42
    struct.new $record
    ref.func $take
    call_ref $take
    i32.const 42
    i32.ne
    if unreachable end
    i32.const 42
    struct.new $record
    call $tail_indirect
    i32.const 42
    i32.ne
    if unreachable end
    i32.const 42
    struct.new $record
    call $tail_ref
    i32.const 42
    i32.ne
    if unreachable end))
