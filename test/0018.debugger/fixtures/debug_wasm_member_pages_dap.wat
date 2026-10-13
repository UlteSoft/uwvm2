(module
  (type $items (array (mut i32)))
  (type $box (struct (field (mut (ref null $items)))))
  (global $keep (mut i32) (i32.const 1))
  (global $counter (mut i32) (i32.const 0))
  (global $result (mut i32) (i32.const 0))
  (func $spin (export "spin") (result i32)
    (local $i i32)
    (local $items (ref null $items))
    (local $box (ref null $box))
    i32.const 161
    array.new_default $items
    local.set $items
    (loop $fill
      local.get $items
      local.get $i
      local.get $i
      i32.const 1000
      i32.add
      array.set $items
      local.get $i
      i32.const 1
      i32.add
      local.tee $i
      i32.const 161
      i32.lt_u
      br_if $fill)
    local.get $items
    struct.new $box
    local.set $box
    (loop $running
      global.get $counter
      i32.const 1
      i32.add
      global.set $counter
      global.get $keep
      br_if $running)
    local.get $items
    i32.const 159
    array.get $items)
  (func (export "_start") call $spin global.set $result))
