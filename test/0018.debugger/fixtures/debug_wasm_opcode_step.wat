(module
  (type $result (func (result i32)))
  (table 1 funcref)
  (elem (i32.const 0) $leaf)
  (tag $boom (param i32))
  (func $leaf (export "leaf") (type $result) i32.const 7)
  (func $tail (export "tail") (type $result) return_call $leaf)
  (func $indirect (export "indirect") (type $result)
    i32.const 0 return_call_indirect (type $result))
  (func $reference (export "reference") (type $result)
    ref.func $leaf return_call_ref $result)
  (func $recursive (export "recursive") (param i32) (result i32)
    local.get 0 i32.eqz
    if (result i32)
      i32.const 7
    else
      local.get 0 i32.const 1 i32.sub call $recursive
    end)
  (func $thrower (result i32) i32.const 99 throw $boom)
  (func $catcher (export "catcher") (type $result)
    block (result i32)
      try_table (result i32) (catch $boom 0)
        call $thrower
      end
    end)
  (func (export "_start")
    call $leaf drop
    call $tail drop
    call $indirect drop
    call $reference drop
    i32.const 2 call $recursive drop
    call $catcher drop
    call $leaf drop))
