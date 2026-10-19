# JavaScript fixture

Pinned compiler: Bytecode Alliance Javy 9.1.0. Javy embeds QuickJS and compiles
the JS bytecode and runtime into a statically linked core Wasm module. This
fixture uses real JavaScript classes, dynamic objects/arrays, strings and
catchable `RangeError`; it does not require a JavaScript host or a browser.

```sh
JAVY=/path/to/javy sh build.sh /tmp/high-level-javascript
```

The smoke checksum is `264715` after 20,000 rounds; the performance checksum
is `673761` after 200,000 rounds. Both end with `caught=true`. Performance uses
fewer rounds than the directly compiled languages because the guest executes
a complete JavaScript interpreter. These runtimes are not interchangeable
benchmark implementations, and their timings should not be compared as a
compiler quality ranking.

The build also creates `javascript-rounds-{50,200,1000}.wasm` for debugger and
sampler calibration. Their checksums are respectively `265424`, `739915`, and
`262020`; all retain the same object, array, string and exception semantics.
Each has its exact generated `.js` source, `.expected.stdout` and `.oracle.json`
alongside it. The receipt records rounds, compiler and Node versions, binary
SHA-256, output and exit code. The oracle actually executes both the Wasm in
Node/WASI and the generated JavaScript directly in Node and requires identical
output. The reported duration includes reference startup and both runs.

```sh
NODE=/path/to/node JAVY=/path/to/javy CALIBRATION_ONLY=1 \
  CALIBRATION_ROUNDS='50 200 1000' sh build.sh /tmp/high-level-javascript
```

`CALIBRATION_ONLY=1` leaves existing standard artifacts untouched. The rounds
list accepts other positive integers. Select a small debugger case first;
QuickJS executes inside Wasm, so debug instrumentation can magnify its cost.
Reference-engine speed does not predict an instrumented UWVM sampling window.

`-C source=uncompressed` retains the JavaScript source in `javy_source`.
`-C deterministic=y` makes pre-initialization deterministic; this test does not
use clocks or randomness. The generated module has no `name` or DWARF section.
Debugger checks can exercise Wasm function indices, instruction breakpoints,
stack frames and linear memory, but cannot claim JavaScript source breakpoints
or JavaScript lexical scopes.

The module imports only `wasi_snapshot_preview1` functions: `environ_get`,
`environ_sizes_get`, `clock_time_get`, `fd_close`, `fd_fdstat_get`, `fd_read`,
`fd_seek`, `fd_write` and `proc_exit`. It is not a WASI component.

Verified Linux x86-64 release archive SHA-256:
`a68b122d48eb3dfc1b801d4e14c39271fde3638243d3272d206e376ac9189e39`.
The official API asset endpoint is
`https://api.github.com/repos/bytecodealliance/javy/releases/assets/495744647`
(request `Accept: application/octet-stream`). Qualification used this endpoint
because the ordinary release download stalled; the digest matches the official
release metadata.

Official references: [Javy README](https://github.com/bytecodealliance/javy),
[pinned release and archive](https://github.com/bytecodealliance/javy/releases/tag/v9.1.0).
