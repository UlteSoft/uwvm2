# Language validation across native and QEMU platforms

This is the qualification procedure for the language suite, not a list of
completed platform results. Producer recipes remain in the
[language suite](../README.md). A Wasm artifact, a UWVM host executable and a
native ROS agent have separate architectures, ABIs and provenance. Record all
three before describing an application as portable.

All actual builds, emulator runs, debugger tests and profiling for this task
belong on SSH Linux. The QEMU process and its descendants must remain in the
original existing 64 GiB cgroup with swap disabled. Guest-side cgroup information
is additional evidence; it does not replace the host process's resource bracket.

## ROS release and architecture scope

ROS 2 support tiers apply to an OS, architecture and middleware combination.
The Jazzy table identifies Ubuntu 24.04 on amd64 and arm64 as Tier 1; Debian 12
has source-oriented Tier 3 rows for both. These are release-specific examples,
not a claim about every distribution or vendor package. Check the selected
release's actual table and installed RMW. See
[REP 2000 in the official repository](https://github.com/ros-infrastructure/rep/blob/master/rep-2000.rst).

ROS 1 Noetic's historical architecture list includes amd64, arm32 and arm64;
its stated support period ended in May 2025. Arm32 therefore belongs in explicit
historical migration testing, with the actual dependency set recorded, rather
than being inferred from a newer ROS 2 table. See
[REP 3](https://github.com/ros-infrastructure/rep/blob/master/rep-0003.rst).

The following are project test targets. A QEMU executable's presence confers no
ROS support tier, compiler/sysroot qualification or SDK parity.

| Target family | Product/application scope | Additional evidence required |
| --- | --- | --- |
| x86_64 Linux | All products; ROS native integration | Real selected ROS release/RMW and typed application delivery |
| aarch64 Linux | All products; required robot ROS integration | Matching executable, loader, C++ runtime and native agent |
| arm32 Linux | Historical ROS 1 migration and portability | Exact ABI, floating-point convention and release dependencies |
| ppc64/ppc64le Linux | Additional UWVM portability | Separate byte orders, native word sizes and wire-codec checks |
| mips64/mips64el Linux | Additional UWVM portability | Explicit ABI and byte order; separate target sysroots |
| riscv64 Linux | Additional UWVM portability | Exact ISA extensions, ABI and loader |
| sparc64 Linux | Additional UWVM portability | Big-endian loads, alignment and codec behavior |
| loongarch64 Linux | Additional UWVM portability | Target ABI, loader and runtime dependency availability |
| i686 Linux | Ordinary/stable portability | 32-bit pointer, size, overflow and alignment behavior |
| Windows x86_64/i686/arm64 | Ordinary/stable full-system testing | Matching PE image, guest OS, toolchain and owned disk overlay |

The extra CPU rows are validation targets, not assertions that upstream ROS or
this SDK currently supplies those native backends. A native x86_64 broker serving
an emulated aarch64 runtime proves that particular cross-platform transport path;
it does not establish native aarch64 RMW qualification.

## Choose the right emulator boundary

Linux user emulation runs a target-ISA user program against host system-call
handling. Use it for ISA/userspace ABI checks with a matching target loader and
sysroot; it does not boot the target's kernel. See
[QEMU user emulation](https://www.qemu.org/docs/master/user/main.html).

A typical invocation, after constructing and identifying the target executable,
is the following Bash example. Set the runtime-option array from that product's
documented backend recipe; an empty array is appropriate only when its default
backend is intended. The example assumes the fixture accepts the argument `17`.

```bash
UWVM_TEST_RUNTIME_FLAGS=()  # Preserve the selected product/backend options.
"$UWVM_QEMU_BINDIR/qemu-aarch64" -L "$UWVM_TARGET_SYSROOT" \
    "$UWVM_TARGET_BINARY" "${UWVM_TEST_RUNTIME_FLAGS[@]}" \
    --run "$UWVM_TEST_WASM" 17
```

The controller must record actual resolved tool paths, versions and hashes;
absolute target/sysroot paths; the exact argument vector; and output/ownership
closure. A successful `--version` call is tool discovery, not language execution.
On the provided Linux host, QEMU tools were discovered under
`/home/macromodel/.local/bin`, outside the initial controller PATH. Their paths
and existing Windows image-store directories are inventory evidence only.

Use full-system emulation for Windows and target-kernel/OS-version behavior.
Inspect an inactive image's format, preserve its backing chain, and create an
owned overlay on the existing data volume. Copy mutable firmware variables into
the owned test directory as well. Keep original image stores intact and retain
the overlay until results have a recoverable, independently checked backup.
QEMU documents offline image operations and backing-file format selection in
the [image utility manual](https://www.qemu.org/docs/master/tools/qemu-img.html).

## Record independent qualification levels

| Level | Required result |
| --- | --- |
| Producer | Actual native/Wasm Debug and Release construction with exact compiler/dependency identities |
| Inspection | Module features, imports and debug sections recorded without executing them |
| Ordinary execution | Positive and invalid-argument outputs/exits compared with the fixture's native/reference oracle |
| Wasm debugger | Genuine stop, step, locals/stack workflow, breakpoint lifecycle, restart and clean shutdown |
| Source debugger | Original file/function and line, genuine paused frame, expected value before/after stepping |
| Language objects | Real producer layout and values; no replacement by a C-linkage scalar result |
| ROS application | Real typed topic/service/action delivery with a native ROS peer and explicit policy-denial controls |
| Performance | Matched workload and runtime strategy, steady kernel and startup separated, valid hardware samples |

A parsed DWARF file is not a stopped-frame result. Native Mono DWARF is not
automatically C# managed locals, and a source-language scalar does not certify
containers, lifetimes or asynchronous frames. Keep each level's actual result
and refusal separately. The fifteen default-on language debugger switches
remain independent of ordinary execution and resource grants; see the
[option guide](../../../docs/debugger/language-options.md).

For non-ROS examples, compile and run the documented language fixture. For a ROS
row, use an isolated application namespace and a real peer that verifies the
payload and response semantics. Include forbidden-operation controls and prove
that a denial has no side effect. Record the runtime, native agent, SDK schema,
RMW, namespace/domain and grants. A stub, model-only transport or successful
import registration does not prove ROS delivery or official API parity.

Native `.so`, `.dll` and `.a` inputs are host artifacts, not Wasm libraries.
Source-level migration needs a Wasm build and its admitted SDK interface, or an
explicitly identified native-agent operation. Keep that migration distinct from
a new application using the SDK. Permission and audit comparisons must describe
the actual policies and observed operations on both systems; the container and
Wasm labels alone do not establish a security advantage.

## Performance and retained evidence

QEMU timing describes the recorded emulation/virtualization setup. Host PMU
samples of a TCG process cannot be relabeled as the target CPU's native
microarchitecture behavior. A guest-PMU result needs its own event-availability,
attribution and sampling validation. Native hardware comparisons retain separate
VTune hardware-hotspot and microarchitecture runs, including collection warnings
and sample counts. Short or otherwise unqualified collections remain ineligible
for interpretation.

Each row binds the runtime source and executable SHA, native agent identity,
producer/Wasm hashes, backend/language options, emulator/CPU/accelerator, guest
OS and sysroot, complete commands and streams, process ownership, resource
brackets and expected outcomes. Keep data/metadata tests separate from executable
code-cache authentication, binary-update invalidation and CPUID/OS-state checks.
Archive data as inert regular files, independently read every role through the
compression trailer, and clean only exact owned files with verified recovery.

As of this document's creation, QEMU path/image discovery adds no new platform
execution pass. The current 210 adapter combinations and 24 linked-Rust metadata
combinations have their own narrower evidence packets. Architecture, managed
debugger and full ROS/native performance matrices remain separately qualified.
