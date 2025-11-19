# WASM Import
Customizable to provide external imports (native code) for wasm to use.

The `imported` subsystem contains host bindings that can be attached to UWVM2 instances to expose operating-system and runtime functionality to
WebAssembly modules. One of the key components is the implementation of the WebAssembly System Interface Preview 1 (WASI P1):

- **WASI P1 host layer.** A spec-aligned WASI Preview 1 implementation for `wasm32-wasip1` and `wasm64-wasip1` targets, sharing the same
  cross-platform runtime as UWVM2 (from DOS-style environments through POSIX systems to modern Windows). See
  [wasi/wasip1/readme.md](wasi/wasip1/readme.md) for architecture and usage details.