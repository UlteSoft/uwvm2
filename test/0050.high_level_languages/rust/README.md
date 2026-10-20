# Rust to WASI: standalone producer example

`rust_language.rs` checks trait-object dispatch through a boxed object, a Vec,
a deterministic BTreeMap, UTF-8/codepoint counts, Result/Option rejection,
checked overflow and defined wrapping uint32 arithmetic. It exports an
un-inlined `debug_value` function for a separate source-debug qualification.
The fixture does not use panic unwinding, threads, a filesystem or a random
hash seed. A normal command needs only its arguments and stdout/stderr.

The actual producer route is Rust 1.87.0 with its matching native and
`wasm32-wasip1` standard libraries. Install matching components into your
toolchain. The upstream [target documentation](https://doc.rust-lang.org/rustc/platform-support/wasm32-wasip1.html)
describes the WASI Preview 1 target. Use the Wasm target, rather than a
JavaScript/browser target requiring an unrelated Host ABI.

```sh
rustc --edition=2021 --crate-name highlevel_rust -C debuginfo=2 \
  -C opt-level=0 -C codegen-units=1 -C panic=abort \
  --target wasm32-wasip1 -C link-arg=--threads=1 \
  rust_language.rs -o rust-debug.wasm
rustc --edition=2021 --crate-name highlevel_rust -C debuginfo=2 \
  -C opt-level=2 -C codegen-units=1 -C panic=abort \
  --target wasm32-wasip1 -C link-arg=--threads=1 \
  rust_language.rs -o rust-release.wasm
rustc --edition=2021 -C debuginfo=2 -C opt-level=0 -C panic=abort \
  rust_language.rs -o rust-debug.native
./rust-debug.native 17
uwvm --run rust-debug.wasm 17
```

Select the execution backend appropriate to your UWVM product. The recorded
manual LLVM ordinary product uses `-Rcm full -Rcc jit`, while stable/ROS use
`-Raot`; place those runtime options before `--run`. A default run uses 1000
iterations, and `17` changes the checksum loop count. Invalid integers, zero,
values above 1000000 or extra arguments print `rust invalid iterations` and
exit 2. These paths are compared with native execution and Wasmtime.

`-C debuginfo=2` retains the target's actual default DWARF format. Rust 1.87.0
rejects `-C dwarf-version=4`; the original failed command is retained in the
producer records. Do not add unsupported compiler flags or strip debug data
to make a debug test appear to pass. Preserve the source at its recorded path.
The foundation fixture's C-linkage scalar function is separate from Rust type
formatting, trait-object inspection, lifetimes and async/generator debugging.

All fifteen language debugger options default on and keeping them enabled is
recommended. To trim only Rust-specific host adaptation, configure
`--debug-languages=individual --debug-language-rust=n` and rebuild consistently.
Legal Rust-produced Wasm continues to run; trimming is not a runtime language
ban or a WASI/ROS permission grant. See the
[language-option guide](../../../docs/debugger/language-options.md).

The Debug/Release native/WASI producer matrix has twenty actual successful
native/Wasmtime outcomes after the compiler-option correction. This producer
result alone does not qualify current UWVM execution, source debugging, ROS
delivery or performance. All task qualification runs on SSH Linux in the
original 64 GiB cgroup with swap disabled. A later consumer/DBG matrix and
native/VTune comparison retain their own exact products and results.

## Linked standard-library metadata regression

The initial ROS/all Debug consumer passed ordinary output and generic Wasm
DBG; its original source breakpoint failed and remains in the
[closed consumer evidence](../results/native-budget/language-zig-fsharp-rust-closed-20261019/README.md).
Real metadata probes isolated both the old DIE and copied-string limits.
The [linked-DWARF matrix](../results/native-budget/language-rust-linked-dwarf-budget-20261019/README.md)
now verifies the current three-repository parser closure in Debug/Release
with all/none language macros, plus controlled frozen-source budget changes.
`check_linked_dwarf_budget.cc` consumes that exact original Rust/std artifact
and checks acceptance, three explicit budget refusals, truncated-data refusal,
clearing of a previously published index, and a clean reparse. The test does
not execute Wasm. The default DIE cap is 131,072 and the existing current
8 MiB cumulative string cap is preserved; neither is a total heap limit.
Complete Runtime source-value requalification must use the separately rebuilt
binary's SHA and cannot reuse an old product's result.


## Scalar query budgets and metadata ownership

Parsing linked DWARF and querying its variables use separate finite budgets.
For this Rust/std fixture, the native diagnostic charges 1,891,089 cumulative
metadata string bytes and 3,037 locations across 32,394 scopes, 1,125 types and
2,655 variables. Repeated names count each time they are validated; the raw
`.debug_str` section size is not the cumulative query charge. The old 1 MiB
scalar-query default refuses this metadata. The current checkout already uses
8 MiB for scalar metadata; that existing default is preserved. Object schema
validation fits its unchanged 1 MiB default. Result, object, depth and memory-read
limits are independent and are not raised for this fixture.

`check_linked_dwarf_queries.cc` consumes the real linked Wasm and its recorded
compilation directory. Build it against this checkout's debugger headers,
matching LLVM headers/libraries and matching C++ runtime; the retained Linux
commands record that complete host linkage. Run the resulting native test as:

```sh
./check_linked_dwarf_queries.native rust-debug.wasm /recorded/compilation/directory
```

The test validates the default scalar schema, explicit legacy-budget refusal,
refusal of narrowed location limits and refusal to lend a cached schema proof to
a foreign copied type span. With `UWVM_USE_THREAD_LOCAL` defined, it also checks
that the object's immutable index proof matches its own span and cannot bypass
narrowed type limits. Without that macro, no borrowed object proof may exist:
the ordinary uncached validator must still enforce limits and validate a copied
schema independently. Both paths must be compiled and executed; language
adapter all/none selection does not select this thread-local optimization.

The native test prints its explicit `THREAD_LOCAL_OBJECT_PROOF enabled` or
`disabled` marker and ends with `NO_GUEST_EXECUTION_OR_VALUE_QUERY`. It does not
fabricate a PC, stopped frame, source variable or Guest-memory read. Real
`break`, `step`, `info locals` and `print value` require a separate actual Runtime
PTY test and a genuine paused frame. The controlled CLI follow-up aligns frozen
r136 scalar/parser defaults to the already-present current defaults and retains
its own binary SHA; it does not qualify every later dirty-worktree feature.

These immutable per-index **metadata validation proofs** cache neither executable
code nor a Guest value. They grant no memory-read or source-owner authority.
Their tests do not certify persistent compiled-code cache identity, executable
SHA-256 invalidation, CPU-feature compatibility, cache authentication or ROS
permissions. Those need their own runtime security checks.

The [current query matrix evidence](../results/native-budget/language-rust-query-budget-20261019/README.md)
records 24 actual native combinations and 48 compiler/run actors across the
three repositories, Debug/Release, language all/none, and thread-local on/off.
Independent raw-artifact verification checks each exact output, input header,
native ELF hash and closed process record. The [controlled Runtime and size result](../results/native-budget/language-rust-runtime-query-size-20261019/README.md)
now independently verifies the complete archive, four full CLI products,
40 ordinary outcomes, eight Wasm DBG workflows and four real source scalar
workflows. Both locals and print change from 40 to 42 after stepping.
The ROS all/none raw and strip-all sizes retain exact ELF identities.
This controlled frozen-source result does not qualify every later worktree
feature, Rust object formatting, ROS transport or performance.
