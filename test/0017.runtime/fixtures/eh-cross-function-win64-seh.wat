;; A Core 3 throw crosses two generated Wasm calls before try_table catches it.
;; Mutable-table indirect calls prevent LLVM O3 from inlining away either
;; frame. A same-frame branch cannot satisfy the exact i32 payload check.
(module
  (type $void (func))
  (table 2 funcref)
  (elem (i32.const 0) $raise $relay)
  (tag $event (param i32))
  (func $raise
    i32.const 29
    throw $event)
  (func $relay
    i32.const 0
    call_indirect (type $void))
  (func (export "_start")
    (block $caught (result i32)
      try_table (catch $event $caught)
        i32.const 1
        call_indirect (type $void)
      end
      unreachable)
    i32.const 29
    i32.ne
    if unreachable end))
