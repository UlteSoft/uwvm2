# Language debugger options and Wasm execution

All fifteen language debugger switches are **enabled by default** in `uwvm2`,
`uwvm2-stable` and `uwvm2-ros`. Keeping them all enabled is recommended. Disabling
other languages is normally unnecessary: a Python, Java, C# or JavaScript
application also contains runtime/library code produced from C, C++ or another
language, and mixed-language stack frames benefit from their debug adapters.

These options select **debugger adapters in the host UWVM build**. They do not
select which source languages are allowed to execute. Turning a language's
debug adapter off does not disable its Wasm instructions, reject modules based
on their producer language, remove its WASI imports or alter its execution
semantics. A legitimate Wasm module produced from that language can still run
through the selected interpreter/JIT. As usual, the module must use supported
Wasm features, available imports and explicitly admitted Host resources.

For example, `--debug-language-python=n` selects Python-specific host debug
adaptation off wherever that specialization is implemented. It does not disable CPython compiled into Wasm, its bytecode interpreter
or a Python application's ordinary execution. Similarly, disabling C# debug
adaptation does not remove the Mono runtime bundled inside a C# Wasm module.
An option is not a ROS/WASI grant, a compiler installation or a managed-runtime
replacement.

## Configure and rebuild

Use these commands separately in each selected checkout. A fresh configuration
with no language overrides keeps all fifteen switches enabled:

```sh
xmake f -c -m release
xmake build -j1 uwvm
```

To disable only Python's debugger adapter and keep the other fourteen defaults:

```sh
xmake f -c -m release --debug-languages=individual --debug-language-python=n
xmake build -j1 uwvm
```

`individual` reads each `debug-language-*` Boolean. All fifteen default to `y`.
Review saved configuration when reconfiguring an existing build: `-c` clears
cached configuration, so preserve the intended compiler/backend/platform/ROS
options explicitly when reproducing a specialized build.

An explicit selection overrides the individual Booleans:

```sh
# Full language debug support: recommended default selection.
xmake f -c -m release --debug-languages=all

# Only Python-specific host adaptation; common Wasm machinery is separate.
xmake f -c -m release --debug-languages=python

# Exactly Java, C# and Python adapters.
xmake f -c -m release --debug-languages=java,csharp,python

# No language-specific adapters. This does not disable ordinary Wasm execution.
xmake f -c -m release --debug-languages=none
```

Rebuild after configuration. Switching a build option does not change an already
compiled executable. With an explicit list, `all` or `none`, changing an individual
Boolean has no effect until selection returns to `individual`. Unknown language
names, duplicate entries, an empty list and empty comma-separated fields are
refused. Lists use the exact lower-case spellings below; use `cpp`, `csharp`,
`javascript`, `assemblyscript` and `vb`.

## Independent switches and internal macros

Every macro accepts exactly `0` or `1`. Xmake emits an explicit value for every
entry. Outside xmake, an undefined macro defaults to `1` through
`src/uwvm2/uwvm/debugger/language_debug_features.h`; values such as `2` or `-1`
are compile errors. Set macros consistently across translation units and avoid
mixing objects built with different selections.

| Language | Individual xmake option | Internal macro |
| --- | --- | --- |
| C | `--debug-language-c=y/n` | `UWVM_DEBUG_LANGUAGE_C` |
| C++ | `--debug-language-cpp=y/n` | `UWVM_DEBUG_LANGUAGE_CPP` |
| Rust | `--debug-language-rust=y/n` | `UWVM_DEBUG_LANGUAGE_RUST` |
| Go | `--debug-language-go=y/n` | `UWVM_DEBUG_LANGUAGE_GO` |
| TinyGo | `--debug-language-tinygo=y/n` | `UWVM_DEBUG_LANGUAGE_TINYGO` |
| Zig | `--debug-language-zig=y/n` | `UWVM_DEBUG_LANGUAGE_ZIG` |
| Java | `--debug-language-java=y/n` | `UWVM_DEBUG_LANGUAGE_JAVA` |
| C# | `--debug-language-csharp=y/n` | `UWVM_DEBUG_LANGUAGE_CSHARP` |
| F# | `--debug-language-fsharp=y/n` | `UWVM_DEBUG_LANGUAGE_FSHARP` |
| Visual Basic | `--debug-language-vb=y/n` | `UWVM_DEBUG_LANGUAGE_VB` |
| Python | `--debug-language-python=y/n` | `UWVM_DEBUG_LANGUAGE_PYTHON` |
| JavaScript | `--debug-language-javascript=y/n` | `UWVM_DEBUG_LANGUAGE_JAVASCRIPT` |
| AssemblyScript | `--debug-language-assemblyscript=y/n` | `UWVM_DEBUG_LANGUAGE_ASSEMBLYSCRIPT` |
| MoonBit | `--debug-language-moonbit=y/n` | `UWVM_DEBUG_LANGUAGE_MOONBIT` |
| Kotlin | `--debug-language-kotlin=y/n` | `UWVM_DEBUG_LANGUAGE_KOTLIN` |

Some entries provide an independent selection point for managed integration
still under qualification. A selection point with no dedicated implemented
host adapter may have no code-size effect; it does not supply that adapter.
Common Wasm/DWARF facilities remain separately implemented.

A switch controls the corresponding implemented debug specialization; its
presence alone is not proof that a particular producer/runtime supports source
breakpoints, language object inspection or every managed value type. The
[high-level-language suite](../../test/0050.high_level_languages/README.md)
records producer-specific capabilities and limitations. Raw Wasm functions,
instructions, stack/memory views, native-runtime DWARF and managed-language
source/variables are distinct debug levels. Preserve and validate actual
producer DWARF/source maps/PDBs and use the correct debugger backend.

Host options also do not add debug instrumentation to an already compiled
Guest. For example, the CPython frame-snapshot route needs its documented Guest
build sources and matching compile-time selection, in addition to a compatible
host debugger. The Mono native DWARF route does not by itself provide C# managed
locals. Enabling all adapters does not fabricate unavailable debug information.

## Debug selection and WASI authority are independent

A debugger-language option neither grants nor denies a WASI resource. For
example, a Java `System.nanoTime()` benchmark needs the monotonic clock even
when every language debug adapter is enabled. In a runtime that defaults to
explicit WASI authority, add only the permission the application uses:

```sh
--wasip1-global-grant monotonic-clock
```

Place that option before `--run`. It does not grant realtime/process/thread
clocks, randomness, directories or process operations. A missing resource grant
can cause a language runtime to exit during initialization; diagnose the actual
WASI call rather than changing the language's debugger switch. Other UWVM
branches may expose a different authority interface; use the recorded command
for the selected product. Debugger trimming also does not relax ROS capability
checks or make unsupported imports available.

## Execution and size evidence

The [default-on compilation matrix](../../test/0050.high_level_languages/results/native-budget/language-defaults-on-size-20261019/README.md)
records 35 selections across three repositories and Debug/Release: 210 actual
adapter compilations/runs, invalid-list checks and macro checks. Its ten full
interpreter products execute 380 expected Guest outcomes. In particular, the
ROS products include `none` and Python-only selections; their ordinary language
execution results remain valid. This evidence is bound to the recorded source,
compiler, binary and Wasm hashes, and does not substitute for each producer's
actual debugger workflow or an LLVM/platform matrix.

In that measured interpreter profile, all/none/Python-only selections have
identical stripped ELF sizes. Language debug selection is not a general promise
of a large size or speed reduction. LLVM/JIT, native Gateways, Guest runtimes,
optimization, debug sections and platform-specific dependencies affect different
parts of the system and need separate measurements. Keep the default full
selection unless a concrete constrained deployment requires trimming, and
retain a full-debug build for diagnosis.


A separate [fresh LLVM construction checkpoint](../../test/0050.high_level_languages/results/native-budget/language-llvm-build-size-20261019/README.md)
measures the manual O2-g1 ROS raw ELF at 121.253 MiB for all languages and
120.811 MiB for none, a difference of 463,912 bytes. These include symbols/debug
sections and are not stripped deployment sizes or debugger/performance results.
ROS native imports are disabled in that manual LLVM profile; dual-ROS platform
sizes belong to the separately linked interpreter matrix.

The [updated LLVM and native comparison](../../test/0050.high_level_languages/results/native-budget/language-debug-native-vtune-20261019/README.md)
records actual strip-all sizes: ROS all59.78MiB and none59.63MiB, a157200-byte
difference in the manual LLVM profile with native ROS imports disabled. The
completed Java comparison and sixteen effective source workflows retain
separate qualification scopes.


The [C/C++ producer and source matrix](../../test/0050.high_level_languages/results/native-budget/language-c-family-20261019/README.md)
adds eight actual compiler/linker runs, sixteen ordinary and sixteen real Wasm
DBG rows, and eight source scalar/step workflows. Its all/none ROS profiles
show that generic Wasm/DWARF remains usable when specific adapters are trimmed.
The C++ scalar fixture uses C linkage; it does not certify disabled demanglers
or class pretty printers. These are correctness results, separate from the
queued native/VTune comparison and other producer-specific debugging work.

The [F#/VB/Rust/Go producer matrix](../../test/0050.high_level_languages/results/native-budget/language-four-producers-20261019/README.md)
provides real native/WASI Debug/Release construction and basic build/run tutorials.
Its 120 exact program outcomes compare native execution with Wasmtime. They do
not qualify a dedicated managed debugger adapter or replace the separately
queued UWVM execution/DBG and native/VTune checks.

The [Zig/TinyGo/Kotlin producer matrix](../../test/0050.high_level_languages/results/native-budget/language-zig-tinygo-kotlin-20261019/README.md)
adds actual native/WASI construction and seventy exact program outcomes, with
TinyGo panic/recover limitations and the Kotlin JVM-to-TeaVM WASI route stated
explicitly. Complete UWVM consumer DBG and native/VTune results require their
separate actual executions. Retained compiler model controls are not language
execution or ROS SDK parity evidence.


The [Go authority and TinyGo/Kotlin startup record](../../test/0050.high_level_languages/results/native-budget/language-go-authority-startup-20261019/README.md)
demonstrates this separation on the frozen ROS-all LLVM product. TinyGo and
Kotlin complete with default authority; Go needs explicit monotonic-clock and
realtime-clock parameters for this fixture. Actual call traces and omission
controls establish those requirements. These invocation parameters do not
change debugger adapter selection, and the startup results add no DBG or
performance credit.

## Standalone adapter probe

The [header-mode build probe](../../test/0050.high_level_languages/build-options/README.md)
checks independent selections without building a complete VM. Its
`matrix-expected-mask=none` option only omits a test assertion; it does not
select `debug-languages=none`. All fifteen production switches still default
on. The probe documents fresh defaults, individual trimming, exact masks and
refusal of incomplete container metadata without guest-memory reads.


The [current configuration preflight](../../test/0050.high_level_languages/results/native-budget/language-debug-option-preflight-20261019/README.md)
separately verifies 78 configure calls and their actual acceptance/refusal
reasons. Its archive received a complete independent readback. Configuration
checks alone do not replace target builds, genuine stopped-frame debugging
or ordinary Wasm execution.

The [Rust query metadata matrix](../../test/0050.high_level_languages/results/native-budget/language-rust-query-budget-20261019/README.md)
adds 24 native metadata combinations with all/none adapter macros, including
thread-local proof and uncached fallback paths. It does not add executable-cache
security, real source-variable, managed-runtime or performance qualification.

The [current adapter matrix](../../test/0050.high_level_languages/results/native-budget/language-current-option-matrix-20261019/README.md)
revalidates all 210 selected header/probe combinations and 99 direct macro
checks against frozen current debugger inputs. It distinguishes fresh execution
from byte-verified closed reuse and retains original harness/storage failures.
Its complete independent archive readback does not substitute for full-VM,
managed-language, ROS platform, QEMU or native/VTune qualification.

The [controlled Rust Runtime and ROS footprint result](../../test/0050.high_level_languages/results/native-budget/language-rust-runtime-query-size-20261019/README.md)
adds four complete CLI products, 40 ordinary outcomes, eight actual Wasm DBG
workflows and four original-source locals/print checks at 40 then 42.
The measured copied strip-all ROS ELF is 62,683,400 bytes with all adapters
and 62,526,200 with none, a 157,200-byte difference. Native ROS imports are
disabled in this manual LLVM profile. The None selection still runs legal
Rust and generic DWARF scalar debugging. Retain the default full selection;
these controlled source-bound correctness results add no native/VTune, real
ROS SDK parity or persistent executable-cache security qualification.


## MoonBit Transform source-map checkpoint, 20 October 2026

The [Transform checkpoint](../../test/0050.high_level_languages/moonbit-transform-sdk-20261020/README.md)
adds three original-file breakpoint/line/step sessions on the frozen ordinary,
stable and ROS LLVM products. Same-stop local reads explicitly remain
unavailable; the stale stop-id produces a distinct refusal. This establishes
source positions and stepping, without lexical values or ROS callback debugging.
Eight separate pure Wasm executions pass on the ROS product with all language
adapters disabled. Legal Wasm execution remains independent of adapter selection.
All fifteen options remain on by default; retaining the complete selection is
recommended. These finite results do not complete the full language/debug or
platform/cache matrix.
