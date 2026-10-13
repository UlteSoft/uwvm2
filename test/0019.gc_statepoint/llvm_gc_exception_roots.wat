;; Actual Core 3 instructions. No imported allocation or hand-written LLVM IR.
;; The C++ component invokes the raw exports by the indices listed below.
(module
  (type $box (struct (field (mut i32))))
  (tag $payload (param (ref $box)))
  (tag $other (param (ref $box)))
  (tag $number (param i32))

  ;; 0: The reference becomes an immutable exception payload in a native bridge.
  (func $throw-box (param $value (ref $box)) (result i32)
    local.get $value
    throw $payload)

  ;; 1: A typed payload must survive both native propagation and catch transfer.
  (func (export "plain-catch") (result i32)
    block $caught (result (ref $box))
      try_table (catch $payload $caught)
        i32.const 37
        struct.new $box
        call $throw-box
        drop
      end
      unreachable
    end
    struct.get $box 0)

  ;; 2: The same signature is insufficient: $other is a distinct tag instance.
  (func $inner-unmatched (param $value (ref $box)) (result i32)
    block $wrong (result (ref $box))
      try_table (catch $other $wrong)
        local.get $value
        call $throw-box
        drop
      end
      unreachable
    end
    struct.get $box 0)

  ;; 3: Inner unmatched cleanup must retire its frame before the outer handler.
  (func (export "unmatched-outer") (result i32)
    block $caught (result (ref $box))
      try_table (catch $payload $caught)
        i32.const 41
        struct.new $box
        call $inner-unmatched
        drop
      end
      unreachable
    end
    struct.get $box 0)

  ;; 4: catch_all discards the thrown payload, but this unrelated local remains.
  (func (export "catch-all-local") (result i32)
    (local $keep (ref null $box))
    i32.const 43
    struct.new $box
    local.set $keep
    block $caught
      try_table (catch_all $caught)
        i32.const 99
        struct.new $box
        call $throw-box
        drop
      end
    end
    local.get $keep
    ref.as_non_null
    struct.get $box 0)

  ;; 5: A true cross-function musttail must retire its own root record first.
  (func $musttail-thrower (param $value (ref $box)) (result i32)
    local.get $value
    return_call $throw-box)

  ;; 6: Native exception crosses the tail thrower and is caught by this frame.
  (func (export "catch-tail") (result i32)
    block $caught (result (ref $box))
      try_table (catch $payload $caught)
        i32.const 47
        struct.new $box
        call $musttail-thrower
        drop
      end
      unreachable
    end
    struct.get $box 0)

  ;; 7: Its unrelated local must be retired on an escaped exception path.
  (func $escape-root (param $value (ref $box)) (result i32)
    (local $keep (ref null $box))
    i32.const 53
    struct.new $box
    local.set $keep
    local.get $value
    call $inner-unmatched)

  ;; 8: C++ host catches the real guest_exception after all JIT frames leave.
  (func (export "native-escape") (result i32)
    i32.const 59
    struct.new $box
    call $escape-root)

  ;; 9: catch_ref and throw_ref preserve the SAME immutable exception instance.
  ;; The current aggregate-only collector must reject the live exn registry.
  (func (export "catch-ref-rethrow") (result i32)
    (local $exception (ref null exn))
    block $outer (result (ref $box))
      try_table (catch $payload $outer)
        block $captured (result (ref $box) (ref exn))
          try_table (catch_ref $payload $captured)
            i32.const 61
            struct.new $box
            call $throw-box
            drop
          end
          unreachable
        end
        local.set $exception
        drop
        local.get $exception
        throw_ref
      end
      unreachable
    end
    struct.get $box 0)

  ;; 10: A second musttail has a reference operand but no surviving caller.
  (func (export "native-tail-escape") (result i32)
    i32.const 67
    struct.new $box
    return_call $musttail-thrower)

  ;; 11: Precise roots enabled must still elide a truly reference-free record.
  (func (export "pure-numeric") (result i32)
    i32.const 9
    i32.const 8
    i32.add)

  ;; 12/13: Exercise the distinct exact-ABI numeric throw/copy adapter as well.
  (func $throw-number (param $value i32) (result i32)
    local.get $value
    throw $number)
  (func (export "numeric-catch") (result i32)
    block $caught (result i32)
      try_table (catch $number $caught)
        i32.const 71
        call $throw-number
        drop
      end
      unreachable
    end)
)
