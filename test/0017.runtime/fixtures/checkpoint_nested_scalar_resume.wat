(module
  (tag $number (param i32))
  (global $prefix (mut i32) (i32.const 0))
  (global $after (mut i32) (i32.const 0))
  (func (export "run") (result i32) (local $n i32)
    global.get $prefix i32.const 1 i32.add global.set $prefix
    block $outer (result i32)
      i32.const 3
      loop $iteration (param i32) (result i32)
        local.tee $n nop i32.const 1 i32.sub local.tee $n
        local.get $n br_if $iteration i32.const 4 i32.add
      end
      i32.const 1
      if (param i32) (result i32)
        nop i32.const 10 i32.add
      else
        nop i32.const 20 i32.add
      end
      block $caught (result i32)
        try_table (catch $number $caught)
          nop i32.const 73 throw $number
        end unreachable
      end
      i32.add
    end
    global.get $after i32.const 1 i32.add global.set $after)
  (func (export "prefix") (result i32) global.get $prefix)
  (func (export "after") (result i32) global.get $after))
