(module
  (global $prefix (mut i32) (i32.const 17))
  (tag $event (param i32))
  (func (result i32)
    i32.const 0 call $arm i32.const 1 call $arm i32.add
    call $loop i32.add call $caught i32.add)
  (func $arm (param $condition i32) (result i32)
    global.get $prefix local.get $condition
    if (result i32) i32.const 4 else i32.const 4 end
    i32.add)
  (func $loop (result i32) (local $iteration i32)
    global.get $prefix
    loop (result i32)
      local.get $iteration i32.const 1 i32.add local.tee $iteration
      i32.const 3 i32.lt_u br_if 0
      i32.const 4
    end
    i32.add)
  (func $caught (result i32)
    global.get $prefix
    block (result i32)
      try_table (catch $event 0)
        i32.const 4 throw $event
      end
      unreachable
    end
    i32.add))
