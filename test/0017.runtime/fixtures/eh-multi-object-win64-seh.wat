;; Multiple independent Core 3 handlers must survive partitioned COFF loading.
;; Mutable-table calls prevent O3 from inlining the cross-function throw paths.
;; The third tag carries a real GC reference, excluding numeric pending-island
;; dispatch. Each catch checks the exact payload, including reference identity.
(module
  (type $void (func))
  (type $cell (struct (field i32)))
  (table 6 funcref)
  (elem (i32.const 0) $raise_a $relay_a $raise_b $relay_b $raise_c $relay_c)
  (global $payload (ref $cell) (struct.new $cell (i32.const 97)))
  (tag $a (param i32))
  (tag $b (param i32))
  (tag $c (param (ref $cell)))
  (func $raise_a i32.const 31 throw $a)
  (func $relay_a i32.const 0 call_indirect (type $void))
  (func $raise_b i32.const 47 throw $b)
  (func $relay_b i32.const 2 call_indirect (type $void))
  (func $raise_c global.get $payload throw $c)
  (func $relay_c i32.const 4 call_indirect (type $void))
  (func $catch_a (result i32)
    (block $caught (result i32)
      try_table (catch $a $caught)
        i32.const 1 call_indirect (type $void)
      end
      unreachable))
  (func $catch_b (result i32)
    (block $caught (result i32)
      try_table (catch $b $caught)
        i32.const 3 call_indirect (type $void)
      end
      unreachable))
  (func $catch_c (result i32)
    (local $value (ref $cell))
    (block $caught (result (ref $cell))
      try_table (catch $c $caught)
        i32.const 5 call_indirect (type $void)
      end
      unreachable)
    local.set $value
    local.get $value global.get $payload ref.eq
    i32.eqz if unreachable end
    local.get $value struct.get $cell 0)
  (func (export "_start")
    call $catch_a i32.const 31 i32.ne if unreachable end
    call $catch_b i32.const 47 i32.ne if unreachable end
    call $catch_c i32.const 97 i32.ne if unreachable end))
