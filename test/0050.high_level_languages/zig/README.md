# Zig to WASI

The Zig 0.14.1 fixture checks a struct method, arrays, allocated copies,
Unicode/codepoint counts, checked error/overflow results, arguments and
explicitly wrapping uint32 arithmetic. Both Debug and ReleaseSafe profiles
have actual native and WASI builds, full Wasm validation, and twenty matching
native/Wasmtime outcomes. These are untimed producer correctness results.

```sh
zig build-exe zig_language.zig -O Debug -lc -fsingle-threaded \
  -target wasm32-wasi -femit-bin=zig-debug.wasm
zig build-exe zig_language.zig -O ReleaseSafe -lc -fsingle-threaded \
  -target wasm32-wasi -femit-bin=zig-release-safe.wasm
zig build-exe zig_language.zig -O Debug -lc -fsingle-threaded \
  -femit-bin=zig-debug.native
./zig-debug.native 17
uwvm --run zig-debug.wasm 17
```

Select the backend required by your UWVM product before `--run`. For the manual
LLVM products, ordinary uses `-Rcm full -Rcc jit`, while stable/ROS use `-Raot`.
No arguments selects 1000 iterations. Malformed input, zero, a value above
1000000 or extra arguments prints `zig invalid iterations` and exits 2. The
fixture needs no mounted directory, clock, random source or ROS import.

Debug information is retained. The exported scalar probe uses
`@call(.never_inline, debug_value, .{})`; Zig 0.14.1 rejects the originally
attempted `export noinline fn` spelling. That original failure remains in the
records. Source breakpoints/values and complete UWVM Wasm DBG are separate
consumer qualifications, and should not be inferred from a successful build.

All fifteen Host debugger-language switches default on. Keeping them enabled
is recommended. `--debug-languages=individual --debug-language-zig=n` trims
Zig-specific Host adaptation wherever implemented after rebuilding; legal Zig
Wasm continues to execute with supported imports/features and admitted
resources. See [all language options](../../../docs/debugger/language-options.md).

The pinned compiler and actual commands follow the official
[Zig 0.14.1 documentation](https://ziglang.org/documentation/0.14.1/).
All task qualification runs on SSH Linux in the original 64 GiB/swap-zero
cgroup. This fixture is a basic non-ROS example and makes no SDK/API or native
performance parity claim.

## Resource SDK and finite debug follow-up

The [dated Zig continuation](../zig-sdk-resources-20261019/README.md) adds
32 actual ROS SDK producer builds,40 real ROS2 positives and four native
wrong-value controls in the ROS checkout. Its small real SDK codec probe
passes source breakpoints, repeated scalar reads, source stepping and stale
stop refusal on the three LLVM products. Eight ordinary probe executions
include the ROS no-adapter control. These do not certify full SDK/API parity,
native-resource source debugging, VTune or other architectures. This directory
remains the basic non-ROS compiler/use example.
