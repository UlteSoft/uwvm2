(module
  (type $cell (struct (field (mut i32))))
  (func (export "_start") (result i32) (local $held (ref $cell))
    call $producer
    local.set $held
    local.get $held
    call $consumer
    local.get $held
    struct.get $cell 0
    i32.add)
  (func $producer (result (ref $cell))
    i64.const 99
    nop
    drop
    i32.const 42
    struct.new $cell)
  (func $consumer (param $input (ref $cell)) (result i32) (local $alias (ref $cell))
    local.get $input
    local.set $alias
    local.get $input
    nop
    struct.get $cell 0))
