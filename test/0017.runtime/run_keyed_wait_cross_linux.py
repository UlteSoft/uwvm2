#!/usr/bin/env python3
"""Real cross-Linux wait/pause/close workers; not full VM or JIT parity.

Run behind the owned-process resource supervisor in the required test cgroup.
Cross providers are extracted sysroots, never installed on the host. Each row
compiles the real production headers, checks ELF identity, executes two real
workers and requires physical joins. The old pause-only predicate must fail
with its explicit cleanup diagnostic; a timeout or signal is not a negative PASS.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shlex
import struct
import subprocess
import time

PROFILES = {
    "x86_64": ("x86_64-linux-gnu", "x86_64", 62, 64, "little", []),
    "aarch64": ("aarch64-linux-gnu", "aarch64", 183, 64, "little", []),
    "i686": ("i686-linux-gnu", "i386", 3, 32, "little", ["-msse2"]),
    "riscv64": ("riscv64-linux-gnu", "riscv64", 243, 64, "little", ["-march=rv64gc", "-mabi=lp64d"]),
    "ppc64": ("powerpc64-linux-gnu", "ppc64", 21, 64, "big", ["-mcpu=power8"]),
    "mips64": ("mips64-linux-gnuabi64", "mips64", 8, 64, "big", ["-march=mips64r2", "-mabi=64"]),
    "sparc64": ("sparc64-linux-gnu", "sparc64", 43, 64, "big", []),
    "loongarch64": ("loongarch64-linux-gnu", "loongarch64", 258, 64, "little", ["-march=la464"]),
}


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def elf(path, machine, bits, order, legacy_executable_stack=False):
    data = path.read_bytes()
    assert data[:4] == b"\x7fELF"
    assert data[4] == (2 if bits == 64 else 1) and data[5] == (1 if order == "little" else 2)
    endian = "<" if order == "little" else ">"
    assert struct.unpack_from(endian + "H", data, 18)[0] == machine
    phoff = struct.unpack_from(endian + ("Q" if bits == 64 else "I"), data, 32 if bits == 64 else 28)[0]
    size, count = struct.unpack_from(endian + "HH", data, 54 if bits == 64 else 42)
    assert size == (56 if bits == 64 else 32) and phoff + size * count <= len(data)
    stack = [struct.unpack_from(endian + "I", data, phoff + i * size + (4 if bits == 64 else 24))[0]
             for i in range(count) if struct.unpack_from(endian + "I", data, phoff + i * size)[0] == 0x6474E551]
    assert stack, "missing GNU_STACK"
    if legacy_executable_stack:
        assert all(flags == 7 for flags in stack), "unexpected MIPS toolchain stack contract"
    else:
        assert all(not flags & 1 for flags in stack), "unexpected executable GNU_STACK"
    return dict(machine=machine, bits=bits, byte_order=order, GNU_STACK_flags=stack,
                legacy_executable_stack=legacy_executable_stack, sha256=sha(path))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, required=True, help="Frozen paired production source root")
    ap.add_argument("--fixtures", type=Path, required=True, help="Frozen paired fixture root")
    ap.add_argument("--source-manifest", type=Path, required=True, help="Paired files/identities manifest for --root")
    ap.add_argument("--providers", type=Path, required=True, help="Verified private dependency files manifest")
    ap.add_argument("--deps", type=Path, required=True)
    ap.add_argument("--qemu", type=Path, required=True, help="Directory containing qemu-* executables")
    ap.add_argument("--clang", type=Path, required=True)
    ap.add_argument("--lld", type=Path, required=True)
    ap.add_argument("--profile", choices=PROFILES, required=True)
    ap.add_argument("--out", type=Path, required=True)
    a = ap.parse_args()
    a.out.mkdir(parents=True, exist_ok=False)
    subprocess.run(["bash", str(a.root / "uwvm2/tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    triple, emulator, machine, bits, order, flags = PROFILES[a.profile]
    sysroot = a.deps / "usr" / triple
    gcc = a.deps / "usr/lib/gcc-cross" / triple / "15"
    linker = a.deps / "usr/bin" / (triple + "-ld") if a.profile in ("ppc64", "sparc64") else a.lld
    if a.profile == "x86_64":
        sysroot = Path("/")
        gcc = next(p for p in (Path("/usr/lib/gcc/x86_64-linux-gnu/15"), Path("/usr/lib/gcc/x86_64-linux-gnu/14")) if p.exists())
    qemu = a.qemu / ("qemu-" + emulator)
    assert sysroot.is_dir() and gcc.is_dir() and (a.profile == "x86_64" or qemu.is_file())
    pins = {str(p.resolve()): sha(p) for p in (Path(__file__), a.clang, linker)}
    # LLD may be an independently bundled native tool with its own C++ ABI.
    # This path belongs only to compiler/linker processes, never QEMU guests.
    tool_lib = a.lld.absolute().parent.parent / "lib"
    native_dirs = [p for p in (tool_lib, tool_lib / "x86_64-unknown-linux-gnu") if p.is_dir()]
    for directory in native_dirs:
        for soname in ("libc++.so.1", "libc++abi.so.1", "libunwind.so.1"):
            provider = directory / soname
            if provider.is_file():
                pins[str(provider.resolve())] = sha(provider)
    manifest = json.loads(a.source_manifest.read_text())
    providers = json.loads(a.providers.read_text())
    assert providers["passed"]
    pins.update({str(p.resolve()): sha(p) for p in (a.source_manifest, a.providers)})
    for path, digest in providers["files"].items():
        assert sha(path) == digest, path
    for repo in ("uwvm2", "uwvm2-ros"):
        for leaf in ("cooperative_pause_domain.h", "keyed_wait_set.h"):
            relative = repo + "/src/uwvm2/utils/thread/" + leaf
            path = a.root / relative
            assert sha(path) == manifest["files"][relative], relative
            pins[str(path.resolve())] = manifest["files"][relative]
    if a.profile != "x86_64":
        pins[str(qemu.resolve())] = sha(qemu)
    record = dict(passed=False, scope="real production wait utility workers, not complete target VM/JIT/ASM parity",
                  profile=a.profile, inputs=pins, rows=[], cgroup=Path("/proc/self/cgroup").read_text(),
                  affinity=sorted(os.sched_getaffinity(0)), source_cuts=manifest["identities"],
                  provider_manifest_sha256=sha(a.providers))

    def publish():
        (a.out / "results.json").write_text(json.dumps(record, indent=2) + "\n")

    def run(row, phase, argv, expected=0, env=None):
        log = a.out / row["repository"] / (row["case"] + "-" + phase + ".log")
        start = time.monotonic()
        with log.open("wb") as stream:
            result = subprocess.run(list(map(str, argv)), stdout=stream, stderr=subprocess.STDOUT, timeout=180, env=env)
        row["phases"].append(dict(phase=phase, argv=list(map(str, argv)), returncode=result.returncode,
                                   expected_returncode=expected, seconds=time.monotonic()-start, log_sha256=sha(log)))
        publish()
        assert result.returncode == expected, log.read_text(errors="replace")[-4000:]
        return log.read_text(errors="replace")

    publish()
    try:
        for repo in ("uwvm2", "uwvm2-ros"):
            source = a.root / repo
            out = a.out / repo
            out.mkdir()
            for case, fixture, negative in (
                ("domain-close", "keyed_wait_domain_close", False),
                ("old-predicate", "keyed_wait_domain_close", True),
                ("suspension", "keyed_wait_suspension", False),
                ("prepared", "keyed_wait_prepared_batch", False),
            ):
                cc = a.fixtures / repo / "test/0017.runtime" / (fixture + ".cc")
                pins[str(cc.resolve())] = sha(cc)
                binary = out / (case + ".elf")
                depfile = out / (case + ".d")
                row = dict(repository=repo, case=case, passed=False, phases=[])
                record["rows"].append(row)
                argv = [a.clang, "--target="+triple, "--sysroot="+str(Path("/") if a.profile == "x86_64" else a.deps),
                        "--gcc-install-dir="+str(gcc), "-idirafter", sysroot/"include", "-std=c++26", "-stdlib=libstdc++",
                        "-O2", "-g0", "-fno-rtti", "-pthread", "-DUWVM=2", *flags, "--ld-path="+str(linker),
                        "-MD", "-MF", depfile, "-I"+str(source/"src"), "-I"+str(source/"third-parties/fast_io/include"),
                        "-I"+str(source/"third-parties/bizwen/include"), cc,
                        "-Wl,-z,execstack" if a.profile == "mips64" else "-Wl,-z,noexecstack", "-latomic", "-o", binary]
                if negative:
                    argv.insert(1, "-DUWVM2TEST_OLD_PAUSE_WAKE_PREDICATE")
                if a.profile != "x86_64":
                    argv[1:1] = ["-L"+str(sysroot/"lib"), "-L"+str(a.deps/"usr/lib"/triple)]
                native_env = os.environ.copy()
                link_dirs = list(native_dirs)
                if linker.is_relative_to(a.deps):
                    link_dirs.insert(0, a.deps/"usr/lib/x86_64-linux-gnu")
                if inherited := native_env.get("LD_LIBRARY_PATH"):
                    link_dirs.append(inherited)
                if link_dirs:
                    native_env["LD_LIBRARY_PATH"] = ":".join(map(str, link_dirs))
                row["native_linker_library_path"] = native_env.get("LD_LIBRARY_PATH", "")
                run(row, "compile", argv, env=native_env)
                # Debian MIPS CRT and libc explicitly request an executable
                # stack. Use their unchanged ABI and record that exception;
                # do not rewrite provider notes or claim a non-exec stack PASS.
                row["ELF"] = elf(binary, machine, bits, order, a.profile == "mips64")
                dependencies = shlex.split(depfile.read_text().replace("\\\n", " ").split(":", 1)[1])
                dep_pins = {str(Path(p).resolve()): sha(p) for p in dependencies}
                for path, digest in dep_pins.items():
                    if Path(path).is_relative_to(a.root.resolve()):
                        relative = str(Path(path).relative_to(a.root.resolve()))
                        assert manifest["files"][relative] == digest, relative
                assert all(key not in pins or pins[key] == value for key, value in dep_pins.items())
                pins.update(dep_pins)
                libs = ":".join(map(str, [a.deps/"usr/lib"/triple, sysroot/"lib", sysroot/"lib64", gcc]))
                launch = [binary] if a.profile == "x86_64" else [qemu, "-U", "LD_LIBRARY_PATH", "-E", "LD_LIBRARY_PATH="+libs, "-L", sysroot, binary]
                text = run(row, "execute", launch, expected=1 if negative else 0)
                if negative:
                    assert "FAIL domain close resumed infinite waits prepared=0" in text and "PASS durable domain close" not in text
                elif case == "domain-close":
                    assert "PASS durable domain close: 40 cells, two genuine workers, two pauses" in text
                row["passed"] = True
                publish()
                print(a.profile, repo, case, "PASS", flush=True)
        assert all(sha(path) == digest for path, digest in pins.items())
        assert all(sha(path) == digest for path, digest in providers["files"].items())
        record.update(passed=True, all_inputs_after_unchanged=True)
    except BaseException as error:
        record["error"] = repr(error)
        raise
    finally:
        publish()


if __name__ == "__main__":
    main()
