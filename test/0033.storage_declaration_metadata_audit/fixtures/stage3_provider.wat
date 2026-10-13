(module
 (table (export "extern_table") 0 externref)
 (table (export "mvp_table") 0 funcref)
 (global (export "vector") v128 (v128.const i32x4 1 2 3 4))
 (global (export "reference") externref (ref.null extern)))
