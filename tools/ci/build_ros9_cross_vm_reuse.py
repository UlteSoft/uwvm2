#!/usr/bin/env python3
"""Cross-build either VM against an already qualified ros.9 target LLVM.

Run only inside the Linux test cgroup. This program never configures or builds
LLVM: it audits the existing target archives, then compiles only the selected VM.
The --audit-only path does no compilation and is safe while another build owns
the cgroup. Frozen source and vendor roots are required for reproducible QEMU
evidence. --output-root selects the bounded persistent build area when the
container's /dev/shm is too small.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import shlex
import subprocess
import sys


TARGETS = {
    "aarch64-linux-gnu": ("aarch64", "AArch64", "EM_AARCH64"),
    "riscv64-linux-gnu": ("riscv64", "RISCV", "EM_RISCV"),
    "i686-linux-gnu": ("i386", "X86", "EM_386"),
}
VERSION = "23.1.1-uwvm-ros.9"
CGROUP = {"memory.max": "68719476736", "memory.swap.max": "0",
          "cpuset.cpus.effective": "0,2,4,6,16-31"}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def embedded_source_ids(path):
    found = set()
    tail = b''
    pattern = re.compile(rb'sha256:[0-9a-f]{64}')
    with path.open('rb') as stream:
        while chunk := stream.read(1 << 20):
            data = tail + chunk
            found.update(item.decode() for item in pattern.findall(data))
            tail = data[-70:]
    return found


def run(argv, *, cwd=None, env=None, log=None):
    args = [str(item) for item in argv]
    if log is None:
        subprocess.run(args, cwd=cwd, env=env, check=True)
    else:
        with log.open("wb") as stream:
            subprocess.run(args, cwd=cwd, env=env, stdout=stream,
                           stderr=subprocess.STDOUT, check=True)


def output(argv, *, cwd=None, env=None):
    return subprocess.check_output([str(item) for item in argv], cwd=cwd,
                                   env=env, text=True)


def verify_cgroup():
    for name, expected in CGROUP.items():
        path = Path("/sys/fs/cgroup") / name
        require(path.is_file() and path.read_text().strip() == expected,
                f"incorrect Linux build cgroup {name}: expected {expected}")
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def verify_source_manifest(vendor_root, llvm_source, *, files):
    bundled = vendor_root / "third-parties/llvm"
    source_manifest = llvm_source / "sources.sha256"
    bundled_manifest = bundled / "sources.sha256"
    require(source_manifest.is_file() and bundled_manifest.is_file(),
            "missing frozen ROS or built LLVM source manifest")
    manifest_sha = digest(source_manifest)
    require(digest(bundled_manifest) == manifest_sha,
            "built LLVM source manifest differs from frozen ROS source")
    if files:
        # Run sha256sum in each source root: paths in the manifest are relative.
        # These checks read only inputs and never configure or rebuild LLVM.
        for root in (bundled, llvm_source):
            run(["sha256sum", "--check", "--quiet", "sources.sha256"], cwd=root)
    return manifest_sha


def parse_cmake_cache(path):
    require(path.is_file(), f"missing target LLVM CMakeCache: {path}")
    cache = {}
    for line in path.read_text().splitlines():
        if not line or line.startswith("//") or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        cache[key.split(":", 1)[0]] = value
    return cache


def verify_version(build, llvm_source, target, backend):
    cache = parse_cmake_cache(build / "CMakeCache.txt")
    checks = {"CMAKE_CXX_COMPILER_TARGET": target, "LLVM_HOST_TRIPLE": target,
              "LLVM_VERSION_SUFFIX": "-uwvm-ros.9",
              "CMAKE_BUILD_TYPE": "Release"}
    for key, expected in checks.items():
        require(cache.get(key) == expected,
                f"target LLVM cache {key}={cache.get(key)!r}, expected {expected!r}")
    # A container restart can remove the original /dev/shm source path while
    # leaving the qualified target archives on /work. A restored symlink to
    # the frozen ROS vendor tree is valid only when the build's recorded source
    # path resolves to that exact tree; verify_source_manifest also checks its
    # complete 12,874-file checksum manifest before compilation.
    recorded_source = cache.get("LLVM_SOURCE_DIR")
    require(recorded_source is not None and
            Path(recorded_source).resolve(strict=True) ==
            (llvm_source / "llvm").resolve(strict=True),
            f"target LLVM source path does not resolve to the frozen vendor tree: {recorded_source!r}")
    require(backend in cache.get("LLVM_TARGETS_TO_BUILD", "").split(";"),
            f"target LLVM lacks {backend} backend")
    header = build / "include/llvm/Config/llvm-config.h"
    require(header.is_file(), f"missing generated target LLVM version header: {header}")
    contents = header.read_text()
    for key, expected in (("MAJOR", "23"), ("MINOR", "1"), ("PATCH", "1")):
        require(re.search(rf"^#define LLVM_VERSION_{key} {expected}$", contents, re.M),
                f"target LLVM {key} version mismatch")
    require(f'#define LLVM_VERSION_STRING "{VERSION}"' in contents,
            "target LLVM generated header is not ros.9")
    return digest(header)


def archive_manifest(build, target, manifest_sha, header_sha, rsp_sha):
    parent = build.parent
    aarch64 = target.startswith("aarch64")
    if aarch64:
        qualification = json.loads((parent / "qualification.json").read_text())
        version = qualification.get("version", {})
        require(version == {"host_target": target, "version": VERSION},
                "AArch64 LLVM qualification version/target mismatch")
        require(qualification.get("source_manifest_sha256") == manifest_sha,
                "AArch64 LLVM qualification source manifest mismatch")
        require(qualification.get("link_rsp_sha256") == rsp_sha,
                "AArch64 LLVM qualification link response changed")
        require(qualification.get("archive_member_arch") == "aarch64",
                "AArch64 LLVM qualification architecture mismatch")
        require(qualification.get("contract_sha256") ==
                digest(parent.parent / "uwvm-llvm-ros9/contract/contract.cmake"),
                "AArch64 LLVM build contract changed")
        version_file = json.loads((build / "uwvm-llvm-version.json").read_text())
        require(version_file == version, "AArch64 LLVM generated version mismatch")
        archive_hashes = {}
        for line in (build / "llvm-archives.sha256").read_text().splitlines():
            sha, name = line.split(None, 1)
            path = Path(name.strip()).resolve(strict=True)
            require(path not in archive_hashes, f"duplicate archive in manifest: {path}")
            archive_hashes[path] = sha
        require(len(archive_hashes) == qualification.get("archives_count"),
                "AArch64 LLVM archive count mismatch")
        expected_members = qualification.get("archive_members_checked")
    else:
        package = json.loads((parent / "package-manifest.json").read_text())
        require(package.get("version") == VERSION and
                package.get("host_target") == target,
                "RV64 LLVM package version/target mismatch")
        require(package.get("source_manifest_sha256") == manifest_sha,
                "RV64 LLVM package source manifest mismatch")
        require(package.get("consumer_link_sha256") == rsp_sha,
                "RV64 LLVM package link response changed")
        require(package.get("version_header_sha256") == header_sha,
                "RV64 LLVM generated version header changed")
        archive_hashes = {}
        for item in package["archives"]:
            path = (build / "lib" / item["name"]).resolve(strict=True)
            require(path not in archive_hashes, f"duplicate archive in package: {path}")
            require(path.stat().st_size == item["size"],
                    f"RV64 LLVM archive size changed: {path}")
            archive_hashes[path] = item["sha256"]
        expected_members = None
    return archive_hashes, expected_members


def qualified_archive_manifest(path, build, target, manifest_sha, header_sha, rsp_sha):
    """Read the exact source-bound manifest from a later bundled-LLVM rebuild."""
    data = json.loads(path.read_text())
    require(data.get("passed") is True and data.get("target") == target,
            "explicit LLVM qualification did not pass for this target")
    require(data.get("vendor_manifest_sha256") == manifest_sha,
            "explicit LLVM qualification vendor source changed")
    require(data.get("version_header_sha256") == header_sha and
            data.get("consumer_link_sha256") == rsp_sha,
            "explicit LLVM qualification generated header/link response changed")
    origin = data.get("source_id", "")
    require(re.fullmatch(r"sha256:[0-9a-f]{64}", origin) is not None,
            "explicit LLVM qualification lacks its original product source ID")
    hashes = {}
    members = 0
    for item in data.get("archives", []):
        name = item["name"]
        require(Path(name).name == name and name.endswith(".a"),
                "explicit LLVM qualification archive name escapes target build")
        archive = (build / "lib" / name).resolve(strict=True)
        require(archive.parent == (build / "lib").resolve(strict=True) and
                archive not in hashes, "explicit LLVM qualification archive is external/duplicated")
        require(archive.stat().st_size == item["size"],
                f"explicit LLVM qualification archive size changed: {name}")
        require(re.fullmatch(r"[0-9a-f]{64}", item["sha256"]) is not None,
                f"explicit LLVM qualification archive hash is invalid: {name}")
        hashes[archive] = item["sha256"]
        members += item["members"]
    require(len(hashes) >= 50 and members == data.get("archive_member_count"),
            "explicit LLVM qualification archive/member closure is incomplete")
    return hashes, members, origin


def inspect_elf(readobj, path, arch, machine, env):
    data = output([readobj, "--file-headers", path], env=env)
    arches = re.findall(r"^Arch: (.+)$", data, re.M)
    machines = re.findall(r"^  Machine: (\S+)", data, re.M)
    require(arches and len(arches) == len(machines),
            f"no complete ELF headers in {path}")
    require(all(item == arch for item in arches),
            f"wrong-architecture ELF member in {path}: {set(arches)}")
    require(all(item == machine for item in machines),
            f"wrong-machine ELF member in {path}: {set(machines)}")
    return len(arches)


def verify_link_closure(build, target, backend, arch, machine,
                        archive_hashes, expected_members, ar, readobj, deps, env):
    rsp = build / "consumer-link.rsp"
    tokens = shlex.split(rsp.read_text())
    require(tokens and all(not token.startswith("@") for token in tokens),
            "LLVM consumer link response is empty or nests another response")
    archives = []
    for token in tokens:
        if not token.endswith(".a"):
            continue
        path = Path(token).resolve(strict=True)
        require(path.parent == (build / "lib").resolve(strict=True),
                f"LLVM response links an external archive: {path}")
        require(path not in archives, f"duplicate LLVM archive link: {path}")
        archives.append(path)
    require(set(archives) == set(archive_hashes),
            "LLVM response archive closure differs from qualified manifest")
    require(any(f"libLLVM{backend}CodeGen.a" == path.name for path in archives),
            f"LLVM response lacks {backend} target CodeGen")
    for other in ("AArch64", "RISCV", "X86", "ARM", "PowerPC", "SystemZ", "LoongArch"):
        if other != backend:
            require(not any(f"LLVM{other}" in path.name for path in archives),
                    f"LLVM response includes {other} target archive")
    require(f"--gcc-install-dir={deps}/usr/lib/gcc-cross/{target}/15" in tokens,
            "LLVM response uses a different GCC target runtime")
    require(str(deps / "usr" / target / "include") in tokens,
            "LLVM response uses a different target include directory")
    require(f"-L{deps}/usr/{target}/lib" in tokens,
            "LLVM response uses a different target library directory")
    members = 0
    for path in archives:
        require(digest(path) == archive_hashes[path],
                f"qualified LLVM archive changed: {path}")
        headers = inspect_elf(readobj, path, arch, machine, env)
        listed = len(output([ar, "t", path], env=env).splitlines())
        require(headers == listed, f"not every archive member is a {arch} ELF: {path}")
        members += headers
    if expected_members is not None:
        require(members == expected_members,
                "target LLVM member count differs from qualification")
    return {"rsp_sha256": digest(rsp), "archives": len(archives),
            "members": members, "archive_sha256": {path.name: archive_hashes[path]
                                                 for path in archives}}


def runtime_multiarch(target):
    return "i386-linux-gnu" if target == "i686-linux-gnu" else target


def runtime_search_paths(deps, target):
    return (deps / "usr/lib" / runtime_multiarch(target),
            deps / "usr" / target / "lib")


def verify_runtime_libraries(deps, target, gcc, arch, machine, ar, readobj, env):
    """Check the target dynamic dependency closure and fixed static libatomic."""
    search = runtime_search_paths(deps, target)
    loader = {"aarch64": "ld-linux-aarch64.so.1",
              "riscv64": "ld-linux-riscv64-lp64d.so.1",
              "i386": "ld-linux.so.2"}[arch]
    pending = ["libssl.so.3", "libcrypto.so.3", "libstdc++.so.6", loader]
    checked = {}
    while pending:
        name = pending.pop()
        if name in checked:
            continue
        matches = [directory / name for directory in search if (directory / name).is_file()]
        require(len(matches) == 1, f"missing or ambiguous target runtime {name}: {matches}")
        path = matches[0].resolve(strict=True)
        require(path.is_relative_to(deps), f"target runtime escapes dependency root: {path}")
        require(inspect_elf(readobj, path, arch, machine, env) == 1,
                f"target runtime is not one ELF: {path}")
        checked[name] = digest(path)
        dynamic = output([readobj, "--dynamic-table", path], env=env)
        pending.extend(re.findall(r"NEEDED\s+Shared library: \[(.*?)\]", dynamic))
    atomic = (gcc / "libatomic.a").resolve(strict=True)
    require(atomic.is_relative_to(deps), f"static libatomic escapes dependency root: {atomic}")
    atomic_members = inspect_elf(readobj, atomic, arch, machine, env)
    require(atomic_members == len(output([ar, "t", atomic], env=env).splitlines()),
            f"not every static libatomic member is a {arch} ELF")
    return {"runtime_library_sha256": checked, "atomic_archive_sha256": digest(atomic),
            "atomic_archive_members": atomic_members}


def fingerprint(source_root, manifest):
    helper = source_root / "tools/ci/wasm3_source_fingerprint.py"
    require(helper.is_file(), f"missing frozen source fingerprint helper: {helper}")
    return output([sys.executable, helper, source_root, manifest]).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--expected-source-id", required=True,
                        help="exact frozen product source ID required by this qualification")
    parser.add_argument("--vendor-root", type=Path,
                        help="frozen ROS tree with the pinned LLVM provider; defaults to --source-root")
    parser.add_argument("--product", choices=("ordinary", "ros"), default="ros")
    parser.add_argument("--target", choices=TARGETS, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, default=Path("/dev/shm"),
                        help="directory containing target build outputs")
    parser.add_argument("--llvm-build", type=Path,
                        help="existing target LLVM build; defaults to ros9 /dev/shm path")
    parser.add_argument("--llvm-source", type=Path,
                        default=Path("/dev/shm/uwvm-llvm-ros9/source"))
    parser.add_argument("--llvm-qualification", type=Path,
                        help="exact bundled-LLVM rebuild summary with archive SHA/member counts")
    parser.add_argument("--deps", type=Path, default=Path("/work/deps"))
    parser.add_argument("--clang", type=Path, default=Path("/toolchain/bin/clang++"))
    parser.add_argument("--llvm-bin", type=Path, default=Path("/toolchain/bin"))
    parser.add_argument("--audit-only", action="store_true",
                        help="audit frozen inputs and all linked LLVM members without compiling")
    parser.add_argument("--test-object-capture", action="store_true",
                        help="test-only VM that exports transient native objects for assembly inspection")
    parser.add_argument("--verify-source-files", action="store_true",
                        help="also sha256sum-check all 12,874 LLVM source manifest entries")
    parser.add_argument("--main-optimization", choices=("O0", "O1", "O2", "O3"), default="O1",
                        help="CLI driver optimization; runtime/JIT always use bounded O1")
    args = parser.parse_args()
    require(re.fullmatch(r"sha256:[0-9a-f]{64}", args.expected_source_id) is not None,
            "expected product source ID is invalid")
    verify_cgroup()
    root = args.source_root.resolve(strict=True)
    vendor = (args.vendor_root or args.source_root).resolve(strict=True)
    require((root == vendor) == (args.product == "ros"),
            "ROS product must use its vendor tree; ordinary product requires a separate ROS vendor tree")
    arch, backend, machine = TARGETS[args.target]
    llvm_build_arg = args.llvm_build or Path(f"/dev/shm/uwvm-llvm-{arch}-ros9/build")
    build = llvm_build_arg.resolve(strict=True)
    llvm_source = args.llvm_source.resolve(strict=True)
    deps = args.deps.resolve(strict=True)
    # Clang selects C++ link-driver behavior from argv[0]. Resolving the
    # clang++ symlink to clang-23 silently omits the implicit libstdc++ link.
    clang = args.clang.absolute()
    require(clang.is_file(), f"missing cross C++ driver: {clang}")
    llvm_bin = args.llvm_bin.resolve(strict=True)
    out = args.out.resolve()
    output_root = args.output_root.resolve(strict=True)
    require(out != root and root not in out.parents, "output must be outside frozen source")
    require(out != vendor and vendor not in out.parents, "output must be outside frozen LLVM vendor")
    require(output_root in out.parents, "target build output must be under --output-root")
    require((vendor / "third-parties/llvm/sources.sha256").is_file(),
            "vendor-root must be a frozen uwvm2-ros tree with its pinned LLVM")
    out.mkdir(mode=0o700, parents=True, exist_ok=True)
    require(not (out / "uwvm").exists(), "output VM already exists; use a fresh --out")
    sysroot = deps / "usr" / args.target
    gcc = deps / "usr/lib/gcc-cross" / args.target / "15"
    require((sysroot / "include").is_dir() and gcc.is_dir(),
            "missing target sysroot or GCC 15 runtime")
    env = dict(os.environ)
    library_paths = (deps / "usr/lib/x86_64-linux-gnu", Path("/toolchain/lib"),
                     Path("/toolchain/lib/x86_64-unknown-linux-gnu"))
    env["LD_LIBRARY_PATH"] = ":".join(map(str, library_paths)) + ":" + env.get("LD_LIBRARY_PATH", "")
    builder = Path(__file__).resolve(strict=True)
    builder_sha = digest(builder)
    clang_sha = digest(clang)
    manifest_sha = verify_source_manifest(vendor, llvm_source,
                                          files=args.verify_source_files or not args.audit_only)
    header_sha = verify_version(build, llvm_source, args.target, backend)
    rsp_sha = digest(build / "consumer-link.rsp")
    qualification = args.llvm_qualification.resolve(strict=True) if args.llvm_qualification else None
    qualification_sha = digest(qualification) if qualification is not None else None
    archives_origin_source_id = None
    if qualification is not None:
        archive_hashes, expected_members, archives_origin_source_id = qualified_archive_manifest(
            qualification, build, args.target, manifest_sha, header_sha, rsp_sha)
    else:
        require(args.target != "i686-linux-gnu",
                "i386 requires --llvm-qualification from its real bundled-LLVM rebuild")
        archive_hashes, expected_members = archive_manifest(build, args.target,
                                                              manifest_sha, header_sha, rsp_sha)
    closure = verify_link_closure(build, args.target, backend, arch, machine,
                                  archive_hashes, expected_members,
                                  llvm_bin / "llvm-ar", llvm_bin / "llvm-readobj", deps, env)
    runtime_libraries = verify_runtime_libraries(
        deps, args.target, gcc, arch, machine, llvm_bin / "llvm-ar",
        llvm_bin / "llvm-readobj", env)
    source_id = fingerprint(root, out / "source-before.json")
    require(source_id == args.expected_source_id,
            f"frozen product source differs from the requested source ID: {source_id}")
    host_probe_size = 2048 if args.target == "riscv64-linux-gnu" else 4096
    preflight = {"passed": True, "target": args.target, "product": args.product,
                 "source_root": str(root), "vendor_root": str(vendor),
                 "output_root": str(output_root), "source_id": source_id,
                 "llvm_version": VERSION, "llvm_source_manifest_sha256": manifest_sha,
                 "llvm_header_sha256": header_sha, "llvm_build": str(build),
                 "builder_sha256": builder_sha, "clang_sha256": clang_sha,
                 "llvm_qualification": str(qualification) if qualification is not None else None,
                 "llvm_qualification_sha256": qualification_sha,
                 "llvm_archives_origin_source_id": archives_origin_source_id,
                 "runtime_optimization": "O1", "main_optimization": args.main_optimization,
                 "host_stack_probe_bytes": host_probe_size,
                 **closure, **runtime_libraries}
    (out / "preflight.json").write_text(json.dumps(preflight, indent=2) + "\n")
    print(f"PASS existing {VERSION} {args.target}: {closure['archives']} linked archives, "
          f"{closure['members']} correct-architecture members; source {source_id}", flush=True)
    if args.audit_only:
        return

    # Reproduce the proven cross target flags, with -O1 after the LLVM response
    # so its Release -O3 cannot override the VM's bounded compilation budget.
    flags = [f"--target={args.target}", f"--sysroot={deps}",
             f"--gcc-install-dir={gcc}", "-idirafter", str(sysroot / "include"),
             "-std=c++26", "-stdlib=libstdc++", "-fno-rtti",
             "-fasynchronous-unwind-tables", "-fstack-clash-protection",
             f"-mstack-probe-size={host_probe_size}", "-O1", "-g0",
             "-Wno-undefined-inline", "-DUWVM=2", "-DUWVM_USE_LLVM_JIT",
             "-DUWVM_USE_UWVM_INT", "-DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1",
             "-DUWVM_RUNTIME_LLVM_JIT_CACHE_USE_OPENSSL_ED25519",
             "-DUWVM_VERSION_X=2", "-DUWVM_VERSION_Y=0", "-DUWVM_VERSION_Z=4",
             "-DUWVM_VERSION_S=0", "-I", str(deps / "usr/include" / runtime_multiarch(args.target)),
             "-I", str(deps / "usr/include"), "-I", str(build / "include"),
             "-I", str(vendor / "third-parties/llvm/llvm/include"),
             "-I", str(root / "src"), "-I", str(root / "third-parties/bizwen/include"),
             "-I", str(root / "third-parties/fast_io/include"),
             "-I", str(root / "third-parties/boost_unordered/include"),
             f'-DUWVM2_BUILD_SOURCE_ID=u8"{source_id}"']
    if args.test_object_capture:
        flags.append("-DUWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT")
    if args.product == "ordinary" and args.target == "riscv64-linux-gnu":
        # The audited ros.9 provider includes the RISC-V TailCC repair.
        # Ordinary does not include ROS's pinned_version.h, so select its ABI.
        flags.append("-DUWVM_LLVM_RISCV_TAILCC_FIXED=1")
    runtime = out / "runtime.o"
    main_object = out / "main.o"
    host_object = out / "host_api.o"
    vm = out / "uwvm"
    runtime_command = [clang, *flags, "-c", "src/uwvm2/runtime/lib/uwvm_runtime.default.cpp", "-o", runtime]
    # Keep intermediates in the selected bounded output directory as named
    # target objects. This preserves the C++ driver for the final link and
    # avoids Clang's combined compile-and-link temporary object in /tmp.
    main_command = [clang, *flags, "-" + args.main_optimization, "-c", "src/uwvm2/uwvm/main.default.cpp", "-o", main_object]
    host_command = [clang, *flags, "-c", "src/uwvm2/uwvm/host_api.default.cpp", "-o", host_object]
    # Debian big-endian PowerPC ELFv1 CRT and TOC relocations require its
    # matching GNU linker. The VM and LLVM archive code still come from Clang.
    linker_args = ([f"--ld-path={deps / 'usr/bin' / (args.target + '-ld')}"]
                   if args.target == "powerpc64-linux-gnu" else ["-fuse-ld=lld"])
    link_command = [clang, *flags,
         main_object, host_object, runtime, f"@{build / 'consumer-link.rsp'}",
         *linker_args, "-O1",
         *(flag for directory in runtime_search_paths(deps, args.target) for flag in ("-L", str(directory))),
         "-L", str(gcc), "-lssl", "-lcrypto", "-l:libatomic.a", "-ldl", "-pthread", "-o", vm]
    commands_path = out / "compile-commands.json"
    commands_path.write_text(json.dumps({name: list(map(str, command)) for name, command in
        (("runtime", runtime_command), ("main", main_command), ("host", host_command), ("link", link_command))},
        indent=2) + "\n")
    commands_sha = digest(commands_path)
    for name, command in (("runtime", runtime_command), ("main", main_command),
                          ("host_api", host_command), ("cli", link_command)):
        run(command, cwd=root, env=env, log=out / (name + ".build.log"))
    require(inspect_elf(llvm_bin / "llvm-readobj", vm, arch, machine, env) == 1,
            "cross-built VM is not one target ELF")
    embedded = embedded_source_ids(vm)
    if embedded != {source_id}:
        vm.rename(out / "uwvm-embedded-source-invalid")
        raise RuntimeError(f"cross VM embedded source IDs differ: {sorted(embedded)}")
    after = fingerprint(root, out / "source-after.json")
    changed = []
    if after != source_id:
        changed.append("frozen ROS source")
    if digest(build / "consumer-link.rsp") != rsp_sha:
        changed.append("LLVM link response")
    if digest(llvm_source / "sources.sha256") != manifest_sha:
        changed.append("LLVM source manifest")
    if digest(builder) != builder_sha:
        changed.append("cross-build script")
    if digest(clang) != clang_sha:
        changed.append("cross Clang")
    if qualification is not None and digest(qualification) != qualification_sha:
        changed.append("LLVM archive qualification")
    if digest(commands_path) != commands_sha:
        changed.append("compile command manifest")
    if digest(gcc / "libatomic.a") != runtime_libraries["atomic_archive_sha256"]:
        changed.append("static libatomic")
    for name, expected in runtime_libraries["runtime_library_sha256"].items():
        candidates = [directory / name for directory in runtime_search_paths(deps, args.target)]
        available = [path for path in candidates if path.is_file()]
        if len(available) != 1 or digest(available[0]) != expected:
            changed.append(f"target runtime {name}")
    for path, expected in archive_hashes.items():
        if digest(path) != expected:
            changed.append(f"LLVM archive {path.name}")
    vm_dynamic = output([llvm_bin / "llvm-readobj", "--dynamic-table", vm], env=env)
    vm_needed = sorted(set(re.findall(r"NEEDED\s+Shared library: \[(.*?)\]", vm_dynamic)))
    unexpected_runtime = set(vm_needed) - set(runtime_libraries["runtime_library_sha256"])
    if unexpected_runtime:
        changed.append("unqualified target runtime " + ", ".join(sorted(unexpected_runtime)))
    if changed:
        vm.rename(out / "uwvm-input-changed-invalid")
        raise RuntimeError("inputs changed during cross compilation: " + ", ".join(changed))
    summary = {**preflight, "binary_sha256": digest(vm), "passed": True,
               "binary_embedded_source_ids": sorted(embedded),
               "product": args.product, "vendor_root": str(vendor),
               "riscv_tailcc_fixed": args.product == "ordinary" and args.target == "riscv64-linux-gnu",
               "test_object_capture": args.test_object_capture,
               "compile_commands_sha256": commands_sha,
               "vm_needed": vm_needed}
    (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"PASS target VM {vm}: {summary['binary_sha256']}", flush=True)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError, RuntimeError) as error:
        print(f"build_ros9_cross_vm_reuse: {error}", file=sys.stderr)
        sys.exit(1)
