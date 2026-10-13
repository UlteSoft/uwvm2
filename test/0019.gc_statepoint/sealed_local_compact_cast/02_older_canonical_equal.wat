(module
  (type $old (struct (field i32)))
  (type $target (struct (field i32)))
  (table $roots 2 2 anyref)
  (func (export "run") (result i32)
    (local $left i32)
    i32.const 0
    i32.const 42
    struct.new $old
    table.set $roots
    i32.const 10000
    local.set $left
    block $done
      loop $fill
        i32.const 1
        local.get $left
        struct.new $target
        table.set $roots
        local.get $left
        i32.const 1
        i32.sub
        local.tee $left
        br_if $fill
      end
    end
    i32.const 0
    table.get $roots
    ref.cast (ref $target)
    struct.get $target 0)
)
