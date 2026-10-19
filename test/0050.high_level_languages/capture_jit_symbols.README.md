`capture_jit_symbols.py` is optional attribution metadata for an ordinary,
caller-owned target blocked at the first profile gate. It never attaches a
debugger, calls ptrace, signals the target, releases stdin, or writes target
memory. Do not point it at other agents' targets. Its output is independent of
hardware-count qualification: an unavailable symbol snapshot cannot turn an
otherwise valid raw hardware measurement into a named Wasm hotspot.

The coordinator must observe READY and keep G withheld throughout capture.
Use a fresh output directory and pass the exact product and Wasm identities:

```sh
python3 capture_jit_symbols.py --pid OWN_READY_CHILD_PID \
  --binary /tmp/high-level-20261017/products/uwvm2/uwvm \
  --wasm /tmp/high-level-20261017/engines/gated/artifacts-r1/assemblyscript/assemblyscript-gated-release-2000000.wasm \
  --expected-binary-sha256 PRODUCT_SHA256 --expected-wasm-sha256 MODULE_SHA256 \
  --out /tmp/high-level-20261017/OWN_NEW_RESULT/jit-symbols
```

Run only in the original exact 64-GiB/swap0 container, on a spare admitted CPU,
before starting collection. The importable API is
`capture(pid, binary, wasm, out, expected_binary_sha256=None, expected_wasm_sha256=None)`;
it returns the persisted receipt, including `passed=false` on metadata failure.
The caller must require `safety_checks_passed=true` and
`fatal_safety_failure=false` before releasing `G`, even when metadata itself is
unavailable. Identity/admission/capability failures prevent release. A pure
metadata failure can leave raw hardware counts valid but supplies no names.
The CLI returns nonzero for failure. Expected hash arguments are optional for
exploration, but production attribution callers should pass both and match the
receipt's boot/PID start time/product/Wasm hashes to their collection. The
helper records but cannot independently prove the caller's READY observation.

The helper reads product and mapped `libLLVM*.so*` ELF symbol tables, verifies
device/inode mappings and PT_LOAD load bias, and finds
`__jit_debug_descriptor`. It requires the 64-bit little-endian x86-64 ABI,
descriptor version1, and consistent bounded next/prev lists. It saves only
complete registered ELF objects and their hashes, plus metadata and validated
function ranges. No arbitrary heap dump is produced. Bounds are 16,384 entries
per descriptor, 128 MiB per registered object and 512 MiB total. Each saved
ELF also preserves at least a 512-MiB free-space reserve. Every target
thread must remain in the original cgroup with all five capability masks zero,
NoNewPrivs1 and TracerPid0. Actual `/proc/PID/exe` must be the supplied product.

For registered ET_REL objects, the address is section `sh_addr` plus symbol
`st_value`. This follows the local LLVM RuntimeDyld implementation, which
patches loaded section addresses in the GDB debug ELF while keeping symbol
offsets. Only nonempty STT_FUNC ranges wholly inside actual executable target
mappings are accepted. Zero-sized, unallocated and unsupported symbols remain
explicitly skipped; aliases can yield multiple native names for one range.

The exact Wasm module-name custom section, or otherwise the literal target
`--run` path, determines the module prefix. The installed system
`libxxhash.so.0` computes standard seed0 XXH3 in the inspector only; its hash is
recorded. The prefix must actually match native symbols before a Wasm index
is bound to this module's hash. Other module prefixes keep their native ranges
but receive no guessed names from the supplied Wasm. Missing xxhash, renamed
modules, unsupported maps, missing permissions/symbols, or mismatched prefixes
produce a failed receipt instead of fabricated attribution.

Before publication, the helper rechecks PID start time/tasks/capabilities,
maps, descriptor/list metadata, all registered ELF bytes, source helpers,
binary/Wasm/mapped-library hashes, boot and admission. These checks establish
consistent boundary snapshots while the caller holds the gate. They are not
an atomic history of every possible concurrent target change; keep the target
quiescent. The capture stores no actual sample IPs and does not prove that
VTune exports absolute IPs for anonymous JIT mappings.

The installed VTune2026.4.0 build632893 successfully exports absolute sample
IPs from the completed setup pilot with `-report hw-events -r RESULT
-group-by process-id,thread-id,cpuid,core-type,address -show-as samples`.
[The offline join helper](engines/JIT_ATTRIBUTION.md) preserves the actual
sample columns and binds the report to this same snapshot. Join absolute IPs
only against this same process's validated half-open function ranges. An
unknown aggregate, an ambiguous alias/overlap, or a module-relative address
without a verified load base must remain unattributed. Never reuse addresses
across products, processes, runs or boots.

GDB's [JIT interface](https://www.sourceware.org/gdb/current/onlinedocs/gdb.html/JIT-Interface.html)
describes the registry, but this helper avoids GDB's normal internal breakpoint
writes entirely. Intel's [2026 JIT API documentation](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-0/jit-profiling-api.html)
documents event-based JIT profiling for launch targets; it does not establish
support for our attach scenario or Linux perf-map files. Intel's
[source-object documentation](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2024-2/source-object.html)
describes address-level assembly views. No production listener or target
semantics are changed.

Function-range attribution means an original Wasm function index/name. It does
not prove source lines, native inline callees, C# managed methods, or JS
bytecode functions. Mono/QuickJS interpreter functions retain that runtime
scope. The first real setup pilot captured one ELF and 50 validated function
ranges; its same-process offline join matched all 85 retired and 11 reference
clock samples. Its performance eligibility remains false, and it demonstrates
the attribution mechanism only. Final profiles retain their own independent
snapshot, collection and qualification receipts.
