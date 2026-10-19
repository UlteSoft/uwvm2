# MoonBit fixtures

Pinned compiler: `moonc v0.10.15+943464bfa (2026-10-12)`, with
`moon 0.1.20261012 (748b63c 2026-10-12)`. Both the `wasm` linear-memory backend
and `wasm-gc` managed-reference backend are exercised.

```sh
export MOON_HOME=/path/to/extracted/moon
export PATH="$MOON_HOME/bin:$PATH"
moon -C "$MOON_HOME/lib/core" bundle --target wasm --warn-list -a
moon -C "$MOON_HOME/lib/core" bundle --target wasm-gc --warn-list -a
sh build.sh /tmp/high-level-moonbit
```

The script builds both backends in debug and performance configurations. It
also preserves an upstream-stdio variant using builtin `println`; the GC
variant imports `spectest.print_char` and is expected to need that host import.
It is retained to make the host-ABI difference explicit.

Calibration builds `moonbit-TARGET-rounds-N-{debug,release}.wasm` for both
targets and `N=1000,20000,2000000` by default. Every portable calibration binary
is actually executed by Node/WASI, with rounds, versions, binary hash, exit code
and output saved in its `.oracle.json`; `.expected.stdout` saves the oracle
output verbatim. Set `CALIBRATION_ONLY=1` to leave the standard artifacts
untouched and `CALIBRATION_ROUNDS='1000 20000 2000000'` to choose the list.
Generated `source-rounds-N-MODE/workload.mbt` changes only the round count.
The reference helper is `../javascript/node_oracle.cjs`. Node startup is part
of the receipt time and is not a UWVM throughput measurement.

The portable variants use a small source-level WASI `fd_write` adapter. It
writes ASCII characters from a reserved 32-byte scratch region; the linear
backend heap starts at 1024. The GC backend keeps managed arrays/strings outside
linear memory. Both portable modules import only WASI Preview 1 `fd_write`.
This test adapter deliberately supports ASCII output only; it is not a general
UTF-8 standard I/O library. It avoids a custom MoonBit host namespace and does
not enable JS string builtins.

Coverage: mutable struct state and methods, managed arrays, nested loops,
arithmetic, strings and handled MoonBit errors. Smoke executes 20,000 rounds and
prints checksum `264715 caught=true`; performance executes 2,000,000 rounds and
prints checksum `682696 caught=true`. MoonBit error handling is language-level
coverage; it is not proof that the compiler lowered it to Wasm exception tags.
The GC debug module includes `name`, `producers` and `sourceMappingURL`, plus an
external source map containing the original source paths and embedded text.
The linear-memory target has function names but no source map in this version.
Neither target emits DWARF lexical-variable information. GC source-line
breakpoints and stepping are possible; they do not establish MoonBit lexical
variable scopes. The script relinks with the final module and sidecar basenames
through the compiler so `sourceMappingURL` stays valid after staging.
The GC smoke map records `source-smoke/workload.mbt` at its absolute Linux
build path. Lines 18 (array update) and 19 (method call) are emitted source points
for source breakpoint and step checks. Build and debug inside the same root
view so these recorded source paths remain accessible.

Pinned Linux x86-64 downloads and SHA-256:

- `https://cli.moonbitlang.com/binaries/0.10.15%2B943464bfa/moonbit-linux-x86_64.tar.gz`:
  `24b93a5409d2a4fb8e3d1b84b473f27b63476cef4418094764df96cb26b03c09`
- `https://cli.moonbitlang.com/cores/core-0.10.15%2B943464bfa.tar.gz`:
  `96d30f4dbcc6a9e1736ef26fbdd0e8fb0e97cb01d50e55d2a0072208b8061ae5`

The MoonBit distribution installer marks executables in `bin` and
`bin/internal/tcc` executable after extraction. The archive can expire under
the vendor's retention policy; retain the verified toolchain in the Linux test
cache if future builds need this exact version.

Official references: [Wasm FFI and host ABI](https://docs.moonbitlang.com/en/latest/language/ffi.html),
[build system](https://docs.moonbitlang.com/en/latest/toolchain/moon/tutorial.html),
[link settings](https://docs.moonbitlang.com/en/latest/toolchain/moon/package.html),
[error handling](https://docs.moonbitlang.com/en/latest/language/error-handling.html).
