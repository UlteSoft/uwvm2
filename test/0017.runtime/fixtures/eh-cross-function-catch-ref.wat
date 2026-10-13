;; A Core 3 exception crosses one indirect Wasm call, is caught as a retained
;; exnref, then is thrown again through another indirect call. The outer
;; try_table must receive the original i32 payload after two guest unwinds.
(module
  (type $void (func))
  (tag $event (param i32))
  (table 2 funcref)
  (elem (i32.const 0) $raise $rethrow)
  (func $raise
    i32.const 37
    throw $event)
  (func $rethrow
    (local $caught_exn (ref null exn))
    (block $caught (result i32 (ref exn))
      try_table (catch_ref $event $caught)
        i32.const 0
        call_indirect (type $void)
      end
      unreachable)
    local.set $caught_exn
    i32.const 37
    i32.ne
    if unreachable end
    local.get $caught_exn
    ref.as_non_null
    throw_ref)
  (func (export "_start")
    (block $outer (result i32)
      try_table (catch $event $outer)
        i32.const 1
        call_indirect (type $void)
      end
      unreachable)
    i32.const 37
    i32.ne
    if unreachable end))
