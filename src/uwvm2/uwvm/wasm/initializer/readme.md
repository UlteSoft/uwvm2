## Initializer design note: why we intentionally “decay” from the parser’s concepts

This directory implements WebAssembly *instantiation* (often called the initializer in uwvm2): binding imports, allocating runtime objects (globals/memories/tables/tags), evaluating constant expressions, applying active data/elem initializers, and optionally invoking the start function.

uwvm2’s parser is built around a concepts-based, feature-extensible architecture. That design is a good fit for parsing because the parser must safely decode **untrusted bytes** at high speed while allowing new features to be added without rewriting the existing decoding pipeline.

The initializer has a different problem statement and therefore intentionally does **not** reuse the parser’s concepts-heavy architecture. Instead, it performs a *decay step*: it converts feature-dependent parse results into a small set of canonical, runtime-oriented data structures.

---

### 1. Different invariants and threat model

- **Parser**: operates on attacker-controlled input. Memory safety and bounds validation dominate the design. The concepts framework helps inject feature-specific validation into a shared, zero-copy decoding skeleton.
- **Initializer**: consumes *validated* module structures (or other trusted sources such as preloaded modules). Its critical invariants are about **runtime semantics** (import resolution, address assignment, object lifetimes), not about byte-level decoding safety.

Keeping the initializer free of the parser’s concepts reduces accidental coupling between “byte decoding concerns” and “runtime semantics concerns”.

---

### 2. Different extension axis

The parser extends along the axis of *binary format features* (new section encodings, new immediate forms, new validation rules). Concepts are a natural way to layer those features without editing the core parser.

The initializer extends along the axis of *runtime state and semantics*:

- new runtime object kinds or representations (e.g., multi-memory, reference types, GC objects),
- new instantiation-time algorithms (linking policies, caching, snapshotting),
- new execution-time instructions that interact with stored state (e.g., bulk-memory’s `memory.init` / `data.drop`).

These extensions are better expressed as additive runtime data structures (tagged enums, small handles, stable “store/address” mappings) rather than as feature-parameterized template types.

---

### 3. Compilation cost, binary size, and layering

Reusing the parser’s feature-parameterized types inside the initializer would:

- amplify template instantiations across runtime code,
- increase compile time and object size,
- make debugger output and ABI boundaries harder to reason about,
- increase the risk of ODR and build-graph sensitivity (tiny feature changes forcing wide recompiles).

The initializer is performance-critical primarily at **runtime**. Plain, concrete types generally optimize better for runtime locality while keeping build times and symbol complexity under control.

---

### 4. Ownership and lifetime are initializer concerns

Parser structures often store views into the original module byte buffer (e.g., `begin/end` pointers). That is ideal for parsing and validation.

The initializer must make explicit decisions about:

- whether data is copied or kept zero-copy,
- how long backing buffers live (module file lifetime vs. instance lifetime),
- how “dropped” resources are represented (e.g., passive data segments after `data.drop`).

A decayed initializer representation makes these policies explicit and prevents parser-oriented views from leaking into runtime object ownership rules.

---

### 5. What “decay” means in practice

“Decay” means converting the parser’s rich, feature-dependent representation into a small, stable set of initializer records.

Example (data segments): regardless of how the parser encodes/extends the data section, the initializer wants a canonical record:

- segment kind: `active` vs `passive`,
- target memory index (for multi-memory),
- byte offset (for active segments),
- payload span (possibly zero-copy),
- dropped state (for passive segments).

This keeps the initializer API stable while allowing the parser to evolve via concepts.

---

### 6. Guideline

- Keep `initializer/*` structures **concrete and minimal**.
- Treat parser outputs as *input adapters*: convert once at the boundary, then operate on canonical initializer/store structures.
- If a future feature requires new initializer semantics, prefer adding a new field/variant in the initializer model instead of threading parser feature parameters through runtime code.
