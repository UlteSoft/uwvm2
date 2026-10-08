(module
  (func $signed (param i32) (result i32) local.get 0 ref.i31 i31.get_s)
  (func $unsigned (param i32) (result i32) local.get 0 ref.i31 i31.get_u)
  (func $dead i32.const 0 if unreachable ref.i31 i31.get_s drop end)
  (func $prefix (result i32) i32.const 23 i32.const -1 ref.i31 i31.get_u i32.add)
  (func (export "_start")
    i32.const -1 call $signed i32.const -1 i32.ne if unreachable end
    i32.const -1 call $unsigned i32.const 2147483647 i32.ne if unreachable end
    i32.const -2147483648 call $signed i32.eqz i32.eqz if unreachable end
    i32.const 1073741824 call $signed i32.const -1073741824 i32.ne if unreachable end
    i32.const 1073741824 call $unsigned i32.const 1073741824 i32.ne if unreachable end
    call $prefix i32.const -2147483626 i32.ne if unreachable end
    call $dead))
