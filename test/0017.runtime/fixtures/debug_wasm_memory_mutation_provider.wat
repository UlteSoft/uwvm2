;; Pure Wasm provider: no WASI/native/DL import, exported memory aliases.
(module
  (memory $plain (export "plain") 1 2)
  (memory $shared (export "shared") 1 2 shared)
  (func $setup
    i32.const 0 i32.const 161 i32.store8 $plain
    i32.const 1 i32.const 178 i32.store8 $plain
    i32.const 63 i32.const 17 i32.store8 $plain
    i32.const 64 i32.const 90 i32.const 5 memory.fill $plain
    i32.const 69 i32.const 34 i32.store8 $plain
    i32.const 65535 i32.const 125 i32.store8 $plain
    i32.const 255 i32.const 17 i32.store8 $plain
    i32.const 256 i32.const 90 i32.const 256 memory.fill $plain
    i32.const 512 i32.const 34 i32.store8 $plain
    i32.const 0 i32.const 161 i32.store8 $plain
    i32.const 1 i32.const 178 i32.store8 $plain
    i32.const 79 i32.const 17 i32.store8 $plain
    i32.const 80 i32.const 90 i32.const 5 memory.fill $plain
    i32.const 85 i32.const 34 i32.store8 $plain
    i32.const 65535 i32.const 125 i32.store8 $plain
    i32.const 0 i32.const 161 i32.store8 $shared
    i32.const 1 i32.const 178 i32.store8 $shared
    i32.const 63 i32.const 17 i32.store8 $shared
    i32.const 64 i32.const 90 i32.const 5 memory.fill $shared
    i32.const 69 i32.const 34 i32.store8 $shared
    i32.const 65535 i32.const 125 i32.store8 $shared)
)
