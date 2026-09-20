(module
  (type $unary (func (param v128) (result v128)))
  (memory 1)
  (table 1 funcref)
  (global $g (mut v128) (v128.const i32x4 1 2 3 4))
  (elem (i32.const 0) $inc)
  (func $inc (type $unary) (param $v v128) (result v128)
    local.get $v v128.const i32x4 1 1 1 1 i32x4.add)
  (func $twice (type $unary) (param $v v128) (result v128)
    local.get $v call $inc call $inc)
  (func $pair (param $a v128) (param $b v128) (result v128 v128)
    local.get $b local.get $a)
  (func (export "_start") (local $n i32) (local $v v128)
    global.get $g call $twice i32.const 0 call_indirect (type $unary) global.set $g
    global.get $g v128.const i32x4 4 5 6 7 i32x4.eq i8x16.all_true i32.eqz if unreachable end
    v128.const i32x4 1 1 1 1 v128.const i32x4 2 2 2 2 call $pair
    i32x4.sub v128.const i32x4 1 1 1 1 i32x4.eq i8x16.all_true i32.eqz if unreachable end
    i32.const 3 local.set $n
    v128.const i32x4 0 0 0 0
    (loop $L (param v128) (result v128)
      v128.const i32x4 1 1 1 1 i32x4.add
      local.get $n i32.const 1 i32.sub local.tee $n br_if $L)
    local.set $v
    i32.const 65520 local.get $v v128.store
    i32.const 65520 v128.load v128.const i32x4 3 3 3 3 i32x4.eq
    i8x16.all_true i32.eqz if unreachable end
    local.get $v v128.const i32x4 99 99 99 99 i32.const 1 select (result v128)
    local.get $v i32x4.eq i8x16.all_true i32.eqz if unreachable end))
