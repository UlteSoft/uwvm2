# Current debugger platform acceptance candidate

These new files are **source only**. They have not been compiled, imported,
PowerShell-parsed, or run in a VM. Python AST inspection is not execution of
the tests. The existing r10 Windows PE and historical platform receipts do not
qualify the current joint source/activation producer or source next/finish.

Both repositories carry identical copies. Production controller, console,
runtime and compiler files are outside this test candidate's ownership.

## New Windows minimum

`run_windows_debug_acceptance_current_vm.ps1` admits the exact host-qualified
artifacts, then runs five actual cases:

| Case | instruction | unwind | Required observation |
|---|---:|---:|---|
| C DWARF5 O1 source finish | yes | yes | A real leaf statement returns to its real outer caller statement, official Code-row line/column match, fresh stop identifier, guest computes 52/58 |
| Wasm and consecutive assembly steps | yes | yes | Actual Wasm stop, two decoded native instructions with the second FROM equal to the first TO, actual trapped PC equal to each TO, fresh stop identifiers, native source stepping refused |
| Unsupported mode | one case | — | Ordinary LLVM lazy or ROS interpreter emits the exact colored fatal mode rejection; crash NTSTATUS, loader failure and parser errors cannot pass |

No thread identifier, source PC, inline path or physical depth is hardcoded.
Actual breakpoint replies identify the participant. Official emitted Wasm
bytes have zero imports; the Code oracle rejects any real import before using
local Code indices as module function indices. Function expression extents
bound the offsets used by the official line oracle.

The native launcher receives a private inherited input pipe and creates two
separate **regular files** for the product's stdout/stderr. This preserves the
strict sealed-console policy; the test does not make pipe output guest-visible.
Native handle operations use existing SDK-typed `noexcept` assembly imports or
fast_io Win32 declarations. The launcher checks its actual Job admission and
child Job inheritance and propagates the full child NTSTATUS with ExitProcess.

The source-only managed process owner is a narrow derivative of the existing
Windows test helper, with a new class name and bounded continuous stdin. Its
C# P/Invoke is managed test infrastructure, not a C++ product ABI or a fast_io
implementation. It creates the launcher suspended, assigns an owned
non-breakaway kill-on-close Job, then resumes it. Each case checks actual Job
active-process accounting equals zero after retirement and drains bounded
captures before admitting another case. Failure kills only that owned Job.
The retained PID and process creation time identify the real launcher. `quit`
writes the command without expecting a nonexistent subsequent prompt.

Windows documents default child Job inheritance and kill-on-close in
[Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects).
The explicit three-handle inheritance list follows
[UpdateProcThreadAttribute](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute).

## Fresh build input contract

The keeper must first cold-build the current qualified Win64 LLVM-full CLI
with the actual joint source/activation API, debugger-only entry/return/tail/EH
hooks, current host-wrapper selection and DWARF link closure. Current ordinary
and ROS products require separate exact source fingerprints and receipts.
The managed owner and native launcher have their own current source pins.
An old build plus a newly asserted source ID is not an admissible input.

`stage_windows_debug_current_vm.py` accepts a normalized **original** build
receipt with these fields. The adapter extracts existing evidence; it must not
execute or invent replacement commands for an existing object or PE.

```text
schema = 1
purpose = actual-current-win64-debug-full-build
repository = ordinary | ros
target = x86_64-w64-windows-gnu
source_id = sha256:<canonical original src + third-parties file-list hash>
source_before_file, source_after_file = actual original fingerprint paths
memory_max = 68719476736
memory_swap_max = 0
cpuset = 0,2,4,6,16-31
oom_before = oom_after = oom_kill_before = oom_kill_after = 0
production_pins = controller, activation_api, joint_source_activation,
                  source_api, debug_host_wrapper actual file SHA256
product_link = original successful command record
product_compiles = original runtime/main/etc command records
launcher_build = original successful new launcher command record
debug_info_dwarf_archive = {path, sha256}
actual_inputs = [{path, sha256}, ...] original consumed sources/objects/libs
```

Each command record has `argv`, `cwd`, `returncode`, `tool_sha256`, original
`log` and `log_sha256`, actual `output` and `output_sha256`. The actual direct
`-o` argument must resolve to that output. Runtime and main objects must be
freshly compiled from this source tree and consumed by that exact product
link. The new launcher source must be consumed by its actual Unicode-entry
build. Direct primary tool names must be Clang/Clang++/LLD/ld.lld (optional
version suffix); an env/shell/resource wrapper is not accepted as the actual
compiler. This first contract refuses standalone and embedded linker response files; a captured compiler/linker
response-file expansion requires an additional reviewed provenance adapter.

The actual `DebugInfoDWARF` archive must be present, hashed, and consumed by
the product link. This first minimum contract requires the direct archive
closure. A dynamic-only component provider needs a separate actual symbol and
import-provider qualification, and is not silently accepted here.

A minimal launcher compile recipe for the keeper's actual Windows SDK is:

```sh
"$CLANGXX" --target=x86_64-w64-windows-gnu --sysroot="$WINSDK" \
  -std=c++26 -stdlib=libc++ -nostdinc++ \
  -I"$WINSDK/include/c++/v1" -I"$WINSDK/x86_64-w64-mingw32/include" \
  -I"$SOURCE/src" -I"$SOURCE/third-parties/fast_io/include" \
  --ld-path="$LLD" -municode -O1 -static \
  "$SOURCE/test/0017.runtime/windows_debug_current_launcher.cc" \
  -L"$WINSDK/x86_64-w64-mingw32/lib" \
  -o "$BUILD/windows_debug_current_launcher.exe"
```

This is an intended command shape, not an executed command record. The keeper
records the expanded actual argv, compiler/SDK/libc++/linker/input hashes, raw
log, return code and resulting launcher SHA. SDK paths must come from its
qualified Win64 closure. Do not borrow DLLs from an unrelated r10 SDK.

## Linux staging and Windows invocation

Only the keeper may run these commands under the existing 64GiB/no-swap
cgroup and admitted CPU set, after no competing heavy job occupies the slot:

```sh
bash "$SOURCE/tools/ci/require_wasm3_test_cgroup.sh"
python3 "$SOURCE/test/0017.runtime/test_windows_debug_current_stage.py"
python3 "$SOURCE/test/0017.runtime/stage_windows_debug_current_vm.py" \
  --source-root "$SOURCE" --repository ros \
  --product "$BUILD/uwvm.exe" \
  --launcher "$BUILD/windows_debug_current_launcher.exe" \
  --build-receipt "$BUILD/current-debug-build-receipt.json" \
  --wasm-clang "$WASM_CLANG" --wasm-ld "$WASM_LD" \
  --wasm-tools "$WASM_TOOLS" --llvm-dwarfdump "$DWARFDUMP" \
  --llvm-readobj "$READOBJ" --out "$NEW_STAGE"
```

Use `--repository ordinary` for the ordinary product. Supply a separate
`--dll /actual/qualified/provider.dll` for each needed non-system DLL. Staging
uses actual `llvm-readobj --coff-imports` recursively for the product, launcher
and every provider. Unknown DLLs must have supplied, hashed AMD64 providers;
surplus providers are refused. Regular and delay imports are both included:
LLVM's actual
[COFFDumper implementation](https://github.com/llvm/llvm-project/blob/main/llvm/tools/llvm-readobj/COFFDumper.cpp)
prints them in the same command. The command is documented in
[llvm-readobj](https://llvm.org/docs/CommandGuide/llvm-readobj.html#pe-coff-specific-options).

Staging itself uses official Clang, wasm-ld, wasm-tools and llvm-dwarfdump to
emit/validate/verify/dump the current C DWARF5 O1 fixture. It retains original
argv, logs, tool versions/hashes, exact intermediate compiler-object SHA
before/after the Wasm link, exact Wasm SHA, actual expressions and statement
rows. Generated objects/logs/oracle and copied admission artifacts are all
checked again before publishing the qualifier. It checks full source **and bundled dependency** fingerprints,
original build evidence, tools, DLLs and fixture bytes again afterward.
`qualification.json` is published only after all checks succeed. Stage
success is not a Windows execution result.

The keeper copies the exact successful stage to the already managed Windows
VM, carries the qualification SHA through its existing trusted bootstrap, and
only then executes:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
  C:\current-stage\run_windows_debug_acceptance_current_vm.ps1 `
  -ArtifactRoot C:\current-stage -Repository ros `
  -QualificationSha256 <actual-successful-stage-qualification-SHA256>
```

The qualifier is checked before dot-sourcing the managed owner or launching
any executable. All artifacts and non-system DLLs are pinned before/after the
cases. Product transcripts, launcher captures, real commands, stop records,
full exit codes and Job retirement evidence remain in the fresh Documents
evidence directory. A failed case remains failed even if cleanup succeeds.

## Existing platform evidence and remaining gaps

`run_windows_cross_eh_smoke_vm.ps1` schema2 has sixteen run-mode cases:
two policies times catch-ref (run, exceptions-off, function-references-off),
Win64 SEH (run, exceptions-off), and multi-object EH (uncached, cache-store,
cache-hit). It has no debugger/source/native stepping, attach or replacement
case. Its sixteen-case result cannot be relabeled as debugger acceptance.

The older `run_native_step_windows_product_vm.ps1` has native stepping,
mode/feature/security negatives, source O0 cases, and replacement cases. Its
source cases use fixed thread/depth/origin assumptions and predate the current
joint producer and stop identifiers. They must be requalified with an actual
fresh PE before their limited scope can be reported again.

This new minimum does not yet cover C++/Rust current source into/next/finish,
DWARF4, nested inline out, repeated call sites, recursive physical activations,
typed tail continuations, EH exits, source numeric values, DAP, replacement
publication/source invalidation, or authenticated midrun attach on Windows.
Those are separate current acceptance cases; synthetic policy/ledger tests
and old receipts are not substitutes.

## macOS preparation and 4GiB execution recipe

Current native stepping is available for **macOS ARM64** only. The macOS x86_64
backend is not implemented and must return an explicit unavailable result.
The ARM64 backend needs the selected SDK's `mach_exc.defs`, a fresh generated
task-id-token MIG server object, actual Security/CoreFoundation framework
linkage, qualified LLVM DWARF/full-JIT dependencies and the current runtime.
`xmake/mach_exc_protected.lua` generates the real server with
`xcrun mig -DMACH_EXC_SERVER_TASKIDTOKEN_STATE=1`; the generated source/object
and SDK spec must appear in actual build evidence. Entitlement/Enhanced
Security refusal is a real unsupported result, not a reason to bypass the
backend's task/thread checks.

The old macOS native/source test preloads a provider and importer with `-Wpre`.
Current canonical single-source metadata binding deliberately excludes
preloads; those old line strings cannot qualify the current joint source
next/finish implementation. A fresh single-module, zero-import C5 fixture
from the official stage above is the shortest next platform slice. Copy the
exact Wasm/source/oracle/tool receipts; do not re-encode it with an unrecorded
local tool. The ordinary and ROS actual CLI mode flags must be explicit.

Before **any** macOS build/test the keeper must obtain an exclusive execution
window, record the actual PID/start identity and no-overlap process inventory,
and require the machine's observed swap usage to be zero. Record selected
SDK/compiler/linker/framework/LLVM/header hashes, the complete source/dependency
fingerprint before and after, all actual commands/logs and Mach-O architecture.
Run one small process tree at a time and admit only a conservative memory
budget below 4GiB. No build/test is authorized by this document itself.

The existing wrapper invocation shape is:

```sh
python3 "$SOURCE/test/0017.runtime/macos_rss_limit.py" \
  --limit-bytes 4294967296 -- <one-actual-command-and-its-arguments>
```

It samples aggregate descendant RSS every 20ms and records direct-child
`wait4` maximum RSS. **It is a watchdog, not a kernel hard aggregate limit**;
it also does not prove complete retirement of orphaned descendants after the
root exits. The keeper therefore needs current birth-pinned ownership,
full-tree retirement and no-overlap checks in its outside supervisor. An
observed peak above 4GiB, denied cleanup, unresolved descendant, swap activity
or changed source/tool/provider pin makes the test fail. macOS has no Linux
cgroup no-swap guarantee; preserve zero-swap observations and any limitations
instead of claiming an equivalent hard cgroup cap.

The keeper may additionally qualify a kernel-matched inherited `RLIMIT_AS`
limit in its **owned child**, recording setrlimit/getrlimit readback and the
actual Darwin/XNU revision. A confirmed per-process address-space cap of at
most 4GiB adds a kernel limit for each child; it is not a 4GiB aggregate process
tree limit. Serial compilation, a conservative aggregate headroom gate and
birth-pinned complete retirement remain required. Do not assume the old
`RLIMIT_RSS` name or a current upstream XNU branch matches the installed kernel.

`run_hot_replace_macos_debug_cli.py` already has bounded individual command
wrappers, but still needs the current source/build qualification.
`run_llvm_debug_control_fd_macos.py` starts raw product children; wrap its
**entire driver tree** in the outside 4GiB supervisor. Its actual driver is
still the direct authenticated socket parent of the VM child, even with the
watchdog outside it. Current attach, native stepping, source next/finish and
function replacement must each retain genuine stop/generation/source-owner
results. No macOS execution has been performed for this candidate.
