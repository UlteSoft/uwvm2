# C++ threads, TLS and standard Wasm exceptions foundation

SOURCE_ONLY_UNCOMPILED_UNEXECUTED. This is a real C++ application using the
target standard library, `std::thread`, blocking atomic wait/notify, independent
Guest TLS and typed cross-frame exception unwinding. It is a prerequisite for
original rclcpp work, not an rclcpp, ROS, Host pthread-TLS or performance pass.
Nothing in this directory has been compiled, imported, assembled or executed.
All such work belongs to the existing SSH Linux64GiB/zero-swap controller.

The program starts two actual C++ workers, waits for both real entries, releases
them together, checks distinct TLS owners, throws/catches two different standard
exception classes through another stack frame, counts actual unwinding guards,
joins both threads and checks all Guest TLS destructors. It also checks two
64-bit values above32 bits and all bounded output writes. A child creation
failure releases and joins the first child before the error propagates. There
is no successful no-thread fallback, copied model, Host std::thread substitute,
clock, preopen, environment requirement or ROS grant.

Use the independently pinned actual WASI SDK33 source/sysroot/compiler closure.
The primary source receipt is
`/tmp/uwvm-wasi-sdk33-threaded-eh-primary-source-root-20261018-r1/source-provenance.json`.
The tag resolves to commit `c10c0507deb3e5aad506f1f9f32084e49a21834b`,
LLVM `4434dabb69916856b824f68a64b029c67175e532`, wasi-libc
`161b3195fc2558d2b1ba3eb9ffae3b2b47407623` and the three actual SDK LLVM
patches. This is source provenance, not a claim that those target libraries
are installed or built. Never mix the old non-exception SDK24 archives.

The future compiler invocation must select `--target=wasm32-wasip1-threads`,
the real target sysroot, `-std=c++20 -pthread -fwasm-exceptions`,
`-mllvm -wasm-use-legacy-eh=false`, debugging information, and link with
`-fwasm-exceptions -lunwind`. Disable LTO for this qualification. Keep actual
EH-specific include/archive paths, compiler-rt, libc, libc++, libc++abi and
libunwind input/link traces. The SDK's
[actual build logic](https://github.com/WebAssembly/wasi-sdk/blob/c10c0507deb3e5aad506f1f9f32084e49a21834b/cmake/wasi-sdk-sysroot.cmake)
selects threaded exception variants; see also its
[exception flags](https://github.com/WebAssembly/wasi-sdk/blob/c10c0507deb3e5aad506f1f9f32084e49a21834b/CppExceptions.md).

Import `env.memory` using actual linker options `--import-memory` and
`--shared-memory`, initial16MiB/max64MiB, and a finite actual linear stack.
Assemble `provider.wat` with the real pinned WAT tool. Verify final memory0 is
that imported shared memory and not an extra local memory; inspect the actual
thread entry, imports, tag/feature types, constructors and TLS initialization.
Do not stub missing symbols, rewrite the exception tag, fabricate a native
provider, ignore undefineds or remove thread creation to make the link pass.
Additional standard-library imports must receive an actual admitted Runtime
implementation or be reported as an implementation gap.

Run INT and LLVM separately against the same immutable artifact with finite
real Threaded policy quotas, genuine provider preloading and the real source
validator. A finite external deadline owns the process and all descendants;
the blocking atomics intentionally rely on actual peer progress. Unknown
imports, a timeout, signal/kill, startup refusal, partial output, nonzero exit,
missing cleanup or unsupported feature are failures. The required stdout is
exactly the three lines in `expected.stdout`, with no Guest stderr. Retain
real spawn/entry evidence, maps/target identity and Root physical-retirement
evidence separately. Its Guest destructor count does not prove Host TLS joined.

Before trusting a Runtime result, use the exact same Wasm/provider with an
independent real WASI-threads-capable engine and its explicit feature/import
configuration. A missing engine provider is unsupported, not a skipped pass.
DBG must stop in both child frames, show each actual TLS owner and unwind
scope, then continue to the exact normal result. Measure the resulting real
application only after functional/DBG/retirement qualification.
