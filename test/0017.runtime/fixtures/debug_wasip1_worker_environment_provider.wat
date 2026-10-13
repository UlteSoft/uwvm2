(module
  (type $node (struct (field i32)))
  (import "wasi_snapshot_preview1" "args_sizes_get" (func (param i32 i32) (result i32)))
  (memory (export "memory") 1)
  (func (export "worker_chain_leaf") (result i32)
    (local $root (ref $node))
    i32.const 17 struct.new $node local.set $root
    nop nop nop
    local.get $root struct.get $node 0 i32.const 74 i32.add))
