(module
  (type $vector_pair (func (param v128) (result v128 i32)))
  (table 2 8 funcref)
  (elem $passive func $two $one)
  (func $one (type $vector_pair) (param v128) (result v128 i32)
    local.get 0 v128.const i32x4 1 1 1 1 i32x4.add i32.const 1)
  (func $two (type $vector_pair) (param v128) (result v128 i32)
    local.get 0 v128.const i32x4 2 2 2 2 i32x4.add i32.const 2)
  (func $check (param $index i32) (param $expected i32)
    v128.const i32x4 10 20 30 40 local.get $index call_indirect (type $vector_pair)
    local.get $expected i32.ne if unreachable end
    v128.const i32x4 10 20 30 40 local.get $expected i32x4.splat i32x4.add
    i32x4.eq i8x16.all_true i32.eqz if unreachable end)
  (func (export "_start")
    i32.const 0 ref.func $one table.set
    i32.const 0 i32.const 1 call $check
    i32.const 0 ref.func $two table.set
    i32.const 0 i32.const 2 call $check
    i32.const 1 i32.const 0 i32.const 1 table.copy
    i32.const 1 i32.const 2 call $check
    i32.const 0 ref.func $one i32.const 2 table.fill
    i32.const 0 i32.const 1 call $check
    i32.const 1 i32.const 1 call $check
    ref.func $two i32.const 3 table.grow i32.const 2 i32.ne if unreachable end
    i32.const 2 i32.const 2 call $check
    i32.const 4 i32.const 2 call $check
    i32.const 0 i32.const 0 i32.const 2 table.init $passive
    elem.drop $passive
    i32.const 0 i32.const 2 call $check
    i32.const 1 i32.const 1 call $check
    ;; Zero-size mutations must not disturb previously published native targets.
    i32.const 5 ref.null func i32.const 0 table.fill
    ref.null func i32.const 0 table.grow i32.const 5 i32.ne if unreachable end
    i32.const 4 i32.const 2 call $check
    i32.const 1 ref.null func table.set
    i32.const 1 table.get ref.is_null i32.eqz if unreachable end))
