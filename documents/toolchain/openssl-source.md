# Building the source-owned OpenSSL dependency

All three UWVM2 repositories retain the complete official OpenSSL 3.5.9 source
in `third-parties/openssl/upstream`. WSEA and LLVM cache crypto share this one
provider selection. The source license, release authenticity, archive hash and
per-file manifest are retained beside the source.

Provider precedence is explicit:

1. `--openssl-root=/absolute/target/root` uses the existing prebuilt interface.
2. With the default root, `--openssl-provider=bundled` builds the retained source.
3. `--openssl-provider=package` explicitly requests xmake's OpenSSL >=3 package.

The bundled recipe requires a POSIX build host, Python 3.9+, Perl5, make, a target
C compiler, ar and ranlib. The selected C++ compiler is independent of OpenSSL;
no host library or executable named `openssl` is used as a substitute. On an
unsupported build host, select an explicit prebuilt root or package provider.

For example, configure a native Linux build with:

```sh
xmake f -p linux -a x86_64 --enable-wsea=y --openssl-provider=bundled --openssl-build-jobs=2
xmake -j2
```

The bundled rule runs before consumer compilation. It builds only static
`libcrypto.a` and `libssl.a`, disables application/test/module targets, and keeps
the normal cryptographic algorithms enabled. It does not install globally or
change default trust roots. It links the exact private archive paths, preventing
an absent cross archive from resolving a host `-lcrypto` accidentally. `no-asm`
is available only as an explicit target bring-up option, `--openssl-no-asm=y`.

Compiler, target/CPU/ABI flags, sysroot, Configure target, recipe, upstream
manifest and tool identities are recorded in the output receipt. Paths are
isolated by platform, architecture and configuration fingerprint under
`<builddir>/.openssl/`. Use `--openssl-provider-receipt=/absolute/pinned-provider.json` for qualification;
its SHA256 participates in the cache identity and listed runtime files are
rehashed before artifact reuse. Keep toolchain and sysroot inputs immutable for a build
cohort; changing their paths/flags creates a distinct output. The helper checks
the complete source manifest and compiler identities before reusing artifacts.
Build output receipts include all public generated/source headers and archive
hashes. A per-output file lock serializes simultaneous consumers of one provider.

Cross builds must select the real target compiler and real target sysroot using
normal xmake toolchain/`--llvm-target`/`--sysroot` options. The dependency inherits
SDK and ABI flags, including `-mfloat-abi`, endianness, `-mcpu`, and `-march`.
`--openssl-cflags` and `--openssl-ldflags` provide additional explicit target
flags; `--openssl-configure-target` overrides only OpenSSL's Configure profile.
It cannot bypass the independent target object/ABI checks. ARM PCS is explicit
with `--openssl-float-abi=soft|hard`; `auto` expects arm=soft, armhf/armeb=hard. No target executable
is run by dependency configuration or compilation.

The Linux profiles distinguish these ABIs:

| Profile | ELF machine / class / byte order | Required ABI |
| --- | --- | --- |
| x86_64 | 62 / 64 / little | x86-64 SysV |
| i686 | 3 / 32 / little | i386 SysV |
| aarch64 | 183 / 64 / little | AArch64 LP64 |
| arm | 40 / 32 / little | ARM EABI5, soft float PCS |
| armhf | 40 / 32 / little | ARM EABI5, hard float PCS |
| armeb | 40 / 32 / big | ARM EABI5, hard float PCS (explicit BE soft override available) |
| aarch64_be | 183 / 64 / big | AArch64 LP64 big endian |
| ppc64 | 21 / 64 / big | PowerPC64 ELFv1 |
| mips64 | 8 / 64 / big | MIPS n64 |
| loongarch64 | 258 / 64 / little | LoongArch LP64D |

The builder checks an actual compiler object plus predefined PCS/endianness
macros. This proves the dependency compiler's ABI, not runtime qualification.
Cross qualification additionally requires an ELF audit of the final verifier
and VM and actual execution under the matching QEMU/sysroot with signed and
adversarial modules. A missing target libc/C++ runtime is not repaired by using
headers or libraries from a differently ordered/float-ABI sysroot.

The Python helper can also be called directly by bounded SSH/cgroup CI. Pass a
JSON spec with `platform`, `arch`, `cc`, `ar`, `ranlib` (argv arrays), `cflags`,
`ldflags`, `jobs`, optional `configure_target`, and `no_asm`. Run:

```sh
python3 tools/dependencies/build_openssl.py --source-root third-parties/openssl --spec target.json --output /absolute/private/build/openssl/target
```

The caller must perform resource/cgroup admission. The helper does not create
cgroups, change host toolchains, download dependencies, run upstream tests, or
execute target binaries. Keep actual build/execution evidence distinct from
source preparation; source vendoring alone does not claim any tested target.
