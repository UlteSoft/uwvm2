# AssemblyScript fixture

Pinned compiler: `assemblyscript@0.28.20`, including the transitive dependency
versions and integrity values in `package-lock.json`. The compiler requires
Node >= 20 and npm >= 10. Qualification used Node 22.21.1 / npm 10.9.4 on SSH
Linux, inside the existing 64 GiB cgroup.

```sh
npm ci --no-audit --no-fund --ignore-scripts
sh build.sh /tmp/high-level-assemblyscript
```

`NODE` can select an explicit Node executable. Output includes an unoptimized
debug module, an optimized smoke module, and an optimized performance module.
The 20,000-round smoke checksum is `264715`; the 2,000,000-round performance
checksum is `682696`. Both print one ASCII line through WASI Preview 1
`fd_write`. No browser or JavaScript host glue is needed.

Calibration additionally builds `assemblyscript-rounds-N-{debug,release}.wasm`
for `N=1000,20000,2000000` by default. Each binary has a Node/WASI execution
receipt (`.oracle.json`) and actual reference output (`.expected.stdout`).
Its generated `.ts` differs only in the requested round count. Set
`CALIBRATION_ONLY=1 CALIBRATION_ROUNDS='1000 20000 2000000'` to build only this
matrix; the list accepts other positive integers. The helper is checked in at
`../javascript/node_oracle.cjs`. Node reference startup is included in the
receipt duration and does not measure UWVM guest throughput.

Coverage: mutable class state, a managed array, dynamic array updates, nested
loops, integer arithmetic and string construction. AssemblyScript does not
support JavaScript exception semantics; compiler aborts intentionally become
Wasm traps through `--use abort=`. This fixture does not claim throw/catch
coverage. `--debug --sourceMap` emits a name section and external source map;
it does not emit DWARF lexical-variable information.

The source map resolves this fixture to `assemblyscript-debug/workload.ts`
beside the module; the build script stages the source there. Lines 18 (array
update) and 19 (method call) are emitted source points suitable for real source
breakpoint and step checks.

`start_contract.py --out DIRECTORY --uwvm BINARY` checks a distinct module
instantiation start and exported command entry, explicit local-function
overrides, an alias to the same start, start-only modules, and preloaded library
initialization. The library exports a trapping `_start` to ensure a host never
invokes library command exports. A correct command prints `init` then `entry`;
preloading the library adds `lib` first, exactly once. Use `--ros` for the ROS
product's AOT command line. The generator deliberately needs no WAT assembler.

AssemblyScript is a separate statically typed, TypeScript-like language that
compiles directly to Wasm. Arbitrary JavaScript cannot be passed to `asc`.
The sibling JavaScript fixture uses Javy and QuickJS for actual JavaScript.

Official references: [language introduction](https://www.assemblyscript.org/),
[compiler options](https://www.assemblyscript.org/compiler.html),
[JavaScript perspective](https://www.assemblyscript.org/concepts.html).
