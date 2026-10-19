# R6 native-frame inspection Source successor

The real native-frame runner now requires nonempty instruction evidence for
each policy's selected function. A successful llvm-objdump status, an ISA
header, zero counted calls and an existing CFI section cannot qualify an empty
unwind disassembly. The runner uses the already captured ELF extent reader to
select the actual function's complete section/address/size range, requires its
first instruction at the real symbol start, rejects invalid/out-of-range or
non-increasing instruction addresses, and counts calls only in instruction rows.
Local labels do not truncate the selected symbol; relocations do not count.
The actual object ELF machine determines the supported x86-64/AArch64/RV64
little-endian target, and RISC-V keeps no-aliases call decoding.

Future Linux classifier-only negative cases cover empty output, file headers,
function labels, relocations, wrong start/range, duplicate addresses and an
undecoded row. These are test Source, not substitute Guest/tool/cache receipts.
Real cold/store-complete, warm/authenticated-hit, unchanged object bytes and CFI
checks remain mandatory. No cache origin or process/owner gate is relaxed.

Apply this private Source successor only after the complete predecessor chain:
production122 (checkpoint15 already included), approved Hostsignal3 overlay,
codec33-r4, CLI27-r2, cross12-r1, integration12-r1, inspection9-r1,
reviewed54-r4 and Source-bytes51-r5. R6's immediate before bytes are exactly
r5 after bytes. Original packets, historical failures and composition r2 stay
immutable. The complete original r5 independent review report is pinned in
this successor's source-inputs data (320253 bytes, SHA256
`6aa0fa74853e7db5357f2efdc5011e11fc0d4f13f506e29097930400d55b3d0a`).
It requests the single retained R5-09 inspection correction. A complete
independent r6 Source review and Root review remain required before composition
or public installation. The predecessor r5 standalone README's empty final
historical-r4 heading is omitted only from this new standalone README; its
original bytes and the product document's populated historical notes survive.

The external Source closure is still exactly 33 captured nodes per repository.
Use this successor's `closures/{repo}/source-closure.json` and its externally
admitted SHA256, not the predecessor's data: the frame entry bytes changed.
All unchanged closure nodes retain their predecessor bytes and hashes. R5's
captured-byte loader, keeper entry literal, private module identity, complete
transitive admission and trusted interpreter/startup/stdlib boundary remain
unchanged. This is local captured Source closure isolation, not a hermetic
Python sandbox. Shared LLVM provider identity is outside this test delta.

Only author-owned text/JSON/hash and AST static processing took place locally.
No candidate import, executable compilation, generation, tests, Native/Guest/
DBG/QEMU/VTune execution, SSH or public installation occurred. Actual execution
remains pending in the authorized original SSH Linux 64GiB/swap0 cgroup under
the independently admitted keeper. This packet grants no SDK scan, security
runtime or performance credit. Historical Source documentation follows.

---

# r5 exact captured Source successor (private, unexecuted)

This single successor addresses independent Source findings R4-07 and R4-08.
The complete fixed review input is118517 bytes, SHA256
`e6425d3a1d606d944eb0bdc4dea850324ea208ca9a26644410d3b9d92990588e`.
It changes test/framework/docs only, never frozen production cache, origin mint,
process sealing, ISA/CPUID, permission or checkpoint gates. It is HOLD for a new
complete independent actual-after review. Original r4 and full composition r2
remain unchanged and retain their REVISE/HOLD status.

## Exact order and Source identities

Apply r5 only after reviewed-successor-r4 READY SHA256
`f7fb439b60570f9154d36c909b6c22e32220759519524c92da7d01198e8e093d` and its
already-documented predecessor order. The reference full224-path composition r2
READY is `81b941e61b85677e0505344bde4d3719adeb061114a77bbb12ff474f216ddbbe`;
it is a Source preimage reference, not authorization to install or evidence that
r4 passed review. r5 is an exact immediate-before increment; it does not replace
or recompose that224-path cohort before independent review. checkpoint15 stays
included once in production122. Root's StageA, SignalCLI3 overlay, current27
support pins and16-file signal receipt remain the same separate dependencies.

The helper Source SHA is `3475afc5e7f08f8cddb45d009d901412280532e88aca45bcbd6d963b395ef3d7`; keeper-entry.fragment is
`cae2c05f7df6e9a2a94a84f5333eeef5c1946d96ea8b54cfe59eee5e17d23d21`. Each closures/<repository>/source-closure.json is externally
pinned DATA describing33 complete current Python Source nodes, including all
14 entry drivers, the byte loader/BoundedCli, owner/RSS/retained Runner, actual
frame decoder, inspectors, exact trap classifier and recursively discovered
numeric/matrix/oracle/case helpers. The original helper bodies remain unchanged
where no r5 delta is required. closure-review-source/ retains every declared body
for Source review. The data manifest has no self-referential embedded hash.

## Trusted startup and captured entry

Before the first candidate instruction, the real keeper must independently pin
and admit the Python executable, standard library, extension modules, startup
policy, exact keeper command literal, exact direct entry Source and complete
closure/config/tool/product/fixture identities in the released original Linux
cgroup. Existing C++ system/shell/Python parent ownership remains its external
responsibility. Nothing running in Python can retroactively authenticate a
malicious interpreter, startup shim or standard library.

All future direct entries MUST use the reviewed keeper command DATA, not bare
python entry.py. Construct the owned argv as Python,-I,-S,-c, the exact text of
keeper-entry.fragment, its SHA, expected entry SHA, canonical absolute entry path,
then the original entry arguments. C++ parent calls embed this same literal.
The literal requires Linux, isolated/no-site/safe-path and optimization0, checks
the externally pinned original guard, reads entry Source once through a bounded
owned regular-file descriptor, hashes that SAME captured bytes object, then
compile/exec uses precisely those bytes with filename/argv/__main__ script
context. It passes the captured entry bytes into the entry scope. bind_entry
requires equality with the complete closure capture. A second path open by
Python is therefore not substituted for the byte snapshot already checked.

The keeper literal itself and its command arguments are part of the external
trusted startup boundary. A marker naming its SHA is not self-authentication;
the actual keeper must verify that exact -c code and interpreter before launch.
The entry's subsequent path/hash check is consistency, not retroactive proof.
-B only suppresses bytecode writes and is not a Source identity or security gate.
-I/-S are required startup controls, not a claim of total Python hermeticity.

Set UWVM2_CACHE_EXECUTION_GUARD and UWVM2_CACHE_EXECUTION_GUARD_SHA256 to the
canonical owned guard JSON and its externally fixed SHA. Set
UWVM2_CACHE_SOURCE_CLOSURE and UWVM2_CACHE_SOURCE_CLOSURE_SHA256 to the exact
repository's immutable closure DATA and its externally fixed SHA. Bootstrap must
verify Linux boot, actual membership-derived cgroup directory/original inode,
64GiB and swap0 BEFORE compiling a candidate helper. The JSON release note is
DATA and does not authorize entering the exclusive SSH execution window.

## Complete local closure and module context

Each entry's earliest bootstrap imports standard-library code only. It reads
cache_cli_process.py once, checks the hard Source SHA and compiles/executes that
same captured byte object. The bootstrap helper imports stdlib/defines code only;
before any other candidate helper executes, SourceContext completely captures
and pins the whole33-node closure with stable fd identity, regular/euid-owned
files, canonical root-bound paths, per-file2MiB and incrementally enforced16MiB
Source-byte caps. A load never substitutes a later disk read, cached code object,
SourceFileLoader or ambient ordinary sys.modules entry for that capture.

Known local imports use the context's own captured-byte load and module map.
Transitive normal imports get a context-bound __import__ hook with declared local
edges and declared stdlib roots. Old plain aliases and their .pyc files are
ignored. One nonce private package and its fresh module names are registered
before execution, solely for context-owned objects, so dataclasses/typing and
__module__ lookups find the actual object. No pre-existing private entry is
adopted. Loading-cycle/reentry fails; ready modules can be reused. Actual numeric
case helpers have deferred function-level imports, which remain available after
the defining numeric module is ready.

Every local module gets canonical __file__, private __name__/__package__, its
ModuleSpec/loader/origin/has_location, cached=None and an injected context. The
captured loader get_source and owned linecache entries expose captured text for
introspection. The current closure consists of flat modules; declared private
sibling-relative imports have a limited context path, and new relative/package
shapes require separate Source review rather than ambient filesystem fallback.
Reload/arbitrary module_from_spec execution is rejected. inspect.getsource and
other stdlib behavior remain within the independently trusted library boundary.

Failed module loads discard only that context's newly registered module and
linecache objects. Every direct entry closes the context in finally; successful
completion audits Source/module/import-policy identity, then restores/removes
only owned objects. It never deletes another context's module or adopts an
ambient alias. The source-byte holder is test identity plumbing, not a Python
sandbox, Runtime credential or cache authentication issuer.

All active helper paths are changed: BoundedCli owner/RSS loads, direct helper
attributes, inspectors and configured decoders use captured load/load_path.
The current two-image shim loads the exact original qRunner/owner/RSS bodies
through this context and passes them to Runner. Historical qRunner's own
SourceFileLoader/load_module definition remains byte-exact and is NOT called by
that shim; unused historical qualification cases are not migrated or qualified.
The old retained future33 file also stays historical. The sole Python producer
child receives its captured stdlib-only Source bytes through a bounded -I/-S/-c
command rather than reopening/importing a candidate script. Child lifecycle,
original cgroup, RSS, refusal/full positive and semantic gates stay mandatory.

The stdlib allowlist does not authenticate arbitrary standard-library modules or
isolate all Python execution. Their normal importlib/sys.modules/extension
loading remains explicitly trusted and externally pinned before startup. Runtime
Source hashes, flags and before/after audits provide consistency only. No general
Python hermetic sandbox or universal process discovery guarantee is claimed.

## Negative contracts and unchanged qualification

The real generic inspector now calls require_instruction_evidence before adding
each selected function, then requires complete nonempty evidence/rows. Its future
main runs a deterministic pure-classifier negative contract: empty, header-only
and relocation-only output must fail even when the earlier tool status check
succeeded. A single instruction positive control must contain one instruction.
This does not fabricate a successful LLVM tool or Guest execution receipt.

A separate future startup negative contract temporarily puts a benign unrelated
object in ordinary sys.modules['owned_process']; a context load must still return
the captured, privately registered real owner object and expected Source SHA,
then restore that ordinary slot exactly. This is labelled a helper identity
contract, never a cache hit or Guest/permission qualification. No ambient pyc was
created or executed here. Real Linux stale-bytecode/startup/closure/exception
cases and positive CLI/cache/DBG semantics remain pending.

Actual compile/exec/import code above is SOURCE for the future authorized Linux
run. This work performed own text/JSON/hash and AST-only static parsing only;
no candidate module imported, no executable candidate code compiled or executed,
no fixtures generated, no build/link/test/native/Guest/DBG/QEMU/VTune or SSH
command occurred, and no public Source was written. SDK scan/security runtime/
performance credit remain zero. Corev7/CLI45 origin gates and mandatory nonempty
authenticated full cold/store/warm/hit requirements are unchanged. Remaining
legacy tools, full language/ROS/debug/security fault/race tests, architectures,
Windows/QEMU and VTune hardware Hotspots/Microarch comparisons remain separate.

---

# Historical r4 Source contract (superseded for Python loading by r5)

# Reviewed v7 consumer successor r4 (private unexecuted SOURCE)

This is a new successor to the independently reviewed runner inputs. Original
sealed packets, recovered templates, historical fixtures/results and the original
qualification-r3 package remain unchanged. Nothing is publicly installed. The
six-source-finding review input is pinned separately in source-inputs.json. This
packet is a proposed remediation, not a claim that independent re-review passed.

## Exact composition order

First compose fixed corev7-r2 metadata SHA256
`9b90dcec2d23b61189a6e250ed1fa370e84d0fa9d5061f1c01c52f65f637b918`, its62
production after files and real origin Runtime hunks, plus actual integrated
CLI45 production Source. Then apply these exact consumer predecessors in order:

1. `uwvm-cache-checkpoint-contract-v7-source-private-20261018-r1` — READY SHA256 `72528795f5e483c2b8f0c586d5371eb8db6001fe15591f58d176484c5de3ec53`.
2. `uwvm-cache-v7-testcompat-codec-private-20261018-r4` — READY SHA256 `ac9a252766afd63d268a2930249f0fd02501d486e34723a91c25084959f029ad`.
3. `uwvm-cache-v7-testcompat-cli-private-20261018-r2` — READY SHA256 `92edd5f4014637da19924ee7d9c750105b81b6b8ab510209992677573c0b37b7`.
4. `uwvm-cache-v7-testcompat-cross-private-20261018-r1` — READY SHA256 `e3617becc43741762687b499e7e5c5c6879bb144993e5995892a7daee22ba139`.
5. `uwvm-cache-v7-testcompat-integration-private-20261018-r1` — READY SHA256 `62e65148741e4219c891f8ed68c24ae47dfc675cddf3abfca4b347477827d24b`.
6. `uwvm-cache-v7-testcompat-inspection-private-20261018-r1` — READY SHA256 `8e46ccc52b48df0d8690571e8997e3b92c2f11840a29aec5898b53065c342331`.

Finally apply this new successor's exact before-checked hunks. Codec/full identity
components remain DATA-only and unchanged here. The preceding frame-codegen and
inspect_jit edits have exact private-after preimages; this packet pins the final
ordered per-path preimage of the five prior runner/codec packets. No full header
is installed into a public checkout and no missing old auxiliary metadata is
claimed recovered. Root's current ROS StageA support dependency is API SHA256
`2e0db56478e93917baed8d24bdb0bdfaced6f1a79f4a4a9bde59f086bedc632a`, lifecycle
`64c0866b1858a5950b511d1322c2a8f8fb4d73ee49e61b3809908b93690f43a1`, and Runtime
preimage `ad7c954bbcd6494e72ed97a81ecbc7ecba5ef94527749cf5b7402faca5c5f57f`.
Root's new three-path SignalCLI+CacheCLI composition metadata is SHA256
`f61262ea03ee2a7c76e74bd258d2b2a8e217ba06df09b6a006709e8400f8c476`;
its ROS callback/impl.h, params.h and params/impl.h after hashes are respectively
`1003b163fe207d2085211a5d66c005cf389f90a35d4eda1dbee23ab02ec79171`,
`9afb5d57069ab7447aa960114d46ecca57f30dcac6b6baed753caf15c9fe5c37`,
`afc7a9d20c5fa18325136895fc454c75a6d2a6f6780fbdfee0b717e451ca2508`.
It preserves the disjoint signal CLI insertions over original CacheCLI45. This
consumer packet never repackages those production122 paths.
Those are support pins for the separate genuine Runtime/Hostsignal composition,
not permission to overwrite its work or a full DBG qualification.

## Explicit termination and real ownership

The shared bounded helper SHA256 is `50ee71d0cedfde1dd256b2a7921525aa60b0ca5c20561e13e4380935caf2b4cc`. Its default policy still
rejects negative Host signal status. Only an explicit `wasm-fatal-signal` policy
can return SIGILL/SIGTRAP/SIGABRT to a caller which must retain the exact fatal
message classifier and original stack/policy/cache assertions. SIGSEGV/KILL/TERM,
timeouts, retained pipes and any cleanup signalling remain failure. A lifecycle
receipt explicitly says semantic_qualified=false; it cannot qualify a Guest trap.
The original0072 accepts_trap classifier is retained for warm trap cases. Native
unwind additionally uses the same exact-one-fatal classification and all original
frame/CFI checks, rather than admitting arbitrary nonzero exits.

Child stdin is null or the bounded actual debugger command pipe; stdout/stderr
are separate nonblocking PIPEs, with8MiB raw cap, finite command/exit-drain times,
retained pidfd ownership and sampled RSS. Parent files are written after actual
retirement. Every recorded original owned identity must be absent/reused and all
pidfds closed without signalling. Up to16 guard samples and the sampled known
RSS maximum/count are retained; unknown witnesses remain null and no full-lifetime
RSS upper bound is claimed. The retained detached-helper sampling limitation
still applies. All direct helper imports check its exact Source bytes first.

## Correct full positives and distinct refusal

Main's -Rjit really selects lazy_compile, while -Raot selects full_compile. Main's
positive path/integrity/IR/SIMD matrix now uses actual full LLVM (including its
explicit custom full spelling); stable/ROS keep their AOT-only mode matrices. Generated driver
mode selection now follows the actual repository variant: stable and ROS use
-Raot, while the ros flag only selects ROS format/namespace. External profile
configurations for stable/ROS must likewise request -Raot.
Positive pairs require completed cold publication, signature_verified warm reuse
and unchanged snapshots. A nonempty verified-positive pair count is mandatory.
Main's lazy/lazy-verification/tiered and legacy no-sign/no-verify cases are genuine
separate refusal commands: normal Guest semantics, no hit/publication and unchanged
cache snapshots. Legacy unsafe switches remain accepted by actual v7 CLI and
must report origin-authentication-required; they are not assumed parser errors.
Refusal counts cannot replace any full positive requirement.

A new per-product backend-refusal driver separately exercises real lazy/tiered
where those backends exist and real DBG in every product. DBG uses its actual
continue/status/normal-exit/quit protocol, cache request and default dumpability;
it never requests the ptrace-changing seal. It must run the genuine fd_write
CACHE-ORACLE42 fixture normally and leave no object/hit. Its report always says
persistent_cache_hit_qualified=false. It is not full DBG/source/locals/ROS-debug
coverage and cannot satisfy the independent positive CLI gate.

## Original template preservation and stronger current qualification

The retained check_cache_v7_cli_options.py remains byte-exact original future33.
New check_cache_v7_cli_options_current.py preserves those33 actual option/syscall/
cache cases but uses the strict bounded lifecycle/RSS/original cgroup inode
contract. The original per-child8GiB address-space cap is retained explicitly;
parent placement stays under the keeper and child CPU comes from execution_guard.
Cleanup-assisted children or unretired identities cannot pass its
separate semantic qualify step. Its inventory must include the same externally
pinned execution_guard as other current runners. The two-image actual CLI
successor binds --cgroup to the canonical /sys/fs/cgroup directory derived from
actual --membership, plus its external original inode, before any retained Runner
import or candidate command. Its genuine two-image/version/cache assertions stay.

## Reliable cold publication before original traps

Old high_memory_cases.cases() behavior remains unchanged. A new explicit
cache_replay_cases() Source entry appends one empty local seed export AFTER all
original functions. It preserves original _start/OOB operations and their indices,
records this as new Wasm, and retains original WAT separately. A new producer
uses actual pinned wasm-tools parse/validate through the bounded runner. Cold
execution selects only the appended seed with actual -Wstart and must exit normally
with a completed authenticated store. Warm/warm-again select original _start,
require signature-verified hits/stable cache and the original exact OOB classifier.
No fatal-before-async-drain timing is assumed. Cold is explicitly not reported as
execution of the original trapping entry. Historical producer bytes/results are
not promoted to the new fixture contract or claimed byte-identical Wasm.

## Reader and owned analysis/guard children

Authenticated inspection rejects empty profiles, cases, rows, objects, function
selectors and function evidence. It retains the real signed execution prerequisite
and exact transitive v7 decoder Source pin. Frame/object analysis llvm-nm,
llvm-objdump and llvm-readobj children now use the same bounded owned runner,
not unbounded check_output. Legacy cgroup-guard Bash invocations also run only
after original-cgroup admission, with Source pins, finite deadlines, retained
ownership, bounded PIPE capture and natural exit. Generic exclusive test-capture uses that runner too,
keeps its original macro/file permission gates and remains authentication=false.
No captured ELF or disabled-cache semantic result becomes signed replay evidence.

The C++ parent launcher requires an absolute interpreter path and expected Python
SHA (`UWVM2_TEST_CACHE_PYTHON_SHA256`), in addition to launcher/guard pins. It checks
the executing interpreter matches, but the keeper must independently pin Python
and its closure BEFORE starting it; running code cannot authenticate a malicious
interpreter already executing that code. Required execution_guard fields remain
boot_id, original membership, cgroup_inode, cpu, release_note and optional owned
RSS cap. Release-note DATA does not itself authorize the exclusive Linux window.

## Limits and execution state

All candidate imports, compilation, actual fixture generation, linking, tests,
native/Guest/DBG/QEMU/VTune runs and SSH I/O here are0. Only own text assembly,
JSON/data reads and exact byte hashes occurred on this machine. The keeper must
admit the original pinned Python/interpreter and transitive Source closure before
starting any candidate driver; its source-level checks cannot confer retroactive
admission to code already imported by a malicious or incorrectly staged invoker. All actual work
must remain inside the released existing Linux64GiB/swap0 cgroup. Actual process
sealing, self-image/keyring origin, provider/ISA/CPUID, signed store/hit, cold/warm,
DBG/ROS, header/module/noEH, platforms and performance still need real receipts.
This packet fixes review-source contracts; it is not a complete security/runtime
qualification or proof that every inventory consumer migrated. Lazy/tiered
positive persistence and DBG/checkpoint/preload admission remain unsupported;
no production origin gate was weakened. Remaining inventory/historical tools,
full ROS SDK/API audits, architecture/Windows/QEMU/VTune and crash/race/fault
qualification remain separately required. No all-cache-off or skipped plan may
be counted as positive cache completion.
