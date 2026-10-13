(module
  (func (export "f_v128") (param v128))
  (func (export "f_extern") (param externref))
  (func (export "f_multi") (result i32 i64) i32.const 0 i64.const 0)
  (global (export "g_v128") v128 (v128.const i32x4 0 0 0 0))
  (global (export "g_extern") externref (ref.null extern))
  (table (export "t_extern") 0 externref))
