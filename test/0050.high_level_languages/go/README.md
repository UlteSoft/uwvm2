# Go and TinyGo source fixture

`go_language.go` exercises interface dispatch, a generic sum, slices, a map,
Unicode, panic/recover, a goroutine/channel rendezvous, live allocations across
`runtime.GC`, arguments and a defined uint32 checksum. The same source can be
offered to Go and TinyGo; each compiler, target and runtime needs its own actual
qualification. A Go result is not automatically a TinyGo result.

The currently qualified producer is Go 1.24.2. Its WASI Preview 1 route uses
`GOOS=wasip1 GOARCH=wasm`, as described in the official
[Go WASI guide](https://go.dev/wiki/WebAssembly#wasigooswasip1-port).
It does not use the `js/wasm` target or `wasm_exec.js`.

```sh
# Choose writable, bounded cache/temp paths on the Linux test machine.
export GOTOOLCHAIN=local GOPROXY=off GOSUMDB=off CGO_ENABLED=0
export GO111MODULE=off GOMAXPROCS=1
GOOS=wasip1 GOARCH=wasm go build -p 1 -trimpath \
  '-gcflags=all=-N -l' '-ldflags=-compressdwarf=false' \
  -o go-debug.wasm go_language.go
GOOS=wasip1 GOARCH=wasm go build -p 1 -trimpath \
  '-ldflags=-compressdwarf=false' -o go-release.wasm go_language.go
GOOS=linux GOARCH=amd64 go build -p 1 -trimpath \
  '-gcflags=all=-N -l' '-ldflags=-compressdwarf=false' \
  -o go-debug.native go_language.go
./go-debug.native 17
uwvm --wasip1-global-grant monotonic-clock \
     --wasip1-global-grant realtime-clock --run go-debug.wasm 17
```

Select the appropriate UWVM interpreter/JIT options before `--run`. The manual
LLVM ordinary product uses `-Rcm full -Rcc jit`, while stable/ROS use `-Raot`.
No arguments means 1000 iterations. `17` changes that count; malformed input,
zero, a value above 1000000 or extra arguments prints `go invalid iterations`
and exits 2. The Go producer matrix compares all five paths against native
execution and Wasmtime for both Debug and Release.

The fixture makes no application filesystem call. A language runtime may still
request clocks or randomness during initialization. In a product using explicit
WASI authority, inspect the denied import and grant only the required resource;
an import's presence alone does not justify a directory mount or broad grant.
Producer/Wasmtime success does not qualify default-deny UWVM startup. That is a
separate consumer test. Arguments do not need a Go-specific pointer-table ABI
switch.

The [actual startup/authority record](../results/native-budget/language-go-authority-startup-20261019/README.md)
qualifies this exact Go Release fixture on the frozen ROS-all LLVM product.
Without a grant, Go exits during its monotonic-clock initialization. Granting
only monotonic-clock reaches GC, which also requires realtime-clock. With both
explicit grants it completes; subtracting either grant exits 2 again. Keep
permission parameters before `--run`. The matrix's default denial remains a
failed original row, separate from the successful explicit-authority invocation.

The successful trace also observes `random_get` without granting it. Completing
this fixture does not establish cryptographic randomness. An application using
cryptographic randomness must request the `random` resource and handle its
failure; this fixture's startup profile is not a universal Go policy. The
ordinary/DBG consumers in the other products remain separately qualified.

Keep DWARF rather than using `-ldflags=-s -w`. Debug disables optimization and
inlining in all compiled packages; Release keeps its normal optimizer. The
un-inlined `debugValue` function is a future source-position/scalar probe.
Goroutine-aware stacks, interface/map/slice formatting and source-level locals
need separate actual debugger checks; instruction-level Wasm control is a
distinct capability.

TinyGo WASI uses `tinygo build -target=wasi`; its scheduler must support this
fixture's goroutine and channel. `-scheduler=none` changes the supported program
and cannot qualify this source. The pinned TinyGo version, selected scheduler,
panic strategy, optimizer, native target and real results must be recorded
before claiming TinyGo support. See its official
[build-option reference](https://tinygo.org/docs/reference/usage/important-options/).

`tinygo_language.go` is the separately qualified TinyGo 0.38.0 example. It uses
explicit error returns for malformed input and bounds checks. It preserves
interfaces, generics, maps, Unicode, goroutine/channel scheduling, live GC roots
and the same wrapping checksum. Debug (`-opt=0`) and optimized (`-opt=2`) native
and WASI builds pass twenty actual native/Wasmtime cases:

```sh
tinygo build -p 1 -opt=0 -target=wasi -o tinygo-debug.wasm tinygo_language.go
tinygo build -p 1 -opt=2 -target=wasi -o tinygo-release.wasm tinygo_language.go
tinygo build -p 1 -opt=0 -o tinygo-debug.native tinygo_language.go
./tinygo-debug.native 17
uwvm --run tinygo-debug.wasm 17
```

The unmodified full Go fixture was also offered to TinyGo. Its runtime bounds
panic aborts the native target; its explicit panic traps the WASI target rather
than being recovered. Both Debug/optimized failures are retained. The checked
error example is not recover emulation or full Go compatibility. This agrees
with TinyGo's documented [recover limitations](https://tinygo.org/docs/reference/lang-support/#recover-builtin).
Use the full Go route when application semantics require those recover cases;
enabling a Host Wasm exception feature cannot reconstruct recovery that the
producer lowered to a trap.

All fifteen host language debugger switches default on. Keep them enabled for
mixed runtime/application frames. `--debug-language-go=n` and
`--debug-language-tinygo=n` independently trim their host specializations under
`--debug-languages=individual`; legal modules continue to execute. Rebuild after
changing the selection. These switches neither choose the compiler nor change
WASI/ROS permissions. See the complete
[language-option guide](../../../docs/debugger/language-options.md).

All task compilation, program execution, DBG and hardware profiling happens
on SSH Linux in the original 64 GiB/swap-zero cgroup. The Go producer's twenty
passing native/Wasmtime outcomes are correctness results. They do not establish
current UWVM DBG, native timing parity, ROS delivery or SDK/API parity.
