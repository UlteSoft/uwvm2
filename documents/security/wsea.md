# Explicit per-module secure execution (embedded WSEA v2)

UWVM enables signature verification only when the user explicitly selects a
module. Ordinary execution remains available. Selecting a main module does not
select its dependencies: a program may contain several independently selected
or unselected WASM modules.

```sh
uwvm --secure help
uwvm --help secure
uwvm --secure status
uwvm --secure module /apps/main.wasm /secure/main.policy --run /apps/main.wasm

# Select both modules; registration happens before any preload callback.
uwvm --wasm-preload-library /apps/library.wasm library \
  --secure named-module /apps/main.wasm /secure/application.policy com.example.main \
  --secure named-module /apps/library.wasm /secure/application.policy com.example.library \
  --run /apps/main.wasm
```

All VM options precede `--run`; subsequent tokens are guest arguments.
Secure selection uses absolute lexical path normalization, including `.` and
`..`, against the current directory. Use the same path for selection and loading;
selection does not discover hard-link aliases or automatically select another
copy of the file. Duplicate selections are rejected. All selections are
registered before loading callbacks, regardless of option order.

`module` requires a policy containing exactly one module rule and binds the
selected path to that rule's identity. For a shared policy, `named-module`
requires the expected signed identity explicitly. A publisher authorized only
for module B cannot replace a selected module A by putting a valid B signature
at A's path. Status output lists selected paths, expected identities and policies.

For each selected module, missing signatures, invalid evidence or policy errors
deny loading with a stable error code. There is no ordinary-loading fallback.
`--secure help` and `--secure status` do not execute a module. Build support is
controlled by `xmake f --enable-wsea=y`; it is enabled by default on supported
POSIX builds. The default bundled provider builds the pinned OpenSSL 3.5.9
source in `third-parties/openssl/upstream` for the selected target. An explicit
`--openssl-root=/path/to/openssl3` selects a prebuilt provider; see
[source provenance and cross builds](../toolchain/openssl-source.md).
Disabling build support leaves
ordinary loading available and makes `--secure module` return an unsupported
verification error. The current protected-storage backend supports POSIX hosts
with OpenSSL 3; other hosts fail closed when secure selection is requested.

## Two independent verification layers

`wasm-cosign` provides `cosign wasm secure policy-init`, `policy-update`, `sign`,
`verify` and `fetch`. It manages keys, administrator configuration and HTTPS
downloads. The VM never fetches certificates, CRLs, signatures or trust roots.
It independently verifies local evidence using OpenSSL before parsing the
selected module. A successful cosign verification is not a VM authorization.

Complete compilation, stripping, debug-section production and any other custom
section writers before signing. The resulting single WASM contains exactly one
final custom section named `uwvm.wsea.v2`; no sidecars are read by the VM or the
secure CLI. Its canonical, bounded CBOR content is:

```text
# Legacy single contribution, retained byte for byte:
[1, bstr(COSE_Sign1), bstr(leaf-and-intermediates-PEM), bstr(complete-CRLs-PEM)]
# Two through eight contributions in the SAME final section:
[2, [[bstr(COSE_Sign1), bstr(chain-PEM), bstr(CRLs-PEM)], ...]]
```

The signature authenticates the exact original WASM prefix before this section,
including the magic/version and every preceding section, custom section, length
encoding and data byte. This avoids a self-referential signature. The WSEA
section's name and length encodings must be minimal; the section must be last.
Duplicate WSEA sections, trailing bytes/sections and extra envelope fields are
rejected. The COSE signature authenticates all manifest/protected-header bytes;
the PKI signatures independently authenticate the included certificates/CRLs.
The private execution receipt additionally binds the complete final image,
including all embedded evidence. The independently provisioned trust root is
never accepted from the module.

Each contribution has at most 64 KiB of COSE, 4 MiB of certificates and 4 MiB
of CRLs, with at most 16 objects per certificate/CRL bundle. The complete
embedded evidence is bounded to 16 MiB and eight signatures. The local module size limit applies to the complete final
file. Certificate public-key grammar is checked before OpenSSL key import, so
unsupported explicit EC parameters and oversized RSA encodings do not trigger
expensive key decoding. Private immutable snapshots retain the existing quota.

`cosign wasm secure sign app.wasm --policy ... --key ... --certificate ...
--crl ... --module-id ... --module-version ... --algorithm ... --not-before ...
--not-after ...` atomically embeds the evidence in the input file. `--output`
creates a separate signed WASM without overwriting an existing destination.
Re-signing an already sealed input requires `--replace-signature`; adding a
contribution requires `--append-signature`. The flags are mutually exclusive. `verify`
reads its embedded evidence. `fetch output.wasm --policy /secure/app.policy
--url https://example.test/app.wasm` downloads the bounded complete module,
verifies TLS and WSEA using separately configured trust, then publishes it
without replacing any existing file. The VM repeats its own offline verification
when that downloaded file is explicitly selected with `--secure`.

## Command-line deployment example

Provision the CA root, publisher certificate chain, CRL and private signing key
through their independent administrator/publisher channels first. This example
uses a publisher certificate with an Ed25519 public key and codeSigning EKU:

```sh
cosign wasm secure policy-init --output /secure/app.policy \
  --trust-root /secure/issuer-root.pem --state-dir /secure/app-state \
  --certificate /secure/publisher-chain.pem --module-id com.example.app \
  --algorithms Ed25519 --capabilities 3

cosign wasm secure sign /release/app.wasm --policy /secure/app.policy \
  --key /secure/publisher-key.pem --certificate /secure/publisher-chain.pem \
  --crl /release/issuer.crl.pem --module-id com.example.app \
  --module-version 1 --algorithm Ed25519 --capabilities 3 \
  --not-before 2026-10-19T00:00:00Z --not-after 2026-10-20T00:00:00Z

cosign wasm secure verify /release/app.wasm --policy /secure/app.policy
# Publish that signed file at the release HTTPS origin, then on the consumer:
cosign wasm secure fetch /apps/app.wasm --policy /secure/app.policy \
  --url https://releases.example.test/app.wasm
uwvm --secure module /apps/app.wasm /secure/app.policy --run /apps/app.wasm
```

Choose actual release validity dates and a certificate/CRL valid for that period.
The consumer receives its protected policy, root and state through administrator
provisioning; it never needs the private publisher key. The downloader's OS
identity must not be able to replace consumer trust configuration. The issuer
CRL is embedded before publication; updating embedded evidence requires creating
another valid sealed file. For RSA or ECDSA, explicitly configure the matching
policy algorithm list and signing algorithm from the table below.

## Multiple signatures and protected quorums

Every present contribution must pass its own allowed algorithm, exact SPKI pin,
certificate path, authenticated fresh CRL and signature checks. Every signature
must authenticate the identical canonical manifest and exact WASM prefix.
Invalid, unknown, expired or revoked contributions fail the complete bundle;
a good signature cannot hide another bad contribution. Reordering valid
contributions is allowed. An identical duplicated COSE contribution is rejected.

A module rule defaults to one required key. Protected `module_signer` lines add
allowed SPKIs and `module_threshold` specifies the required number of distinct
valid public keys, from one through eight. Multiple algorithms or certificates
under the same public key count as one vote. For example:

```text
module=com.example.main|uwvm2|<primary-SPKI-SHA256>|1|0|3|ES256,Ed25519
module_signer=com.example.main|<second-SPKI-SHA256>
module_threshold=com.example.main|2
```

Extension lines follow their module rule. Pins and thresholds cannot be supplied
by downloaded evidence. A threshold cannot exceed the distinct configured pins;
unknown identities, duplicate pins/thresholds and noncanonical counts are refused.
An explicit 1-of-N policy permits a remaining authorized signature when another
signature is removed. A 2-of-N policy refuses a file stripped below two valid
keys. Receipts bind the complete final signature set; any changed evidence needs
a fresh load. Revocation checks and receipt expiry cover every actual signer,
including additional pins, without rejecting an unused retired pin under 1-of-N.

Provision additional keys using repeated `policy-init --additional-certificate`
and `--signature-threshold`. `policy-update` supports the same flags and
`--clear-additional-signers`, requires an increasing policy version and preserves
protected state and permanent revocation markers.

For a two-key release, configure `--algorithms ES256,Ed25519`, a primary
certificate, one `--additional-certificate`, and `--signature-threshold 2`.
Sign the first contribution with `sign --allow-partial`; it reports
`SIGNED_PARTIAL`, and `verify`, `fetch` and selected VM execution refuse it.
Then use `sign --append-signature` with the second private key, certificate and
algorithm, repeating the identical module ID/version/epoch/capabilities/validity
values. Appending authenticates all existing contributions before modifying the
file and refuses changed manifests without overwriting it. Only a completed
quorum reports `VERIFIED`. `--allow-partial` is confined to the signing workflow.
For example, after provisioning the two publisher chains/keys and their CRLs:

```sh
cosign wasm secure policy-init --output /secure/two-key.policy \
  --trust-root /secure/root.pem --state-dir /secure/two-key-state \
  --certificate /secure/p256-chain.pem \
  --additional-certificate /secure/ed25519-chain.pem \
  --module-id com.example.app --algorithms ES256,Ed25519 \
  --signature-threshold 2 --capabilities 3

cosign wasm secure sign /release/app.wasm --policy /secure/two-key.policy \
  --key /secure/p256-key.pem --certificate /secure/p256-chain.pem \
  --crl /release/p256-issuer.crl.pem --algorithm ES256 --allow-partial \
  --module-id com.example.app --module-version 1 --capabilities 3 \
  --not-before 2026-10-19T00:00:00Z --not-after 2026-10-20T00:00:00Z

cosign wasm secure sign /release/app.wasm --policy /secure/two-key.policy \
  --key /secure/ed25519-key.pem --certificate /secure/ed25519-chain.pem \
  --crl /release/ed25519-issuer.crl.pem --algorithm Ed25519 --append-signature \
  --module-id com.example.app --module-version 1 --capabilities 3 \
  --not-before 2026-10-19T00:00:00Z --not-after 2026-10-20T00:00:00Z

cosign wasm secure verify /release/app.wasm --policy /secure/two-key.policy
uwvm --secure module /release/app.wasm /secure/two-key.policy --run /release/app.wasm
```

Use actual validity dates supported by both certificate/CRL bundles. The first
signing step deliberately creates an incomplete file; distributing it cannot
satisfy this policy. The second step completes the distinct-key quorum.

Module hashing runs once per bundle verification. OpenSSL independently checks
each contribution, while the existing process-local cache retains only validated
RSA public-key mathematics. Signatures, trust, expiry, permissions, revocation
and quorum decisions are always checked again.

## Local administrator policy

Use `cosign wasm secure policy-init --help` and `policy-update --help` to create
and update protected policy through the command line. Policy updates require an
increasing version and preserve rollback state. `fetch` never changes policy,
roots, signer pins or state. A downloader must run without write access to these
trusted files and their directories.

The exact UTF-8/ASCII line format requires a final newline and rejects unknown
or duplicate fields. Multiple `module` lines are allowed with unique IDs:

```text
UWVM-WSEA-POLICY-1
policy_version=1
trust_root=/secure/root.pem
state_dir=/secure/private-state
max_crl_age_seconds=86400
max_module_bytes=104857600
module=com.example.main|uwvm2|<64 lowercase SPKI SHA-256 hex digits>|1|0|3
module=com.example.library|uwvm2|<64 lowercase SPKI SHA-256 hex digits>|1|0|0
```

The six module fields are identity, fixed ABI, authorized signer SPKI digest,
minimum version, minimum security epoch, and allowed capability mask. IDs contain
1–255 ASCII letters, digits, periods, underscores or hyphens. Version is positive;
epoch can be zero. At most 1024 module rules are supported. CRL age is positive
and no more than 31536000 seconds; module size is 8 bytes to 1 GiB. A policy can
be shared by separately selected modules.

An existing six-field module rule permits **ES256 only**. A seventh field may
explicitly allow additional signature profiles, for example:

```text
module=com.example.main|uwvm2|<signer-SPKI-SHA256>|1|0|3|ESP256,ES256
module=com.example.rsa|uwvm2|<RSA-signer-SPKI-SHA256>|1|0|3|PS256,PS384,PS512
```

Algorithm names are exact, case-sensitive, comma-separated and unique. Empty,
unknown, duplicate or whitespace-padded names are rejected. The protected COSE
algorithm must be in that module's local list; there is no automatic widening,
key-type inference during verification, or fallback to a different algorithm.
Use `cosign wasm secure policy-init --algorithms ...` or an increasing-version
`policy-update --algorithms ...`, and `sign --algorithm ...` for a release.
Changing an algorithm list also changes the protected policy fingerprint.

Distinct policy files should use distinct state directories. A shared policy
with several module rules uses one shared state directory; its single policy
fingerprint is advanced transactionally for all of those modules.

Policy and root paths must be clean absolute paths. Files and ancestors must
belong to root or the effective user, have no group/other write permission, and
contain no symlinks. A root-owned sticky temporary ancestor is permitted. The
state directory must belong to the execution user with mode 0700; its `state`
file must be owned by that user with mode 0600. `policy-init` bootstraps it;
the VM refuses to silently recreate a missing state file.

The state records the highest observed policy version and its
SHA-256(policy bytes || root PEM bytes), the latest accepted OS UTC second, and
the highest module version and epoch independently. A per-directory lock and
write/fsync/rename/fsync transaction prevent competing processes from losing
updates. A policy cannot change at the same version. A lower version, epoch,
policy or clock is rejected. State advances only after cryptographic and policy
checks; a later WASM validation failure may still consume that version floor.

The VM samples the actual OS clock after acquiring the state lock and reading
the protected state. A request's earlier verification time is a lower bound;
a slower concurrent verification can therefore complete after a later request.
The commit still rejects time below that earlier observation or the protected
floor, and rejects expiry of the manifest, certificate or CRL evidence while
waiting. It never clamps the clock to a stored value or grants a rollback grace
interval. Receipt rechecks use the same locked clock/expiry decision.

Production cosign commands similarly use `CheckCurrentState`: their read-only
state check observes the real clock after reading the protected state and still
rejects rollback relative to the request's earlier observation. The existing
Go `CheckState` API retains explicit caller-supplied time for deterministic
evaluation. State checking supplements the separate signature/PKI verification;
it is not an execution authorization and never writes VM state.

This is an OS-protected local administrator update mechanism, not TUF or a
network-authorized root rotation protocol. The process identity and root can
change local trust, as they can replace the VM binary. Filesystem snapshot
restoration by a privileged attacker requires hardware-backed monotonic state
to prevent rollback; ordinary files do not provide that property.

## PE-inspired trust policy

The design separates publisher/integrity verification from the VM's execution
decision. Microsoft describes that distinction in its
[PE signing model](https://learn.microsoft.com/en-us/windows/win32/secbp/understanding-pe-signatures)
and [App Control signing policy](https://learn.microsoft.com/en-us/windows/security/application-security/application-control/app-control-for-business/deployment/use-code-signing-for-better-control-and-protection).
WSEA applies independent local roots, codeSigning authorization, signer pins,
revocation and module-specific policy at the execution boundary. Its WASM format
is COSE-based; it does not implement Authenticode/CMS interchange or TSA
countersignatures. Manifest, certificate and CRL expiration remain effective at
current trusted host time. All prefix bytes are authenticated, the seal must be
last, and extra envelope fields or trailing content are rejected.

## Embedded signature and certificate contract

The envelope is tagged COSE_Sign1 with canonical definite CBOR. The protected
header is exactly the one-entry map `{1: algorithm_id}`. The envelope is:

```text
protected = canonical_cbor({1: algorithm_id})
18([bstr(protected), {}, bstr(manifest), bstr(signature)])
manifest = ["uwvm-wsea-v2", id, version, epoch, exact_prefix_size,
            bstr(sha256_complete_prefix), not_before, not_after, "uwvm2", capabilities]
Sig_structure = ["Signature1", bstr(protected), h'', bstr(manifest)]
```

All integers and lengths use shortest encoding. Unknown headers, extra fields,
duplicate/indefinite containers, unlisted algorithms and trailing bytes are
rejected. The final complete module digest remains SHA-256 for every profile.
The signature profiles below bind both the algorithm and its key parameters:

| Policy name | COSE ID | Key and signature contract |
| --- | --- | --- |
| ES256 / ESP256 | -7 / -9 | P-256, SHA-256, low-S raw `r32 || s32` |
| ES384 / ESP384 | -35 / -51 | P-384, SHA-384, low-S raw `r48 || s48` |
| ES512 / ESP512 | -36 / -52 | P-521, SHA-512, low-S raw `r66 || s66` |
| Ed25519 | -19 | Pure Ed25519, 64 bytes; no prehash or context |
| PS256 / PS384 / PS512 | -37 / -38 / -39 | RSA-PSS, respective SHA-2 hash, MGF1 with the same hash, salt length exactly 32 / 48 / 64 bytes |
| RS256 / RS384 / RS512 | -257 / -258 / -259 | RSA PKCS#1 v1.5, respective SHA-2 hash |

RSA keys require 3072–8192 bits and exponent 65537; signatures are exactly the
modulus byte length. Restricted RSA-PSS SPKI keys are unsupported. ECDSA scalars
must be positive and below the curve order; S must be at most half that order.
Ed25519 public keys and R encodings must have canonical y coordinates and no
small-order point; S must be below the subgroup order. Curve operations and
signature equations remain in OpenSSL and Go's cryptographic libraries.
Certificate SPKI encoding is also restricted: RSA rsaEncryption requires NULL
parameters, EC requires the named curve OID and an uncompressed point, and
Ed25519 requires absent parameters and exactly 32 public-key bytes. The pin is
SHA-256 over that certificate's complete SPKI DER. The SPKI public-key BIT
STRING must contain a whole number of bytes; partial-byte encodings are rejected.
The fully specified ESP and Ed25519 IDs follow
[RFC 9864](https://www.rfc-editor.org/rfc/rfc9864.html). Legacy ES names remain
available only through explicit local policy (six-field policy means ES256).
Generic EdDSA (-8), Ed448, DSA, SHA-1 and MD5 profiles are not admitted.

Validity uses Unix seconds and `not_before <= now < not_after`; `not_after`
must fit INT64_MAX. Unknown or unavailable profiles fail closed. A receipt also
binds the exact COSE protected header and signature, so replacing them while
keeping the payload unchanged requires a fresh verified load.

The signer must be a non-CA supported signing leaf with explicit digitalSignature key usage
and codeSigning EKU. Its SPKI must be authorized for the signed module identity.
OpenSSL performs strict path, name constraints, certificate-policy constraints, critical-extension, time and CA
validation against only local anchors. CA EKUs must permit codeSigning if
present. Chain keys are restricted to P-256/P-384/P-521, Ed25519, or RSA with
the bounds above. Certificate/CRL signatures admit ECDSA and RSA PKCS#1 v1.5
with SHA-256/384/512, Ed25519, or RSA-PSS with explicit matching SHA-2/MGF1,
digest-sized salt and trailer 1. Algorithm parameters must match the exact
profile; RSA-PSS parameters must be fully consumed with exact round-trip
encoding, explicit matching SHA-2/MGF1 and digest-sized salt. Trailer 1 may be
omitted as the default or explicitly encoded; other values are rejected. Inner and
outer certificate/CRL signature identifiers must agree. PEM objects must
contain one complete DER value with no nested armor or trailing DER data.

Each non-root certificate needs a current, authenticated issuer CRL whose issuer
has cRLSign. Both nextUpdate and the policy maximum age apply. Missing, future,
expired, forged or revoked evidence denies execution. Delta, scoped/indirect
CRLs and unsupported critical extensions are rejected. CRLs must be v2. All applicable supplied
CRLs are examined; a revocation cannot be hidden by placing an older good CRL
first. A serial listed with removeFromCRL still denies execution; this profile
does not automatically release certificate holds. OCSP and historical trusted timestamps are not accepted in this profile;
the signer certificate must be valid at verification time.

The policy follows the useful separation in PE/Authenticode verification:
cryptographic integrity, an independently trusted publisher chain, code-signing
purpose, revocation evidence and the selected trust action must all succeed.
The WASM protocol has its own precise final-section exclusion boundary.
Microsoft documents risks from unsigned PE signature padding and extra content;
WSEA authenticates every byte outside its own final seal and rejects any
appended content or unsigned padding.
Historical timestamp exceptions are not inferred from a signed date or from an
expired certificate. References: [Microsoft trust actions](https://learn.microsoft.com/en-us/windows/win32/api/wintrust/nf-wintrust-winverifytrust)
and [executable signing coverage](https://learn.microsoft.com/en-us/windows/win32/secbp/understanding-pe-signatures).

## Execution and permissions

Verification uses the same private immutable source image later parsed,
compiled and instantiated. Only the loader can attach a private authorization
receipt to a module. Initialization verifies its buffer identity and digest,
rechecks policy, revocation memory, rollback floors and time, and rejects policy
changes since receipt issuance. PKI is verified once per selected load. Its
private receipt is reusable only for that exact buffer/image before the earliest
manifest, chain-certificate, CRL nextUpdate or CRL-age expiry; it cannot suppress
a new load's independent verification. Prefix and complete-image SHA-256 share
one streaming hash state, avoiding a duplicate initial pass. An unchanged
same-second state recheck avoids a redundant durable write. New loads read the current protected policy. Preloads, checkpoint
source restoration and debugger-owned source loads use the same loader. Warm
compiled caches do not substitute for this source authorization; the existing
cache origin, executable and source integrity mechanisms remain necessary.

RSA public-key mathematical validation uses a bounded, mutex-protected,
process-local memo of 64 successful OpenSSL results, indexed by SHA-256 of the
certificate SPKI DER. Only this intrinsic key check is reused. Key size,
exponent, SPKI grammar, the current module signature, certificate path, EKU,
CRLs, local signer/algorithm/capability policy, time and rollback/revocation state
remain checked for every new load. Invalid keys never enter the memo, eviction
requires validation again, and no memo is written to disk. A previous module's
successful signature is never authority for another module using the same key.
This avoids duplicate expensive RSA public-key checks, especially for several
independently selected modules signed by the same key. OpenSSL documents the
underlying [RSA public-key validation](https://docs.openssl.org/3.5/man7/EVP_PKEY-RSA/#rsa-key-validation).

The recorded native small-module run with process startup measured approximately
2 ms additional median startup time for P-256 and Ed25519, 3–4 ms for P-384/P-521,
19 ms for RSA3072, 40–41 ms for RSA4096, and 260–375 ms for a cold RSA8192 key.
These are observations for the recorded host, input, load and tmpfs filesystem,
not latency guarantees. A separate, contended native run of the complete cosign
process measured approximately 51–73 ms median for its offline verification
command, including full CLI startup, across these seventeen profile/key samples.
The expensive first RSA key validation is retained;
applications that prioritize startup can explicitly authorize P-256 or Ed25519.
Separate native size-scaling observations for P-256 and Ed25519 measured about
3 ms additional median startup for 1 MiB, 18–19 ms for 16 MiB, and 71 ms for
64 MiB of authenticated custom-section padding. Secure total startup for the
64 MiB samples was approximately 96–98 ms. These used two warmups, five samples
per mode and warm tmpfs pages under the recorded load; they do not describe
cold durable-storage latency. Both gates refused a changed final prefix byte
after the original large artifact passed cosign verification.
Multi-module tests exercise both thirteen distinct signing profiles and six
separately signed modules sharing an RSA8192 key, including a corrupted final
library that must stop all guest entry after earlier keys were validated.

The signed capability mask must fit the local module rule. Before initialization,
actual imports and import-reset destinations are checked against that mask:

| Bit | Required import authority |
| --- | --- |
| 1 | Basic WASI Preview 1 |
| 2 | WASI `fd_*`, `path_*`, `poll_oneoff`, including standard I/O; also requires bit 1 |
| 4 | WASI `sock_*`; also requires bit 1 |
| 8 | ROS1 host modules |
| 16 | ROS2 host modules |
| 32 | Other native host imports, including unknown WASI functions |

Unknown bits are rejected. For example a WASI program writing stdout needs mask
3. WASM-to-WASM imports retain each module's separate selection and permissions.
An unsigned, unselected dependency can still execute if the user permits it;
select every dependency to protect the entire program. Runtime WASI permissions,
ROS capabilities, resource limits and host authorization continue independently.
Code signing proves origin and integrity, not correctness or sandbox isolation.

Native dynamic-library preload is rejected while any secure selection is active,
because native constructors execute outside a WASM signature boundary. Function
hot replacement and replacement of already compiled source are also rejected
while secure selection is active. Initial debugger-owned source loading still
passes through the signed loader; an old receipt cannot authenticate new bytes.
Ordinary debugger memory inspection does not authenticate or resign a module.

Policy/evidence changes are checked at loading and initialization boundaries.
This implementation does not asynchronously stop already running instances or
monitor trust files. Offline verification cannot learn a new revocation until
fresh evidence is supplied. The host must protect its OS clock; persisted time
detects backwards movement but cannot establish trustworthy UTC by itself.

Authenticated revocation is remembered separately in private
`state_dir/revoked-<release-SPKI-SHA256>` files. Recording and normal authorization
use the same state lock. Once the VM observes a genuine revocation of the signer
or its validated issuing path, that release key remains denied even if an agent
replays an older, still-fresh good CRL. Records survive policy version updates;
this conservative profile requires a new release key and increasing policy
version for recovery and does not automatically clear certificateHold. Cosign
consults these records but never removes or resets them. Storage integrity and
durability are part of the trusted host boundary, as with version state.

Cosign `fetch` publishes only a completely verified bundle and never overwrites
an existing output WASM. It refuses a revoked bundle instead of publishing it. An
administrator must still supply a new embedded evidence bundle or update
local trust when fresh revocation becomes available; `fetch` is not a background revocation delivery
or live-instance termination service.

## Diagnostics and verification

Errors include `SIG_001` missing signature, `SIG_002` encoding, `SIG_003` invalid
signature, `SIG_004` digest/size, `SIG_005` unsupported algorithm/backend,
`CERT_001` root, `CERT_002` path, `CERT_003` validity, `CERT_004` revoked,
`CERT_005` revocation evidence, `CERT_006` publisher authorization, `POL_001`
module/state rollback, `POL_002` policy, `POL_003` capabilities, `POL_004` time,
and `VM_003` missing/stale execution receipt. Errors never turn into an unsigned
retry for a selected module.

The historical detached-v1 `tests/security_wsea/run.py` exercised its production verifier with disposable
OpenSSL PKI and adversarial COSE/evidence/state fixtures. The CLI/loader runner
used production parsing, preload loading and initialization but deliberately
did not execute a guest backend. Run it only in the designated SSH Linux
cgroup; preserve source hashes and process/cgroup evidence with results.

The initial ES256-only 2026-10-18 actual VM qualification uses the ordinary interpreter with WASI,
OpenSSL enabled, `UWVM_USE_THREAD_LOCAL` absent and the LLVM backend disabled.
The corresponding engine options are `--execution-int=uwvm-int`,
`--execution-jit=none` and `--use-thread-local=n`. Exact direct-compiler commands,
frozen input hashes, guest-entry witnesses and resource receipts are in
[`tests/security_wsea/results/20261018`](../../tests/security_wsea/results/20261018/).
That historical run does not qualify the thread-local profile, C++ language-module
builds or LLVM checkpoint/cache execution. Their signature gates were reviewed as source.
Those historical detached-v1 receipts do not qualify embedded-v2 builds. New
results are recorded separately under `tests/security_wsea/results/20261018-embedded-v2`.
The embedded matrix checks every algorithm through actual guest output and
forged-signature rejection. HTTPS simulation checks deployment refusal, direct
VM refusal when the upper gate is bypassed, and tampering after Go acceptance.
Startup timing measures ordinary and selected-module execution separately;
measurements are specific to their recorded host/profile/input and include
process startup. Cross-platform qualification records each repository and its
actual QEMU/ELF/ABI/OpenSSL identity, rather than reusing a different product
under another target label. ROS qualification is limited to x86 and ARM series,
including their tested big-endian ARM profiles.

The separate [clock-commit and thread-local qualification](../../tests/security_wsea/results/20261019-multisignature/SUMMARY.md)
records the concurrent-request clock regression and updated default TLS builds.
`concurrent_verifier.cpp` tests eight callers against all thirteen profiles and
RSA4096/8192 boundaries, genuine signatures and forged copies, plus explicit
expiry and rollback controls. Its native sanitizer and actual x86_64/aarch64
QEMU evidence is distinct from guest execution. The all-target TLS controller
rebuilds each repository with `UWVM_USE_THREAD_LOCAL`, using the existing
per-target compiler/OpenSSL/runtime receipts. It preserves previous attempts;
only complete matching reports can contribute to a qualification. These tests
do not qualify LLVM/JIT, C++ language-module builds or parallel guest execution.
The default TLS rebuild also exposed an unconditional call to an x87/68881-only
square-root helper on native IEEE targets. The helper now selects the existing
extended rounding path only when needed and otherwise uses native `std::sqrt`;
`native_sqrt.cpp` checks the production helper directly on every QEMU target.

References: [COSE RFC 9052](https://www.rfc-editor.org/rfc/rfc9052.html),
[algorithms RFC 9053](https://www.rfc-editor.org/rfc/rfc9053.html), and
[OpenSSL certificate validation](https://docs.openssl.org/3.0/man3/X509_verify_cert/)
and [verification flags](https://docs.openssl.org/3.0/man3/X509_VERIFY_PARAM_set_flags/).
