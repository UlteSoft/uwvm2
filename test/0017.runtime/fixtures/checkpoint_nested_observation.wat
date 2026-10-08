(module
  (tag $number (param i32))
  ;; Main is function0. All callees are forward references, no host imports.
  (func (export "run") (result i32)
    call $nested call $tagged i32.add call $tagged_ref i32.add
    call $all i32.add call $all_ref i32.add)
  (func $child (param i32) (result i32) local.get 0 i32.const 10 i32.add)
  (func $nested (result i32) (local $n i32)
    block $outer (result i32)
      i32.const 3
      loop $iteration (param i32) (result i32)
        local.tee $n i32.const 1 i32.sub local.tee $n
        local.get $n br_if $iteration i32.const 4 i32.add
      end
      i32.const 1
      if (param i32) (result i32)
        call $child
      else
        i32.const 20 i32.add
      end
    end)
  (func $thrower (result i32) i32.const 73 throw $number)
  (func $tagged (result i32)
    block $caught (result i32)
      try_table (catch $number $caught) call $thrower drop end
      unreachable
    end
    i32.const 1 i32.add)
  (func $tagged_ref (result i32) (local $held (ref null exn))
    block $caught (result i32 (ref exn))
      try_table (catch_ref $number $caught) call $thrower drop end
      unreachable
    end
    local.set $held i32.const 2 i32.add)
  (func $all (result i32)
    block $caught
      try_table (catch_all $caught) call $thrower drop end unreachable
    end i32.const 76)
  (func $all_ref (result i32) (local $held (ref null exn))
    block $caught (result (ref exn))
      try_table (catch_all_ref $caught) call $thrower drop end unreachable
    end
    local.set $held
    block $rethrown (result i32)
      try_table (catch $number $rethrown) local.get $held throw_ref end unreachable
    end
    i32.const 4 i32.add))
