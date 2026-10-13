(module
  (type $box (struct (field f32)))
  (table $roots 2 2 anyref)
  (func (export "run") (result i32)
    (local $left i32)
    i32.const 0
    i32.const 0x7fa12345
    f32.reinterpret_i32
    struct.new $box
    table.set $roots
    i32.const 10000
    local.set $left
    block $done
      loop $fill
        i32.const 1
        local.get $left
        f32.convert_i32_s
        struct.new $box
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
    ref.cast (ref $box)
    struct.get $box 0
    i32.reinterpret_f32)
)
