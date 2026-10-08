# Current macOS arm64 debugger acceptance

These new files are source candidates. No native API, Python unit suite, Mac
payload, compiler, debugger or SSH command was run while writing them. The
Windows r2 six-file packet and production runtime/controller are unchanged.
Only the keeper may schedule actual native execution, serially, with no other
test/build occupying the same Mac resource allowance.

The first native command is a real owner probe, not a product test:

```sh
python3 test/0017.runtime/macos_owned_debug_process.py \
  --out /tmp/uwvm-mac-owner-fresh-01 --orphan-witness -- /usr/bin/true
```

It must record successful exact inherited/read-back `RLIMIT_AS=4294967296`,
a successful 16 MiB private mapping, an oversized mapping rejected with
`ENOMEM`, both actual `fork` and `posix_spawn` rejected with `EPERM` by the
fixed inherited sandbox, real root/witness PID+start-second+microsecond+UID+
PGID identities, and a real reparented witness after the root exits. The root
is **not reaped** until the owned group contains only that root; only then
does `wait4` reap it. Final group inventory must be empty. `killpg` always
uses the still-unreaped owned root as its PGID anchor; it never kills a
process selected by a name or a previously observed unowned PID.

The supervisor takes a private per-user `flock`, sets its own AS/core limits,
and admits only one payload at a time. Each root is a new session/group with
private startup pipes and regular `CREATE_NEW`-equivalent output files. Before
guest entry the guard closes the private proof/gate descriptors. The optional
witness is explicitly created and stopped by the trusted guard **before** its
no-fork policy, registered from real kernel identity, then runs the same
policy/probes. It closes management stdin and never executes guest code.
No-fork remains in force through the true payload `exec`; guest Wasm threads
are threads of the same admitted PID. This executor does not admit arbitrary
compiler children or an unrestricted fork tree.

`proc_pidinfo` and `proc_listpgrppids` use the public SDK layout with returned
size/count checks. The latter's public wrapper returns **PID count**, not
bytes; an extra inventory slot detects truncation. PID reuse, UID/group
changes, unknown post-admission members, incomplete inventories, output
growth, timeout or missing native statistics fail closed. Cleanup continues
after an RSS/swap qualification failure; no next payload is admitted unless
the owned group was completely retired. A retirement failure is a fatal
keeper stop: preserve the root/witness births and inspect the remaining group
before admitting any later run.

The RSS observation includes the supervisor and every admitted group member,
including the registered witness after reparenting. A 3 GiB stop threshold
leaves 1 GiB headroom below the requested 4 GiB test ceiling. Samples are
20 ms observations; they are **not an aggregate kernel hard cap** and cannot
prove that no short transient exceeded a threshold. `wait4` peak is only the
root peak and is recorded separately. `vm.swapusage.used` and
`host_statistics64` cumulative `swapins`/`swapouts`/`swapped_count` are checked
before, during and after the run; any observed global swap activity invalidates
qualification, even if another application caused it. None of this disables
swap globally or alters product memory protections.

The installed host was read-only identified as arm64 Darwin 25.6.0,
XNU 12377.161.14~5. Apple's published [XNU 12377.121.6 resource
implementation](https://raw.githubusercontent.com/apple-oss-distributions/xnu/xnu-12377.121.6/bsd/kern/kern_resource.c)
sets a VM map-size limit for `RLIMIT_AS`; it is not exactly this installed
release. Actual set/readback **and the map probe** are mandatory instead of
claiming an exact-source kernel proof. AS is virtual mapping allowance, not
RSS: dyld shared mappings or the VM's legal mmap safety reservation can cause
`ENOMEM` below 4 GiB RSS. Preserve this as a resource/platform qualification
restriction; never remove VM guard pages, silently enlarge the allowance or
count an old binary as a current debugger PASS.

Apple's [public event flags](https://raw.githubusercontent.com/apple-oss-distributions/xnu/xnu-12377.121.6/bsd/sys/event.h)
and [kqueue implementation](https://raw.githubusercontent.com/apple-oss-distributions/xnu/xnu-12377.121.6/bsd/kern/kern_event.c)
reject `NOTE_TRACK`/`NOTE_CHILD`; `NOTE_FORK` alone cannot identify every
child. This is why the closure uses an actually probed no-fork policy instead
of a polling PPID tree. The [kernel group PID enumeration](https://raw.githubusercontent.com/apple-oss-distributions/xnu/xnu-12377.121.6/bsd/kern/proc_info.c)
includes both live and zombie lists; root birth/group validity still requires
the actual installed-kernel probe. If `/usr/bin/sandbox-exec`, the fixed
profile, public ABI, Mach counters or AS probe is unavailable, refuse this
qualification. No private sandbox API, sudo, tracing injection or product
policy workaround is used.

## Fresh source/build and official fixtures

Current joint source stepping requires the complete current fused Stage3v1
runtime/compiler/debugger dependency freeze. An older r10 PE, old preloader
provider, or earlier standalone producer object does not qualify. Linux build
and staging remain in the keeper's admitted 64 GiB/no-swap cgroup.

`stage_macos_debug_current_product.py` consumes an actual schema-1 build
receipt with purpose `actual-current-macos-arm64-debug-full-build`, repository
`ordinary`/`ros`, target `arm64-apple-macos`, resource fields `memory_max`,
`memory_swap_max`, `cpuset`, unchanged zero OOM counters, `source_before_file`,
`source_after_file`, canonical `source_id`, `production_pins`, `product_link`,
`product_compiles`, `debug_info_dwarf_archive`, `framework_inputs`,
`protected_mig` and `actual_inputs`.

The fingerprint must cover **all actual `src/` and `third-parties/` files**.
`production_pins` uses the exact relative paths in the stager, including the
joint API, debug-only wrapper, current private-leaf Stage3 compiler/runtime
source. Each compile/link record retains original direct LLVM `argv`, `cwd`,
`returncode=0`, tool/raw-log/output paths and original SHA256s. Fresh actual
`uwvm_runtime.default.cpp` and `main.default.cpp` object outputs must be
consumed by the actual link. No env wrapper, reconstructed command or unresolved
response file is accepted. The actual thin arm64 Mach-O retains the same
source ID and consumes a real `DebugInfoDWARF` archive.

`framework_inputs` maps `Security` and `CoreFoundation` to their actual selected
SDK stub `{path,sha256}` records. The original link uses both frameworks.
`protected_mig` preserves the **actual direct** `mig` argv including
`-DMACH_EXC_SERVER_TASKIDTOKEN_STATE=1`, `cwd`, `returncode=0`, `tool_sha256`,
`log`, `log_sha256`, `sdk_root`, `server` and `header`. Its actual selected
`mach_exc.defs`, generated server/header and framework stubs have original
`actual_inputs` pins; the fresh generated protected server is compiled into
the same product. Do not manufacture a direct command by expanding a historic
`xcrun` log. A different original-record format needs an independently reviewed
adapter retaining those records, not a guessed record.

```sh
python3 test/0017.runtime/stage_macos_debug_current_product.py \
  --source-root "$FRESH_SOURCE" --product "$FRESH_ARM64_MACHO" \
  --build-receipt "$ACTUAL_BUILD_RECEIPT" --repository ros \
  --out "$NEW_PRODUCT_QUALIFICATION_DIRECTORY"

python3 test/0017.runtime/stage_macos_debug_current_fixture.py \
  --source-root "$FRESH_SOURCE" --out "$NEW_FIXTURE_DIRECTORY" \
  --clang "$ACTUAL_WASM_CLANG" --wasm-ld "$ACTUAL_WASM_LD" \
  --wasm-tools "$ACTUAL_WASM_TOOLS" --llvm-dwarfdump "$ACTUAL_DWARFDUMP"
```

The fixture stager runs official Clang C17 `-O1 -g -gdwarf-5`, wasm-ld,
wasm-tools validation and LLVM DWARF verification/full line oracle. It requires
real nested/repeated inline DIEs, actual complete statement sequences, and
zero imported functions before treating local Code indices as module function
indices. It separately officially encodes/validates the three **no-linear-
memory** fixed numeric WAT files. No invalid raw replacement body is labeled
an officially valid module. Objects/modules/tool/source inputs and raw logs
are retained with SHA256 and checked before/after. Staging PASS is not native
Mac execution PASS.

Copy only the qualified fresh Mach-O, required exact arm64 runtime library
closure, compact product qualification, complete small fixture directory, and
this source candidate to Mac. The keeper separately qualifies actual Mach-O
load commands/system SDK/runtime dylib closure before execution; this driver
does not fabricate that closure. No `DYLD_*` override/injection is admitted.
Keep original cross-build tools/objects/archive inputs/logs on Linux.

The minimum Mac packet execution is:

```sh
python3 test/0017.runtime/run_macos_debug_current_cli.py \
  --source-root "$FRESH_MAC_SOURCE" --repository ros --product "$FRESH_MAC_PRODUCT" \
  --product-qualification "$PACKET/product.qualification.json" \
  --qualification-sha256 "$ACTUAL_QUALIFICATION_SHA256" \
  --fixture-receipt "$FIXTURES/fixture.receipt.json" \
  --fixture-receipt-sha256 "$ACTUAL_FIXTURE_RECEIPT_SHA256" \
  --wasm "$FIXTURES/numeric.wasm" --replace-wasm "$FIXTURES/replace.wasm" \
  --replacement-wasm "$FIXTURES/replacement.wasm" \
  --subset no-memory --out /tmp/uwvm-mac-current-numeric-fresh-01
```

Only after the true owner and no-memory native paths qualify, rerun in a new
output directory with `--subset source`, then `--subset all` when the C module
is compatible with the actual AS allowance. Change repository/product/packet
for the ordinary product; never reuse the ROS build receipt as an ordinary
product qualification. Tools/compiler/link work do not run on Mac through this
driver. If a needed Mac build uses external compiler children, this no-fork
executor refuses it; use a keeper-qualified Linux cross-build with the genuine
Mac SDK and full arm64 library closure, or separately qualify a serial low-
memory direct compile/link ownership closure.

## Exact first-slice assertions and remaining scope

The default `all` subset is seven actual console cases, both diagnostic policies:

- Two no-memory cases execute a real emitted Wasm breakpoint/step, then two
  consecutive hardware/Mach native traps. Nonempty decoded instruction bytes
  and mnemonic, actual FROM→TO chain, selected participant, fresh stop labels
  and native PC equal to TO are required. Native traps must explicitly reject
  reuse of a previous Wasm/source position. Guest result must exit zero.
- Two replacements run the first call and verify old result 12, stop before
  the second call, reject malformed raw body without changing stop/publication,
  then replace an **inactive** exact `(i32)->i32` function at generation 1→2.
  A fresh stop label is required and the current unreplaced caller's runtime
  epoch must stay unchanged. The second call must yield new result 13. Global
  runtime epoch and per-function replacement generation are distinct.
- Two source finish cases find the actual C5 O1 leaf statement, request source
  `out`, and require the actual outer caller's official Code-relative statement
  row, matching file/line/column, no forged caller PC and a fresh real stop
  label. The module checks 52/58 and must exit zero.
- One unsupported mode must exit with ordinary status 1..255 and the actual
  colored fatal formatter's `unsupported in the current mode: llvm-jit/lazy`
  (ordinary) or `uwvm-int/full` (ROS). A signal crash, loader failure or parser
  error cannot satisfy this case.

Every console child has an owner receipt with actual argv/root births,
startup proofs, whole admitted group retirement, AS/RSS/swap observations,
raw stdout/stderr and hashes. The transcript is an oracle, never a source
read/value/activation capability. `quit` is an exit command and is **not**
followed by a prompt wait. The source and official input/build packets must be
unchanged at final qualification. No preloader `-Wpre` path is used.

This slice does not claim C++/Rust source next/into, variable evaluation,
server/IDE transport or secure midrun attach acceptance on Mac. Those existing
old platform runners are not replaced or treated as current proof. Current
Mac x86 native stepping remains unsupported; native acceptance here is actual
Apple arm64 protected-Mach backend only. Enhanced Security or a legitimate
map-size restriction stays unavailable, not an excuse to bypass the backend.

The new pure unit source covers birth reuse/microsecond/group/UID rejection,
reparenting, aggregate sampled budget, public structure sizes, thin arm64
Mach-O byte inspection and strict selected-stop protocol negatives. The keeper
may run it in Linux's admitted cgroup:

```sh
python3 -m unittest discover -s test/0017.runtime -p test_macos_owned_debug_process.py
```

Those synthetic tests never execute a Mac payload or grant debugger authority.
AST inspection during source development is not a unit-suite PASS.
