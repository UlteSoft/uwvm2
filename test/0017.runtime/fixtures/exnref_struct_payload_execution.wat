;; Core 3: a GC reference in a tag payload must preserve its full kind and
;; object lifetime through catch_ref and throw_ref, including a second catch.
(module
  (type $record (struct (field i32)))
  (tag $t (param (ref $record)))
  (func (export "_start")
    block $outer (result (ref $record))
      try_table (catch $t $outer)
        block $inner (result (ref $record) (ref exn))
          try_table (catch_ref $t $inner)
            i32.const 42
            struct.new $record
            throw $t
          end
          unreachable
        end
        throw_ref
      end
      unreachable
    end
    struct.get $record 0
    i32.const 42
    i32.ne
    if unreachable end))
