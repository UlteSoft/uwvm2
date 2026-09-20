(module
  (type $v (func))
  (type $i (func (param i32)))
  (memory 1)

  (func $leaf (type $i) (param $trap i32)
    local.get $trap
    if
      i32.const -1
      i32.load
      drop
    end)

  (func $_start (export "_start") (type $v)
    (local $i i32)
    i32.const 0
    local.set $i
    block $exit
      loop $hot
        local.get $i
        i32.const 2000000
        i32.ge_u
        br_if $exit
        i32.const 0
        call $leaf
        local.get $i
        i32.const 1
        i32.add
        local.set $i
        br $hot
      end
    end
    i32.const 1
    call $leaf))
