(module
  (type $cell (struct (field (mut i32))))
  (tag $e (param i32))
  (memory (export "memory") 1)
  (func (export "_start") (local $cell (ref null $cell)) (local $counter i32)
    i32.const 0 struct.new $cell local.set $cell
    (loop $loop
      local.get $cell ref.as_non_null local.get $counter struct.set $cell 0
      (block $caught (result i32)
        (try_table (catch $e $caught) local.get $counter throw $e)
        unreachable)
      drop
      local.get $counter i32.const 1 i32.add local.set $counter
      br $loop)))
