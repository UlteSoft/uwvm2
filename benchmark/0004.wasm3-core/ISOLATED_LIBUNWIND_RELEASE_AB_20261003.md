# Isolated libunwind Release/assertions comparison

Source-only proposal. No new unwinder was built or executed. The original toolchain DSO and all previous samples remain unchanged. The purpose is to test one measured native-EH bottleneck while keeping payloads, immutable throw traces, tags, public ABI and JIT CFI registration intact.

## Actual evidence and unresolved identity

The current 50040-byte loaded libunwind has SHA-256 `241acf655fc4c677866c5c6c749a27478baf3305ed7880bdd16ce124afff672c` and MD5 `1c4746d081d017b51ab695659db7583f`; the MD5 matches the two EH Hotspots database module records. The independent filtered review is `build/wasm3-evidence/current-R3c-EH-P0-vtune-filtered-actual-20261003-r1/independent-derived-attribution.json` (SHA-256 `9dbe3b673d58e9df5ad4c4f196b78899c0efc1e3a7bd07fd6cae7657e5296a85`). Target P0 sampled self time in libunwind was 2.880949s of 3.374992s for unwind/native and 1.811505s of 2.240072s for instruction/native. This is sampled attribution, not elapsed latency or pure hardware counts; function-sum rounding differs from thread totals by 8us/7us. Uarch was multiplexed (0.56), and its function attribution was Unknown.

The stripped DSO does not provide static function names for 0x8330, 0x9cf0, 0x7ed0 or 0xad60. No `.comment`, `.note.gnu.build-id` or `.gnu_debuglink` section exists. Its bytes retain `LIBUNWIND_PRINT_APIS`, `LIBUNWIND_PRINT_UNWINDING`, `LIBUNWIND_PRINT_DWARF`, CFI opcode formatting and an assertion pretty-function for `CFI_Parser::parseFDEInstructions`. The attached Python-only inspection records exact file offsets. `/home/luo/llvm/libunwind/src` is an embedded build path, not a source revision or patch proof. The pretty-function's extra register template also exists in upstream LLVM23-init; it does not establish customization.

Root's actual disassembly inspection identifies the 0x8330 range as a two-input CIE/FDE opcode interpreter with per-opcode logging-condition branches. Its exact mangled name/source version remains unresolved. Do not label the nearest dynamic symbol `unw_strerror` as that function. Other addresses need their own actual FDE/disassembly review; an address's sampled cost cannot be assigned to trace capture versus native raise without actual caller information.

## Single controlled variable

LLVM's CMake option `LIBUNWIND_ENABLE_ASSERTIONS` defaults ON independently of build mode. On Release it appends `-UNDEBUG`; selecting Release alone does not remove these checks. Under `NDEBUG`, config.h removes debug tracing/API macros; normal malformed-CFI error branches and unwind control flow remain in source. The switch also removes assertions, so the experiment measures the combined standard release/assertion configuration effect, not just the two logging loads. [LLVM CMake options and flag handling](https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-23-init/libunwind/CMakeLists.txt), [trace macro definitions](https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-23-init/libunwind/src/config.h).

Build matched A/B from the same immutable source: A Release/assertions ON, B Release/assertions OFF. Both must have the same compiler, target, optimization, threading, frame-header-cache, frame APIs, CET/GCS, native/cross unwinding, shared SONAME, and libc/link configuration. Recover the actual installed DSO's source/CMake cache/compile commands if possible. Otherwise use a separately pinned upstream candidate for both A/B and keep the installed DSO as A0. In that case A/B isolates assertions in the new source; A0 versus B is a library-version/configuration comparison, with no assertion-only causal claim.

Upstream `llvmorg-23-init` is a candidate reference, not the current DSO's proven source. Keeper must resolve and record the actual tag commit and source tree SHA before building. These inputs are pending; no placeholder is executable qualification.

## Minimal keeper build

Use the existing trusted 64GiB/swap0 scope and E-core compilation. Do not overlap P0 timing, another compilation or QEMU/VM work. Build only `unwind_shared`, not LLVM/Clang. Use two fresh private build directories and a private install/output prefix; do not run install into the user's toolchain. CMake supports this runtimes entry point. [Official build instructions](https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-23-init/libunwind/docs/BuildingLibunwind.rst), [shared target and exception-safe C compilation](https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-23-init/libunwind/src/CMakeLists.txt).

The command shape below is a keeper template. `SOURCE`, `CC`, `CXX`, `A_BUILD`, `B_BUILD` and the shared options must first be bound to real immutable paths/hashes. `COMMON_OPTIONS` is an argv array derived from the recovered runtime configuration or an explicitly reviewed upstream candidate. Never inject `-fno-exceptions` into all source languages: upstream deliberately compiles the C exception entry points with `-fexceptions`.

```sh
cmake -G Ninja -S "$SOURCE/runtimes" -B "$A_BUILD" \
  -DLLVM_ENABLE_RUNTIMES=libunwind -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DLIBUNWIND_ENABLE_SHARED=ON -DLIBUNWIND_ENABLE_STATIC=OFF \
  -DLIBUNWIND_ENABLE_ASSERTIONS=ON "${COMMON_OPTIONS[@]}"
cmake --build "$A_BUILD" --target unwind_shared --parallel 16
cmake -G Ninja -S "$SOURCE/runtimes" -B "$B_BUILD" \
  -DLLVM_ENABLE_RUNTIMES=libunwind -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DLIBUNWIND_ENABLE_SHARED=ON -DLIBUNWIND_ENABLE_STATIC=OFF \
  -DLIBUNWIND_ENABLE_ASSERTIONS=OFF "${COMMON_OPTIONS[@]}"
cmake --build "$B_BUILD" --target unwind_shared --parallel 16
```

Record source/tool/cache/compile-command hashes before and after, source dirty state, generated config headers and the entire effective argv. A must have effective NDEBUG undefined; B must have it defined with no later `-UNDEBUG`. Common options may not override assertions. Keep source synchronization and all unwind error checks intact; no VM flag, validator, collector or custom personality changes are part of this candidate.

## Load, correctness, assembly, then measure

1. Pin each new DSO and its actual dependencies, ELF class/machine, SONAME and exported required `_Unwind_*`, `unw_*`, `__register_frame`/`__deregister_frame` symbols. Compare public headers/context layouts if source differs. Native dynamic-FDE tests, real throw/catch and mixed-host tests decide compatibility; a SONAME match alone is insufficient.
2. Select one private DSO directory through an explicitly recorded `LD_LIBRARY_PATH` prefix. Keep the already pinned libc++abi and remaining dependencies unchanged. No `LD_PRELOAD`, system install or `/etc/ld.so` changes. Untimed loader diagnostics and contemporaneous guest maps must confirm the actual new DSO bytes; remove `LD_DEBUG` and capture variables before timing. Revalidate the actual fixed closure and expected environment instead of pretending the historical DSO closure still matches.
3. Run same-byte native-EH cold fixtures for instruction/unwind and auto/native dispatch. Include cross-call tag aliases, catch_ref/throw_ref and old-trace rethrow, uncaught colored diagnostics, mixed native/T0 frames, dynamically registered/unregistered JIT code and replacement/drain refusal. Original thrown exception identity and stored trace must be preserved. Failure or unresolved ABI/CFI compatibility stops candidate timing.
4. Untimed O3 object/disassembly must show B's CFI interpreter no longer performs the tracing-config checks; retain bounds/error paths and frame restore behaviour. Dynamic symbols alone do not name inlined/private helpers. No object/IR capture or loader logging in timed samples.
5. Start with the existing 8M/500000-catch fixture (SHA `560243a56c6b29dbe0337548fa96626db2c168e56ea69d88208cffa4dfc48560`) and the no-throw controls. Use matched A/B/B/A order for instruction/native and unwind/native; retain auto as a separate algorithm control. The current CLI, Wasm bytes, cache-off, source, policies and affinity remain identical. Existing P0 plain wait4/internal timer, pure grouped hardware counts and VTune HW samples are separate families. Temperature is observation only; effective frequency/PMU running time and noise remain explicit quality evidence. Any whole-guest counter includes startup/JIT and cannot be called collector or throw-only ROI.

No current source/B DSO build, native correctness, speedup or release acceptance is claimed. This proposal is deliberately smaller than removing the extra immutable trace walk or changing the exception personality.
