# LLVM GC stack-map carrier probe

`run.py --out ABSOLUTE_NEW_DIRECTORY` compiles `carriers.ll` with the selected
LLVM `opt`, `llc`, and `llvm-readobj`. It checks that an `addrspace(1)` value live
across a call appears in the statepoint stack map, while an `i64` or tagged
`{ i64, i64 }` value does not appear as an implicit GC root. The default cross
targets cover Mach-O arm64, ELF x86-64/RISC-V64, and COFF x86-64. A target
triple controls object generation only: this does not execute target code.

The fixture tests a possible compiler mechanism, not the product JIT or its
16-byte `gc_reference` layout. It cannot clear the GC reclamation release
blocker. The product needs precise roots through all backends and threads,
object-graph collection, stale-handle protection, and actual RSS/pause gates.
For macOS, run through `test/0017.runtime/macos_rss_limit.py` with a limit below
4 GiB. For the requested formal Linux rerun, use the configured 64 GiB cgroup.

`run_explicit_stackmap.py --out ABSOLUTE_NEW_DIRECTORY` separately compiles
`explicit_stackmap.ll`. It splits one `i128` carrier into 64-bit payload and
kind values, passes both to `llvm.experimental.stackmap`, and keeps both live
across an allocator call. LLVM's [Stack Maps documentation](https://llvm.org/docs/StackMaps.html)
defines the intrinsic as an explicit location request for arbitrary live values;
this differs from the automatic managed-pointer discovery above. The LLVM 23
development toolchain produced two register locations in each of the four
Mach-O/ELF/COFF object formats under `-O3`, with no forced instruction at the
map site. The [Mac evidence](/Users/liyinan/Documents/MacroModel/src/uwvm2/build/wasm3-evidence/gc-explicit-stackmap-macos-llvm23dev-20260927-r2/summary.json)
records exact tool, fixture, object, map and disassembly hashes; the whole
probe peaked at 45,481,984 bytes under the 4 GiB process-tree watchdog.

This is a representation experiment only. The product currently has no exact
live-reference enumeration, return-PC-to-map binding, callee-saved register
recovery, coordinated safepoints, object retirement, or collector. A bare stack
map immediately before a call does not prove that a runtime invoked inside that
call can recover the prior instruction's register values. The fixture also uses
the development LLVM 23 toolchain, not a certified bundled 23.1.1 build.

The [collector integration checklist](collector_integration.md) records the
product root sources, safe-point handshake, cross-store lifetime constraints,
and measurable release gates that remain after these representation probes.

`run_shadow_root_frame.py --overlay INCLUDE_ROOT --out ABSOLUTE_NEW_DIRECTORY`
tests a second mechanism: frame-owned arrays of complete integer-carried
references, published through an opaque runtime call. The LLVM optimizer keeps
those escaped slots observable at allocating calls. This avoids having to
recover volatile registers from a stack map in the standalone experiment.
`INCLUDE_ROOT` must contain the pinned exclusive local-only sweep prototype;
the runner checks its SHA before compiling. The prototype is preserved with
the [Mac evidence](/Users/liyinan/Documents/MacroModel/src/uwvm2/build/wasm3-evidence/gc-shadow-roots-macos-20260928/manifest.json).

Both repository probes executed 16 real **experimental local-only** collections
and reclaimed 27 objects: twelve caller refs below a nested call, a selected
branch PHI, seven allocating loop iterations, and root-frame cleanup during
an Itanium C++ exception. A negative control publishes only eleven of the
twelve still-live caller refs; membership checking then rejects the omitted
reference and aborts with its expected key. This demonstrates that the test
really collects, rather than implicitly rooting the input C++ array.

LLVM 23 development tools also generated O3 Mach-O arm64, ELF x86-64/RISC-V64/
PowerPC64 big-endian/i386, and COFF x86-64 objects. The 32-bit probe uses an
8-byte complete carrier and 32-bit `size_t`; the 64-bit probes use a 16-byte
carrier and 64-bit `size_t`. These cross rows are object generation only.
Native arm64 execution used Apple Clang 21 for the ASan/UBSan C++ collector
and bridge; the independent `llc` object is not ASan-instrumented. macOS leak
sanitizer was disabled. Peak process-tree RSS was 421,117,952 bytes (ordinary)
and 426,688,512 bytes (ROS), each under the 4 GiB watchdog. Product source IDs
were unchanged during both probes.

This is still outside `src`: it does not enumerate product JIT/interpreter
locals or operands, coordinate concurrent guest/host readers, collect foreign
stores/extern bridges/exnrefs, or implement Windows SEH cleanup. The product
GC release gate must continue to fail. Formal Linux replay requires the
64 GiB, zero-swap cgroup and `--cgroup` pointing to its visible directory.
