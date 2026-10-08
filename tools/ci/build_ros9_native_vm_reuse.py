#!/usr/bin/env python3
"""Build a source-matched native VM using qualified, prebuilt LLVM ros.9.

This intentionally has no CMake or Ninja invocation. Run it only in the
64-GiB Linux test cgroup, one VM build at a time.
"""

import argparse
import hashlib
import json
from pathlib import Path
import resource
import shlex
import subprocess


CGROUP = {"memory.max": "68719476736", "memory.swap.max": "0",
          "cpuset.cpus.effective": "0,2,4,6,16-31"}
X86_DISASSEMBLER_SHA256 = "853fbdab243af9ac47699b4a38719beedb0b7b8cddda6e081ff88095243cd0ce"


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def run(args, *, cwd=None, log=None):
    argv = [str(x) for x in args]
    if log is None:
        subprocess.run(argv, cwd=cwd, check=True)
    else:
        with log.open("wb") as output:
            subprocess.run(argv, cwd=cwd, stdout=output,
                           stderr=subprocess.STDOUT, check=True)


def source_id(root, manifest):
    output = subprocess.check_output(
        ["python3", str(root / "tools/ci/wasm3_source_fingerprint.py"),
         str(root), str(manifest)], text=True)
    return output.strip()


def audit(root, llvm_build):
    for name, expected in CGROUP.items():
        require((Path("/sys/fs/cgroup") / name).read_text().strip() == expected,
                f"incorrect cgroup {name}")
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

    package = json.loads((llvm_build.parent / "package-manifest.json").read_text())
    require(package.get("version") == "23.1.1-uwvm-ros.9",
            "prebuilt LLVM version is not ros.9")
    source_manifest = root / "third-parties/llvm/sources.sha256"
    require(digest(source_manifest) == package.get("source_manifest_sha256"),
            "VM and LLVM source manifests differ")
    version_header = llvm_build / "include/llvm/Config/llvm-config.h"
    require(digest(version_header) == package.get("generated_version_header_sha256"),
            "generated LLVM version header changed")
    rsp = llvm_build / "consumer-link.rsp"
    require(digest(rsp) == package.get("consumer_link_rsp_sha256"),
            "LLVM consumer response changed")
    archives = {Path(item["path"]).resolve(): item["sha256"]
                for item in package["archives"]}
    require(len(archives) == len(package["archives"]),
            "LLVM manifest has duplicate archives")
    linked = [Path(token).resolve() for token in shlex.split(rsp.read_text())
              if token.endswith(".a")]
    require(len(linked) == len(set(linked)) and set(linked) == set(archives),
            "LLVM response/archive closure mismatch")
    require(all(path.parent == (llvm_build / "lib").resolve() for path in linked),
            "LLVM response uses external archives")
    for path, expected in archives.items():
        require(path.is_file() and digest(path) == expected,
                f"qualified LLVM archive changed: {path}")
    # The ros.9 consumer contract predates the built-in native debugger. Its
    # qualified 62-library closure omits the already-built X86 disassembler.
    # Pin this exact same-build archive and prove its machine/symbol before
    # adding it to the VM link; never reconfigure or rebuild bundled LLVM.
    disassembler = llvm_build / "lib/libLLVMX86Disassembler.a"
    require(disassembler not in linked and digest(disassembler) == X86_DISASSEMBLER_SHA256,
            "native debugger disassembler archive differs from qualified ros.9 build")
    headers = subprocess.check_output(
        ["/toolchain/bin/llvm-readobj", "--file-headers", str(disassembler)], text=True)
    members = subprocess.check_output(
        ["/toolchain/bin/llvm-ar", "t", str(disassembler)], text=True).splitlines()
    require(headers.count("Arch: x86_64") == len(members) and
            headers.count("Machine: EM_X86_64") == len(members),
            "native debugger archive has a non-x86_64 member")
    symbols = subprocess.check_output(
        ["/toolchain/bin/llvm-nm", "--defined-only", str(disassembler)], text=True)
    require(" LLVMInitializeX86Disassembler\n" in symbols,
            "native debugger entry point missing from archive")
    return rsp, disassembler, len(linked) + 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--llvm-build", required=True, type=Path)
    parser.add_argument("--deps-root", default="/work/deps/usr", type=Path)
    parser.add_argument("--compiler", default="/toolchain/bin/clang++", type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--audit-only", action="store_true")
    args = parser.parse_args()
    root = args.source_root.resolve(strict=True)
    llvm_build = args.llvm_build.resolve(strict=True)
    out = args.out.resolve()  # Output need not exist yet.
    rsp, disassembler, archive_count = audit(root, llvm_build)
    if args.audit_only:
        print(json.dumps({"qualified_archives": archive_count,
                          "consumer_response_sha256": digest(rsp)}))
        return

    out.mkdir(parents=True, exist_ok=True)
    before = source_id(root, out / "source-manifest-before.json")
    flags = ["-std=c++26", "-stdlib=libc++", "-fno-rtti",
             "-fasynchronous-unwind-tables", "-O1", "-g0",
             "-Wno-undefined-inline", "-DUWVM=2", "-DUWVM_USE_LLVM_JIT",
             "-DUWVM_DISABLE_INT", "-DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1",
             "-DUWVM_RUNTIME_LLVM_JIT_CACHE_USE_OPENSSL_ED25519",
             "-DUWVM_VERSION_X=2", "-DUWVM_VERSION_Y=0",
             "-DUWVM_VERSION_Z=4", "-DUWVM_VERSION_S=0",
             f'-DUWVM2_BUILD_SOURCE_ID=u8"{before}"',
             "-I", str(args.deps_root / "include"),
             "-I", str(args.deps_root / "include/x86_64-linux-gnu"),
             "-I", str(llvm_build / "include"),
             "-I", str(root / "third-parties/llvm/llvm/include"),
             "-I", str(root / "src"),
             "-I", str(root / "third-parties/bizwen/include"),
             "-I", str(root / "third-parties/fast_io/include"),
             "-I", str(root / "third-parties/boost_unordered/include")]
    compiler = str(args.compiler)
    run([compiler, *flags, "-c", root / "src/uwvm2/runtime/lib/uwvm_runtime.default.cpp",
         "-o", out / "runtime.o"], log=out / "runtime.build.log")
    run([compiler, *flags, "-fuse-ld=lld", "-rtlib=compiler-rt",
         "-unwindlib=libunwind", root / "src/uwvm2/uwvm/main.default.cpp",
         root / "src/uwvm2/uwvm/host_api.default.cpp", out / "runtime.o",
         disassembler, f"@{rsp}", "-L", str(args.deps_root / "lib/x86_64-linux-gnu"),
         "-lssl", "-lcrypto", "-ldl", "-pthread", "-o", out / "uwvm"],
        log=out / "cli.build.log")
    after = source_id(root, out / "source-manifest-after.json")
    if before != after:
        (out / "uwvm").rename(out / "uwvm-source-changed-invalid")
        raise RuntimeError("source changed during VM build")
    (out / "summary.json").write_text(json.dumps({
        "source_id": before, "llvm_version": "23.1.1-uwvm-ros.9",
        "consumer_response_sha256": digest(rsp),
        "native_disassembler_sha256": digest(disassembler),
        "qualified_archives": archive_count,
        "vm_sha256": digest(out / "uwvm"),
    }, indent=2) + "\n")
    print((out / "summary.json").read_text(), end="")


if __name__ == "__main__":
    main()
