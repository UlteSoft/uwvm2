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


The [TinyGo SDK continuation](../tinygo-sdk-resources-20261019/README.md)
adds an actual0.40.1 SDK/compiler/ROS2 resource matrix and finite source-debug
checks. ROS-specific SDK sources stay in the ROS checkout; these ordinary
language examples keep their basic build/run contract. The new SDK tests
separate supported checks from expected panic traps. No new native/VTune
performance equivalence is claimed. Keep all15 debugger adapters enabled;
disabling them does not prohibit legal TinyGo Wasm execution.

## TinyGo complete-example matrix checkpoint (19 October 2026)

The [source-bound example checkpoint](../tinygo-examples-matrix-20261019/README.md) independently verifies 121 of
135 planned products and 34 closed release-interpreter observations. Remaining
compiler/runtime/DBG and performance work is explicitly unqualified; the packet
retains resource refusals, original logs and an executable Linux resume plan.

## TinyGo qualified example, source DBG and codec matrix (19 October 2026)

The [qualified finite TinyGo matrix](../tinygo-examples-complete-20261019/README.md) records 135 actual products,
390 ordinary outcomes, 15 complete source-value workflows across three frozen
LLVM products and five optimizations, and 28 native/LLVM/Wasmtime codec
correctness cases. It completes the example rows pending in the earlier partial
checkpoint. Full SDK, native ROS, hardware performance and platform acceptance
remain separate open requirements.


## MoonBit typed Image checkpoint, 2026-10-19

The [finite Image SDK matrix](../moonbit-image-sdk-20261019/README.md) records360 actual model executions,60 application products,120 interpreter runs,12 pure engine checks,82 native Host-codec assertions and three source-map debugger workflows. MoonBit lexical object values, positive native ROS Image delivery, hardware performance and the complete original fifteen-language bundle remain unqualified. The ROS checkout contains the active SDK and detailed English Image guide; this packet is identical in all three repositories.


## MoonBit same-source Image native/Wasm checkpoint, 2026-10-20

The [native/Wasm checkpoint](../moonbit-image-native-wasm-20261020/README.md) records three real products and40 independently checked finite cases. Imported-GC memory pressure and a debug large-workload timeout remain failures. Five calibrations and two first-replicate observations do not qualify a performance comparison; raw-memory admission stopped the cohort before any VTune collection. The identical packet retains complete positive logs, failed predecessors, the report-path error disclosure and the detailed English build/run guide. Full SDK/client parity, managed-value debugging, native ROS Image delivery, hardware and platform acceptance remain open.


## MoonBit actual native ROS2 Image checkpoint, 2026-10-20

The [native Image checkpoint](../moonbit-native-image-20261020/README.md) independently verifies eight actual complete Image deliveries, eight enabled zero-grant scenes and48 naturally exited actors. It covers four new MoonBit products on both frozen interpreter modes, exact prior-String/Image policies, real error13/error22 decisions and native graph/GID/QoS/quiet retirement. The separate pure-codec performance retry again stopped at the raw60GiB guard with zero new VTune collections. Physical cameras, native Image subscription, ROS1/mixed/vendor coverage, full official client APIs, managed lexical debugging and platform/cache/hardware acceptance remain open.


## MoonBit actual native ROS1 Image checkpoint, 2026-10-20

The [native ROS1 Image checkpoint](../moonbit-native-ros1-image-20261020/README.md) independently verifies eight complete TCPROS deliveries, eight enabled zero-grant scenes and 49 naturally exited actors, including an isolated native Master. It covers four fresh MoonBit products on both frozen interpreter modes, exact Image/connection-header checks, Master graph identities, real error13/error22 decisions and quiet retirement. A separate five-engine codec performance retry stopped at the strict raw60GiB guard with zero new completed formal timings or VTune collections. Physical cameras, native Image subscription, mixed/vendor coverage, full official client APIs, managed lexical debugging and platform/cache/hardware acceptance remain open.


## MoonBit native Image pipeline and codec timing checkpoint, 2026-10-20

The [native input/processing/output checkpoint](../moonbit-image-pipeline-20261020/README.md) independently verifies eight full native ROS2 Image pipelines, 24 zero/partial permission scenes and 96 naturally exited actors. It also records 48 exact prior policies, 16 explicit split-topic profiles and 48 refusals. A separate unchanged codec kernel completed 15 fresh five-engine formal timings and one actual-sample-qualified Native VTune Hotspots profile. In that finite workload the three UWVM medians are below the selected Wasmtime median, while the roughly 3.8x gap to Native remains open. The following microarchitecture attempt stopped at the strict raw 60 GiB guard; the ten-profile hardware matrix, native ROS performance, sustained imported GC, full client APIs, lexical DBG and platform/cache acceptance remain incomplete.
