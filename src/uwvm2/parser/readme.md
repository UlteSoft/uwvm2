 # High-performance, secure, and highly scalable standard parser
 
 The UWVM2 project ships a WebAssembly binary parser that is designed to be:
 
 - **High-performance**: friendly to modern CPUs, cache hierarchies, and SIMD back-ends.
 - **Secure and robust**: defensive against malformed input and exhaustively tested with fuzzers.
 - **Standard-compliant and extensible**: closely following the official WebAssembly specifications while remaining easy to extend with new proposals.
 
 This document summarizes the architecture of the parser from four perspectives: concept-oriented design (COP), SIMD and performance, safety, and standards compliance. For the detailed ADL concepts, extension points, and the full concept reference, see [adl.md](adl.md).
 
 ## Concept-oriented design (COP)
 
 The parser is built around C++ concepts and type-level composition rather than ad-hoc callbacks or global state.
 
 - **Feature as a concept.** Each WebAssembly feature implements the `wasm_feature` concept, which requires a feature name, a binary-format version, and an ADL entry point describing how the feature participates in parsing.
 - **Sections as concepts.** Type, import, function, table, memory, global, export, start, element, code, data, and custom sections are modeled as individual section concepts. Each section declares its numeric ID, optional name, and one or more ADL handlers.
 - **Compile-time composition.** A concrete parser configuration is synthesized at compile time from a tuple of feature types. This composition controls:
   - which sections exist and in which order,
   - which type aliases (`type_replacer` instances) are used for each standard structure,
   - which optional checks (for example, duplicate detection) are enabled.
 - **ADL as the extension hook.** All parsing logic for a feature is discovered through Argument-Dependent Lookup. To extend the parser, you provide new feature types and ADL functions in their namespaces; existing core code does not need to be edited.
 
 This COP-style design gives a standard parser that can express the full WebAssembly binary format, while also scaling cleanly as new proposals are added.
 
 ## SIMD and performance characteristics
 
 The parser is engineered to be friendly to SIMD and other low-level optimizations.
 
 - **Binary-format-first layout.** Data structures mirror the WebAssembly binary format, minimizing transformations and branches while scanning the input.
 - **SIMD-aware features.** The `uwvm2.parser.wasm.proposal.relaxed_simd` module implements parsing support for the Relaxed SIMD proposal (WebAssembly Release 3.0 draft). This allows modules that rely on high-performance vector instructions to be parsed without special-case code.
 - **Compile-time specialization.** Because the active feature set is known at compile time, the generated parser can be specialized for that exact combination of sections and proposals. This keeps the hot path small and improves cache locality.
 - **Room for back-end vectorization.** The separation between the low-level binary reader and higher-level ADL handlers makes it possible to plug in SIMD-accelerated readers without changing the surrounding architecture.
 
 The result is a parser that remains fast across a wide range of platforms while still being fully standards-aware.
 
 ## Safety, robustness, and fuzzing
 
 Security and correctness are treated as first-class concerns.
 
 - **Spec-driven invariants.** The implementation is annotated with comments that restate the relevant parts of the WebAssembly specification, together with the corresponding checks performed by the code.
 - **Fine-grained error reporting.** All parse errors are reported through a structured error object, allowing callers to distinguish between malformed binaries, unsupported features, and configuration issues.
 - **Systematic fuzzing.** The repository contains multiple fuzzers that target both individual sections and entire modules. These harnesses continuously exercise the parser against randomly generated and mutated binaries to detect crashes, assertion failures, and logic bugs.
 - **Defensive parsing.** Inputs are never trusted: bounds are checked rigorously, index spaces are validated, and invalid combinations of sections or feature flags are rejected early.
 
 Together, the manual cross-checking against the spec and the automated fuzzing campaigns provide a strong level of assurance for real-world workloads.
 
 ## Standards compliance and scalability
 
 The parser aims to be both **faithful to the WebAssembly specifications** and **scalable across versions and proposals**.
 
 - **Versioned binary format.** The core of the parser targets the WebAssembly binary format version 1 and its standard section layout. Feature types encode which binary-format version they belong to.
 - **Per-section correctness.** Each standard section is responsible for its own well-formedness rules (for example, type indices in the function section, limits in the table and memory sections, and element/data segment encodings). These rules are captured by the corresponding concepts and ADL handlers.
 - **Controlled feature growth.** New proposals (such as relaxed SIMD and multi-table/multi-memory configurations) can be expressed as independent features, without modifying existing ones. Compile-time checks prevent inconsistent combinations and duplicate handlers.
 - **Repository-wide tracking.** The set of supported features and the mapping to official WebAssembly releases are tracked in the top-level documentation files (`documents/features.md` and `documents/wasm-release.md`).
 
 Because the parser is assembled from small, well-specified building blocks, it can scale from a minimal, standards-only configuration to a feature-rich configuration that includes multiple proposals, without sacrificing performance or safety.
