# WebAssembly object layer

The `uwvm2::object` subsystem contains **runtime representations** of WebAssembly constructs (e.g., globals, tables, and linear memories). It is the boundary where validated module information becomes concrete, long-lived runtime state.

## Why we “decay” from the parser’s concepts

uwvm2’s parser uses a concepts-driven, feature-extensible architecture to decode and validate **untrusted** binary input at high speed. That approach is ideal for parsing because it enables feature-specific decoding and validation to be composed without rewriting the core parser.

The object layer (and the initializer/instantiation pipeline feeding it) intentionally does **not** reuse that concepts-heavy model. Instead, it performs a *decay step*: it converts feature-parameterized parse results into a small set of canonical, runtime-oriented structures.

This is recommended for the object layer because:

- **Different invariants**: parsing is dominated by bounds checks and byte-level safety; runtime objects are dominated by semantic invariants (addresses/handles, mutability rules, lifetime, concurrency).
- **Forward compatibility and stable APIs**: object types should remain stable as features evolve; embedding parser feature parameters into runtime object types would leak feature combinatorics into the runtime API/ABI.
- **Build-time and complexity control**: reusing feature-parameterized templates across runtime code increases compile time, binary size, and debug complexity, with little benefit to runtime semantics.
- **Explicit ownership**: parsers often keep views into module bytes (zero-copy spans). The runtime must decide whether to copy, retain, or drop data; a decayed representation makes these policies explicit.

In practice, “decay” means the runtime stores only what it needs to execute: concrete object fields, stable handles/indices, and minimal metadata required by the semantics, while keeping the parser free to evolve via concepts.

## Memory model

For an overview of the host memory models and linear memory backends used by the object layer, see `src/uwvm2/object/memory/readme.md`.
