# Gated unsigned scalar kernel: C, Rust, Go, TinyGo and Zig

These sources prepare a native/WASI performance comparison using one defined
uint32 recurrence. They are additional performance fixtures; the larger language
semantics, debugger, ROS and SDK examples retain their separate qualification.
Source availability, a queued command or a successful compilation is not a
performance result. Consult the actual ended Linux reports before quoting ratios.

All task compilation, execution, debugger work and hardware sampling takes place
on SSH Linux in the original 64 GiB cgroup with swap disabled. The new cohort is
queued after the existing native comparison and language consumers. It must not
compile or launch another Guest during those formal hardware measurements.

## Algorithm and protocol

The initial value is 7. At iteration `index`, compute
`value = ((value * 33 + index) modulo 2^32) XOR 0x55aa`. C/Go unsigned arithmetic,
Rust wrapping operations and Zig wrapping operators implement the same rule.
The loop count comes from the command line and the final checksum is printed.
The separate `reference.c` oracle uses uint64 arithmetic and explicit modulo
2^32, so its intermediate addition/multiplication does not overflow. It supplies
the long-loop checksum independently of the four benchmark implementations.
Go and TinyGo compile the same Go file independently; one result does not
qualify the other compiler, scheduler or target.

No argument selects 2,000,000,000 iterations. One ASCII decimal argument selects
1 through 2,000,000,000; malformed input, zero, a larger value or extra arguments
print `lcg32 invalid iterations` to stderr and exit 2 before the application gate.
Leading zeroes are accepted within the ten-byte input limit. Compiler/runtime
initialization can occur before application argument validation.

The application writes `UWVM_PROFILE_READY` and blocks until stdin supplies the
single byte `G`. It starts its timer, runs the loop, writes `UWVM_PROFILE_DONE`,
reads its elapsed timer and blocks until stdin supplies `D`. Both markers go to
stderr. Only after the second gate does it print these stdout lines:

```text
lcg32 iterations=1000 checksum=3317011347
lcg32 kernel ns=<positive elapsed nanoseconds>
```

The application timer includes the loop, timer overhead and DONE marker I/O.
The host also records gate wall time and fresh-process readiness separately.
Waiting for permission to start, compiler/JIT startup and VTune attachment are
outside the kernel timer. Host process accounting and the reported kernel timer
are different measurements. This loop alone cannot establish application-wide,
managed-runtime, ROS callback or message-delivery performance parity.

## Build with the recorded toolchain versions

Select trusted compiler installations and writable bounded cache/temp paths.
The existing producer toolchains are Go 1.24.2, TinyGo 0.38.0 with that Go
installation, Rust 1.87.0 plus its matching wasm32-wasip1 standard library, and
Zig 0.14.1. C uses host Clang 21 and WASI SDK 24's Clang/sysroot. A new source
requires its own actual construction results even when its compiler was already
qualified for another fixture. The following optimized examples are recipes,
not a claim that the queued cohort has finished.

```sh
clang-21 -std=c11 -O2 -g -gdwarf-4 lcg32.c -o c-release.native
"$WASI_SDK_PATH/bin/clang" --target=wasm32-wasip1 \
  --sysroot="$WASI_SDK_PATH/share/wasi-sysroot" -std=c11 -O2 -g -gdwarf-4 \
  -Wl,--threads=1 lcg32.c -o c-release.wasm

GOTOOLCHAIN=local GOENV=off GOPROXY=off GOSUMDB=off CGO_ENABLED=0 \
  GO111MODULE=off GOOS=linux GOARCH=amd64 \
  go build -p 1 -trimpath '-ldflags=-compressdwarf=false' \
  -o go-release.native lcg32.go
GOTOOLCHAIN=local GOENV=off GOPROXY=off GOSUMDB=off CGO_ENABLED=0 \
  GO111MODULE=off GOOS=wasip1 GOARCH=wasm \
  go build -p 1 -trimpath '-ldflags=-compressdwarf=false' \
  -o go-release.wasm lcg32.go

tinygo build -p 1 -opt=2 -o tinygo-release.native lcg32.go
tinygo build -p 1 -opt=2 -target=wasi -o tinygo-release.wasm lcg32.go

rustc --edition=2021 -C debuginfo=2 -C opt-level=2 \
  -C codegen-units=1 -C panic=abort lcg32.rs -o rust-release.native
rustc --edition=2021 -C debuginfo=2 -C opt-level=2 \
  -C codegen-units=1 -C panic=abort --target wasm32-wasip1 \
  -C link-arg=--threads=1 lcg32.rs -o rust-release.wasm

zig build-exe lcg32.zig -O ReleaseSafe -lc -fsingle-threaded \
  -femit-bin=zig-release.native
zig build-exe lcg32.zig -O ReleaseSafe -lc -fsingle-threaded \
  -target wasm32-wasi -femit-bin=zig-release.wasm
```

The Debug matrix uses C/Rust optimization level 0, Go `-gcflags=all=-N -l`,
TinyGo `-opt=0` and Zig `-O Debug`. Keep debug sections in both configurations.
Rust does not accept the previously rejected `-C dwarf-version=4`; use its
actual target DWARF format. Zig's optimized selection remains ReleaseSafe.
These choices preserve the recorded producer profiles rather than silently
switching off bounds checks to improve a number.

## Run and diagnose WASI authority

For a quick Linux smoke check, a pipe can supply both gate bytes:

```sh
printf GD | ./rust-release.native 1000
printf GD | wasmtime run -C cache=n,parallel-compilation=n rust-release.wasm 1000
```

That pipe does not prove either gate blocks; it is not hardware qualification.
The actual harness observes an unchanged READY marker while blocked, releases
`G`, observes DONE while the process stays alive, stops its collector and only
then releases `D`. Use `profile_gated_kernel.py` with a hash-bound JSON plan for
that workflow. Its timed run is separate from hardware Hotspots and detailed
microarchitecture collections. Retain unqualified hardware rows and warnings.

UWVM execution mode depends on the selected product. The frozen ordinary LLVM
product uses `-Rcm full -Rcc jit`; its stable/ROS counterparts use `-Raot`. Put
runtime, feature and WASI parameters before `--run`. These new sources call timer
APIs, so the authority needs its own actual trace and denial/subtraction checks.
The earlier Go semantics fixture's two-clock proof is not a proof for every
timer implementation in this cohort. The queued consumer first records default
behavior, then validates the explicit timer permissions it actually uses.
Do not add directory, environment or random grants merely because a compiler
emits an import. Timing uses no application filesystem API.

All fifteen language debugger switches default on. Keep them enabled for mixed
application/runtime diagnosis. Trimming a host debugger specialization does not
prohibit legal Wasm execution or grant a timer/ROS permission. See the
[language option guide](../../../docs/debugger/language-options.md).
