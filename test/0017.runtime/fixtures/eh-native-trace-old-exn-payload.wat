;; A wrapper caught without a ref still carries an independently owned older exnref.
;; Escaping that older ref must report $initial, not $rewrap, as its throw origin.
(module
  (tag $original (param i32))
  (tag $wrapper (param exnref))
  (func $initial
    i32.const 43
    throw $original)
  (func $rewrap (param exnref)
    local.get 0
    throw $wrapper)
  (func (export "_start") (local $old (ref null exn))
    block $wrapped (result exnref)
      try_table (catch $wrapper $wrapped)
        block $caught (result i32 (ref exn))
          try_table (catch_ref $original $caught)
            call $initial
          end
          unreachable
        end
        local.set $old
        i32.const 43
        i32.ne
        if unreachable end
        local.get $old
        call $rewrap
      end
      unreachable
    end
    throw_ref))
