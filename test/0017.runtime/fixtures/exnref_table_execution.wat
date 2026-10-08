;; Core 3: a non-null exception reference survives table.set/get, grow,
;; fill, and copy before throw_ref restores its tag and payload.
(module
  (tag $t (param i32))
  (table 1 8 exnref)
  (func (export "_start")
    (local $saved (ref null exn))
    block $outer (result i32)
      try_table (catch $t $outer)
        block $inner (result i32 (ref exn))
          try_table (catch_ref $t $inner)
            i32.const 37
            throw $t
          end
          unreachable
        end
        local.set $saved
        drop
        i32.const 0
        local.get $saved
        table.set 0
        i32.const 0
        table.get 0
        ref.is_null
        if unreachable end
        local.get $saved
        i32.const 2
        table.grow 0
        i32.const 1
        i32.ne
        if unreachable end
        table.size 0
        i32.const 3
        i32.ne
        if unreachable end
        i32.const 1
        local.get $saved
        i32.const 2
        table.fill 0
        i32.const 2
        i32.const 0
        i32.const 1
        table.copy 0 0
        ref.null exn
        local.set $saved
        i32.const 2
        table.get 0
        local.set $saved
        i32.const 2
        ref.null exn
        table.set 0
        local.get $saved
        throw_ref
      end
      unreachable
    end
    i32.const 37
    i32.ne
    if unreachable end))
