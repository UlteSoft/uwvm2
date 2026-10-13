# Private checkpoint identity binding proposal, revision 2

This is a mandatory future admission contract, not a production restore feature.
The existing format4 checkpoint DATA codec and running engines are unchanged.
`checkpoint_binding.h` compares and hashes bounded typed identity DATA. A caller
can construct DATA; matching two manifests does not mint a stop, build/provider
certificate, immutable source owner, cache-entry owner or restore permission.
A real private producer and the coherent manager must supply these authorities.

## Exact current source evidence

The ordinary and ROS `runtime/llvm_jit_cache/format.h` use cache format5 and
reader-owned product metadata (`uwvm2` / `uwvm2ros`), also included in namespaced
storage and context identity. The key has canonical length-delimited fields. Ordinary/ROS cache magic is
`UWVMLJC\x01` / `UWVMROS\x01`; product identity must use both actual formats,
not an ordinary-only test oracle.
ISA metadata records target triple, CPU and features; context metadata contains
product, the complete semantic key, LLVM version, runtime ABI and codegen policy.
The cache reader checks format/size, ISA/context equality and selected signature
policy before decompression. Native objects are copied into owned result bytes.
The deterministic Ed25519 identity seed is explicitly not a secret-key trust
boundary against an attacker running under the same account.

`environment.h::uwvm_runtime_abi_fingerprint()` currently records ABI revision27,
typed/raw call and result ABIs, unwind/EH, GC/storage/layout/memory/FP/target
lowering revisions, runtime/compiler family, product, semantic version, optional
git commit, dirty flag and optional `UWVM2_BUILD_SOURCE_ID`. Default cache policy
rejects dirty/unidentified provenance unless a build selected an unsafe developer
escape hatch. These source identifiers do not prove the exact loaded executable,
compile/link inputs or patched/dynamically loaded LLVM/libunwind/OpenSSL/libc.
Semantic version alone is deliberately insufficient for checkpoint admission.

Full cache emission uses the complete pre-optimization module bitcode hash after
target attributes. Its separate `module-wasm-hash` is explicitly only a namespace
hint: it hashes selected function declarations/bodies and counts, not all actual
original Wasm bytes, data/custom sections or the whole module/dependency closure.
Parallel objects bind a base key, partition count and index. `cache_load_result`
retains object/status/signature/ISA but not an actual accepted blob/context digest
owner suitable for a later checkpoint transaction. The ObjectCache derived key
is reused between lookup and emission for the same live LLVM module.

At the R1 audit cut the immutable recording profile returned ten cache-identity
words but both full policy strings emitted only words0..8 and omitted word9, the
native workspace quota. The separately reviewed full-tuple source receipt
`uwvm-checkpoint-cache-complete-tuple-source-applied-20261003-r1` (manifest258c792c)
now serializes every actual tuple field. This proposal independently canonically
hashes all ten typed words. Adding the missing field changes the complete cache
key and isolates old objects without a cache-format5/runtime-ABI27 bump. The
source fix has not itself qualified a safe cached debug engine. Existing debug-full disables full object lookup AND store because
native objects may embed process-local observer/typed-slot addresses. Numeric
plans and several foreign targets separately disable persistence. A requested
cache directory therefore does not imply a cache-backed debug engine.

The current checkpoint format4 has original module bytes, structural typed
objects, recording IDs and a SHA256 footer. It has no mandatory product/build,
actual accepted cache-entry binding or expected-runtime identity parameter.
Its successful decode is DATA corruption/shape evidence, never a matching VM.

## Mandatory binding envelope and ordering

Introduce envelope4 with identity revision2 enclosing a separately versioned
format4 state body. Old standalone format3 files are explicitly rejected as unbound DATA and
MUST be refused for executable restore (`identity missing`). An unrecognized
envelope, state, identity or cache format fails before VM access or allocation
of an instance. This proposal requires canonical LE identity integers regardless
of native endianness. Native pointer width/target/ABI is separately identified;
being able to read the same file on big endian does not grant cross-target replay.
No automatic schema upgrade may manufacture absent provenance or continuations.

The verified canonical state-body digest is the SHA256 of those exact bounded
encoded bytes, not a hash of the target VM's current state. The target identity
comparison combines this freshly recomputed saved-body digest with actual source,
build, execution policy and cache owners, all before reading/writing mutable VM
state. Identity revision2 recomputes the complete saved replay-log digest and
its explicit event cursor; resident future events are included, so repairing the
outer checksum cannot change later replay inputs. Function closures are
recomputed from dense per-module instance/declaration keys, exact types,
generations and replacement body bytes. Revision1 is rejected. These checks
prove DATA consistency, not actual execution, validated declarations or journal
witnesses. Header/body/footer are committed together by the protected
management database transaction. No checksum grants management authorization.

Hash `UWCPID02` followed by the exact field order in `canonical_digest`: revision,
envelope, state schema, canonical endian ID, product, four diagnostic release
words, six build digests, checkpoint compatibility/continuation revisions, ALL
ten profile words, target/execution ABI, source-link closure, state-body and event
prefix digests, effective cache mode/reason, module/cache counts, then each dense
module and accepted cache object. Fixed-width integers use fast_io `le_put`;
SHA256 uses `fast_io::sha256_context`. No C++ padding, native addresses, lossy
decimal concatenation, file paths, UID, build timestamp or guessed cache filename
enters compatibility identity. Nonzero digest fields are mandatory (zero means
unavailable). Equality also compares typed fields, not just a displayed hash.

## Wasm identity and full closure

For every actual canonical source owner, hash the SAME exclusively owned complete
immutable bytes BEFORE parser construction. Record SHA32 and exact byte length,
including all custom/debug and active/passive data bytes. The parser, validator,
initializer and compiler must consume that same owner. Never hash a filename,
later reopen, mutable mmap range, cheap function-body hint or file metadata and
then use another byte allocation. Hash collisions are assumed infeasible;
length and role remain separately checked rather than relying on SHA alone.

The module vector is dense, complete and ordered by the real instance graph,
including modules with no active frames and unused but linked dependencies. Each
row binds ordinal, main/dependency role, exact runtime role-name byte length and
SHA (rename matters), full original byte digest and exact effective function
body-generation closure. The source-link digest describes actual import/export
resolution, alias multiplicity, function/tag/type identities and native adapter
bindings in stable typed IDs. It must not encode native record addresses. A
single-file parser certificate cannot assert that foreign imports are closed.
Opaque providers without a trusted state/effect adapter remain unavailable.

Function replacement does not mutate the original Wasm digest. Record the exact
ordered module/function-index/current-generation/replacement-body bytes/ABI
closure, including a domain-separated empty replacement set. Retired code needed
by a live frame is a separately identified generation. Missing retained plans or
continuations declines; identical names/signatures do not make old code current.
A saved module set, order, role/name, original byte, length, link graph or effective
body generation mismatch fails. Never choose a similarly named file automatically.

## Exact product/build identity

Product is runtime-owned, never copied from a saved envelope or supplied cache
context. Ordinary and ROS MUST reject each other even if Wasm/version/source IDs
and all user options are identical. Diagnostic version words are included but do
not stand in for build identity. A source change with the same version is distinct.

The actual private build/provider issuer must record: compiled-source manifest
SHA (all shipped runtime/compiler/control/validator sources and generated inputs,
including dirty content); compiler/link invocation manifest SHA (flags, macros,
target/data layout, precise source/header/provider closures); loaded product image
SHA; complete loaded provider closure SHA; exact runtime ABI and codegen ABI SHAs;
and checkpoint compatibility/continuation revisions. Provider closure covers the
actual patched LLVM/libunwind/OpenSSL/libc/platform dependencies and their ABI,
not only upstream version strings. Reproducible manifest generation needs a
specified no-self-reference build-id rule. Reading a path with the executable's
name is not proof it is the image currently loaded. The native loader/launcher
must pin and authenticate the actual loaded image/provider identities. Current
public git/build macros do not implement this issuer. Unknown exact build or
untracked provider → checkpoint capture/restore unavailable; unsafe dirty cache
flags MUST NOT waive checkpoint exact-build binding.

## Actual cache entry and strict policy

Cache OFF is an explicit recorded effective state with reason. OFF envelopes have
no object rows and restore only with the same effective OFF policy. User-disabled,
debug process-bound, target process-bound and unavailable provenance are distinct.
A requested ON configuration whose runtime actually disables lookup/store is not
an ON binding. The manager must report `cache binding unavailable for this debug
engine`; it may require explicit OFF selection, never silently claim cached state.

Cache ON binds one actual accepted full object or the COMPLETE ordered partition
set per module. Every row records actual module/partition ordinal/count, cache
format5, exact stored blob length/SHA, decompressed native object length/SHA,
complete canonical key SHA, ISA and context metadata SHAs, and provider/emitter
identity. The precise accepted context and source/ABI/profile key must be pinned
by the owning engine publication. Writer success or predicted lookup key is not
an accepted entry. Failed asynchronous writes are not valid rows. Test-only object
captures, miss/fallback compilations and incompatible/unsigned/unverified entries
cannot impersonate accepted cached engines. Checkpoint code cannot re-enable the
currently unsafe debug object cache merely to satisfy a file identity rule.

Before snapshot publication and restore, actual cache assets must remain protected
against guest fd aliases, truncate/rename/hardlink exposure and external replacement.
The manager checks pinned accepted bytes and the exact requested persisted entry;
missing/rebuilt/differently compressed/re-signed/overwritten blob or any changed
native object/key/context/provider/codegen identity FAILS. Native code addresses
and relocation results never enter the portable file. Identical caches are still
not proof that runtime addresses are rebound or a resume entry exists.

The initial policy is STRICT: missing cache, ON↔OFF, altered entry, cache-directory
miss, changed partition count/order, foreign build/product, or fallback compilation
rejects without touching VM state. Explicit regeneration is a future separate
trusted operation: revalidate the SAME owned Wasm closure with the fused compiler,
construct qualified new continuation code and exact cache bundle, validate all
state/effect/ABI compatibility, and issue a NEW bound checkpoint identity in a
failure-atomic transaction. It never silently rewrites an old envelope's binding.
The initial implementation has no regeneration authorization or restore consumer.

## Actual producer and admission boundary still to implement

The header's values and comparison status are untrusted DATA. A future private
issuer must tie source SHA/length to actual owned-file lifetime, exact build to
actual loaded code/providers, and cache rows to real engine/load/publication
owners. It must run under real lease→ONE current-ticket complete cohort→host
closure→publication and all resource/root/exposure/FD census. Matching DATA is a
necessary identity check, never sufficient to enter that transaction or restore.

Save must bind state/effects/resources from that same coherent episode. Restore
must compare identity before any VM state read/write, type/graph/resource quotas,
imports, effects journal and actual continuation eligibility; prepare an isolated
new domain and allocation-free commit only after old JIT activations bail out.
No native PC/SP/GPR jump, mapped file after-truncate assumption, caller epoch or
private-producer test mock is allowed. Resource-input diagnostics and format4
roundtrip tests still do not qualify any of these operations.

## Evidence plan and negative cases

Component tests use real owned-file reads and actual cache-format writer output
for two distinct modern Wasm inputs and two distinct object payloads. They verify
wrong Wasm bytes/length/order/role/link/replacement, wrong accepted cache key/blob/
object/metadata/partition, cross-product, same version with different image/build
manifest/provider, schema/identity/cache-format versions, incomplete/zero build,
OFF↔ON and reason mismatch, and complete ten-word profile hashing. These are DATA
comparison/encoding tests, not actual loaded-build or cache-owner certificates.
Their output includes an independent canonical endian digest oracle; execute
both LE native and BE QEMU in the approved remote cgroup, never on this host.

Product tests require real cached code after a qualified relocation-safe debug
cache provider exists: fresh all3 TUs/SDK/provider builds, cache hit provenance,
mismatch before VM memory/global/GC/host effect read or write, no partial mutation
or file publish, and explicit regeneration rejected until implemented. The current
build/cache/world-state/capture/restore capabilities remain unavailable.

## Primary references

WebAssembly's [runtime/store/module-instance model](https://webassembly.github.io/spec/core/exec/runtime.html)
is the basis for binding identities across module-local indices and store aliases.
[LLVM ObjectCache](https://llvm.org/doxygen/classllvm_1_1ObjectCache.html) supplies
separate lookup and compiled-object callbacks; neither the interface nor an expected
key issues an UWVM retained accepted-entry receipt. [NIST FIPS180-4](https://csrc.nist.gov/pubs/fips/180-4/upd1/final)
defines message digests; this design separately requires actual owner/authentication
and never treats a content hash as a management capability. The binding policy
above is an UWVM design proposal, not a WebAssembly standard snapshot format.

The precise revision2 function/event preimages and pending corruption regressions
are specified in `test/0036.checkpoint_binding/README.state-identity.md`.

State schema6 retains the empty table descriptor and revises the canonical state wire
markers (`UWCPST6\0`, `UWCPSTE6`). Binding identity revision2, envelope4,
cache format5 remain unchanged. The actual native compilation profile records
state schema6 in tuple field2 and the genuine native continuation ABI in field3; all old state5/4/3
profile/cache manifests are incompatible. Empty descriptor carrier bits and file
IDs remain DATA, never runtime source ownership or checkpoint authority.


State schema6 additionally binds preserved exception diagnostic origin/name
payloads and the explicit unknown instruction/generation markers. Inner state
magic and commit are `UWCPST6\0` / `UWCPSTE6`; state5/4/3 are never silently
migrated. Envelope4, cache format5 and identity2 remain unchanged. The complete ten-field
actual compilation profile records schema6 and separately the genuine native ABI;
profile policy versions are independently preserved, not reset by this bump.

Controlled schema6 adds original builtin WASIp1 binding-only flag3 resources.
Their exact108-byte code-interface descriptor participates in the effective
function-generation digest. Schema5 does not provide that provenance and is
never silently upgraded. This changes the inner state ST6/STE6 and actual profile
field2 only; identity2, envelope4, cache5, native ABI2 and policy res17/observe12
remain independent. Equal detached descriptors grant no host lifetime, effects,
replay or external restoration rights; actual adapters remain mandatory.
