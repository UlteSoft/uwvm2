;; Relay module: re-export the main module's "hot" function as an imported alias.
(module
  (type $unary_i32 (func (param i32) (result i32)))
  (import "call_fastpath_cycle_main" "hot" (func $hot (type $unary_i32)))
  (export "hot_alias" (func $hot))
)
