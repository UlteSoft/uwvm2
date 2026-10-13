#!/usr/bin/env python3
"""Compile and execute an explicit integer-carrier root-slot experiment.

The required gc_object.h overlay is an isolated exclusive local-only sweep;
the product collector, safepoints, or compiler root coverage are not qualified.
Cross-target rows mean object generation, not execution on those targets.
Run Linux inside the established 64 GiB cgroup, or macOS under the 4 GiB
process-tree watchdog in test/0017.runtime/macos_rss_limit.py.
"""

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import signal
import struct
import subprocess
import sys


TARGETS = (
    ("aarch64-apple-darwin", 64),
    ("x86_64-unknown-linux-gnu", 64),
    ("x86_64-w64-windows-gnu", 64),
    ("riscv64-unknown-linux-gnu", 64),
    ("powerpc64-unknown-linux-gnu", 64),
    ("i386-unknown-linux-gnu", 32),
)
SWEEP_SHA256 = "78c7c0601cb2e21b85d9ba7024d5e217d9d36f21802522b1a32a6c24af694ecd"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def command(argv, out, name, env=None, allowed_exit=0):
    result = subprocess.run(argv, capture_output=True, check=False, env=env)
    (out / (name + ".stdout.txt")).write_bytes(result.stdout)
    (out / (name + ".stderr.txt")).write_bytes(result.stderr)
    row = {"argv": [str(arg) for arg in argv], "exit": result.returncode,
           "stdout_sha256": digest(out / (name + ".stdout.txt")),
           "stderr_sha256": digest(out / (name + ".stderr.txt"))}
    if result.returncode != allowed_exit:
        raise RuntimeError(f"{name} exited {result.returncode}; see {out / (name + '.stderr.txt')}")
    return row, result.stdout.decode("utf-8", "replace")


def tool(name):
    resolved = shutil.which(name)
    if resolved is None:
        raise RuntimeError(f"tool unavailable: {name}")
    return str(Path(resolved).resolve())


def render(template, triple, bits):
    word = "i" + str(bits)
    carrier = "i" + str(bits * 2)
    alignment = str(bits // 8)
    loads = []
    stores = []
    for index in range(12):
        loads.extend((
            f"  %argument_{index} = getelementptr {carrier}, ptr %arguments, {word} {index}",
            f"  %reference_{index} = load {carrier}, ptr %argument_{index}, align {alignment}",
            f"  %slot_{index} = getelementptr {carrier}, ptr %slots, {word} {index}",
            f"  store {carrier} %reference_{index}, ptr %slot_{index}, align {alignment}",
        ))
        stores.extend((
            f"  %output_{index} = getelementptr {carrier}, ptr %output, {word} {index}",
            f"  store {carrier} %reference_{index}, ptr %output_{index}, align {alignment}",
        ))
    text = template.replace("@MANY_LOADS@", "\n".join(loads)).replace("@MANY_STORES@", "\n".join(stores))
    text = text.replace("@WORD@", word).replace("@CARRIER@", carrier).replace("@ALIGN@", alignment)
    return f'target triple = "{triple}"\n' + text


def source_id(repo, out, name):
    manifest = out / (name + ".source-manifest.json")
    command([sys.executable, str(repo / "tools/ci/wasm3_source_fingerprint.py"),
             str(repo), str(manifest)], out, name + ".fingerprint")
    value = json.loads(manifest.read_text())
    return value.get("source_id", value.get("source_digest"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--overlay", type=Path, required=True,
                        help="include root containing the pinned experimental uwvm2/.../gc_object.h")
    parser.add_argument("--opt", default="opt")
    parser.add_argument("--llc", default="llc")
    parser.add_argument("--readobj", default="llvm-readobj")
    parser.add_argument("--objdump", default="llvm-objdump")
    parser.add_argument("--cxx")
    parser.add_argument("--cxxflag", action="append", default=[])
    parser.add_argument("--native-triple")
    parser.add_argument("--cgroup", type=Path,
                        help="Linux cgroup directory; restrictions and membership are recorded and verified")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    overlay = args.overlay.resolve()
    overlay_header = overlay / "uwvm2/uwvm/runtime/storage/gc_object.h"
    if digest(overlay_header) != SWEEP_SHA256:
        raise RuntimeError("overlay differs from the pinned local-only sweep experiment")
    tools = {name: tool(getattr(args, name)) for name in ("opt", "llc", "readobj", "objdump")}
    if args.cxx:
        tools["cxx"] = tool(args.cxx)
    elif platform.system() == "Darwin":
        tools["cxx"] = subprocess.run(["xcrun", "--find", "clang++"], capture_output=True,
                                       text=True, check=True).stdout.strip()
    else:
        tools["cxx"] = tool("clang++")
    for name, path in tools.items():
        command([path, "--version"], out, name + ".version")
    template_path = Path(__file__).with_name("shadow_root_frame.ll.in")
    runtime_path = Path(__file__).with_name("shadow_root_runtime.cc")
    summary = {"start_utc": utc(),
               "scope": "standalone LLVM IR plus exclusive local-only sweep; no product GC qualification",
               "overlay_sha256": digest(overlay_header), "template_sha256": digest(template_path),
               "runtime_sha256": digest(runtime_path), "runner_sha256": digest(Path(__file__)),
               "source_id_before": source_id(repo, out, "before"), "tools": tools,
               "cross_targets_mean": "O3 object generation only; not target execution",
               "targets": [], "native": {}}
    if platform.system() == "Linux":
        if args.cgroup is None:
            raise RuntimeError("Linux execution requires --cgroup and the caller must enter it before invoking the runner")
        group = args.cgroup.resolve()
        proc = Path("/proc/self/cgroup").read_text()
        membership = proc.strip().split("0::", 1)[-1]
        actual = (Path("/sys/fs/cgroup") / membership.lstrip("/")).resolve()
        if actual != group:
            raise RuntimeError(f"runner is outside the requested cgroup: {actual}")
        limits = {name: (group / name).read_text().strip() for name in
                  ("memory.max", "memory.swap.max", "cpuset.cpus.effective", "memory.events")}
        if limits["memory.max"] != str(64 * 1024 ** 3) or limits["memory.swap.max"] != "0":
            raise RuntimeError("Linux group is not the required 64 GiB, zero-swap sandbox")
        summary["linux_cgroup_before"] = {"path": str(group), "membership": proc, **limits}
    elif platform.system() != "Darwin":
        raise RuntimeError("native execution has only been prepared for Linux and macOS")

    template = template_path.read_text()
    optimized = {}
    objects = {}
    for triple, bits in TARGETS:
        ir = out / (triple + ".ll")
        ir.write_text(render(template, triple, bits))
        opt_ir = out / (triple + ".optimized.ll")
        command([tools["opt"], "-passes=default<O3>", "-S", str(ir), "-o", str(opt_ir)], out, triple + ".opt")
        opt_text = opt_ir.read_text()
        for helper in ("uwvm_probe_root_enter", "uwvm_probe_root_publish", "uwvm_probe_root_leave",
                       "uwvm_probe_collect", "uwvm_probe_allocate_collect"):
            if not re.search(r"(?:call|invoke)\b[^\n]*@" + helper + r"\(", opt_text):
                raise RuntimeError(f"{triple}: O3 removed required call {helper}")
        obj = out / (triple + ".o")
        command([tools["llc"], "-O3", "-filetype=obj", f"-mtriple={triple}",
                 str(opt_ir), "-o", str(obj)], out, triple + ".llc")
        command([tools["readobj"], "--symbols", "--relocations", "--unwind", str(obj)], out, triple + ".object-info")
        command([tools["objdump"], "--disassemble", "--no-show-raw-insn", str(obj)], out, triple + ".disassembly")
        optimized[triple] = opt_ir
        objects[triple] = obj
        summary["targets"].append({"triple": triple, "pointer_bits": bits,
                                   "optimized_ir_sha256": digest(opt_ir), "object_sha256": digest(obj),
                                   "object_info_sha256": digest(out / (triple + ".object-info.stdout.txt")),
                                   "disassembly_sha256": digest(out / (triple + ".disassembly.stdout.txt"))})

    native_triple = args.native_triple or ("aarch64-apple-darwin" if platform.system() == "Darwin" and
                                          platform.machine() == "arm64" else "x86_64-unknown-linux-gnu")
    if native_triple not in objects:
        raise RuntimeError("native triple must match one of the prepared object targets")
    bits = next(bits for triple, bits in TARGETS if triple == native_triple)
    if bits != struct.calcsize("P") * 8:
        raise RuntimeError("native object pointer width differs from the running Python host")
    binary = out / "shadow-root-probe"
    argv = [tools["cxx"], "-std=c++2c", "-O1", "-g1", "-fno-rtti", "-Werror",
            "-Wno-undefined-inline", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            "-pthread", *args.cxxflag]
    if platform.system() == "Darwin" and not any(flag.startswith("-stdlib=") for flag in args.cxxflag):
        argv.append("-stdlib=libc++")
    if platform.system() == "Darwin" and "-isysroot" not in args.cxxflag:
        # Calling the resolved Apple driver directly does not get xcrun's
        # SDKROOT environment. Bind the SDK explicitly and record its path.
        sdk = subprocess.run(["xcrun", "--show-sdk-path"], capture_output=True,
                             text=True, check=True).stdout.strip()
        argv.extend(("-isysroot", sdk))
        summary["native"]["macos_sdk"] = sdk
    for include in (overlay, repo / "src", repo / "third-parties/fast_io/include",
                    repo / "third-parties/bizwen/include", repo / "third-parties/boost_unordered/include"):
        argv.append("-I" + str(include))
    argv.extend((str(runtime_path), str(objects[native_triple]), "-o", str(binary)))
    summary["native"]["build"], _ = command(argv, out, "native.build")
    env = dict(os.environ)
    env["ASAN_OPTIONS"] = "detect_leaks=" + ("1" if platform.system() == "Linux" else "0") + ":halt_on_error=1"
    env["UBSAN_OPTIONS"] = "halt_on_error=1"
    summary["native"]["run"], output = command([str(binary)], out, "native.run", env)
    match = re.search(r"shadow_root_frames PASS; experimental collections=(\d+); reclaimed=(\d+); max_nested_frames=(\d+)", output)
    if not match or int(match.group(1)) == 0 or int(match.group(2)) == 0 or int(match.group(3)) != 2:
        raise RuntimeError("native root/reclamation/cleanup oracle failed")
    summary["native"].update({"triple": native_triple, "binary_sha256": digest(binary),
                              "collections": int(match.group(1)), "reclaimed": int(match.group(2)),
                              "max_nested_frames": int(match.group(3)),
                              "asan_options": env["ASAN_OPTIONS"], "ubsan_options": env["UBSAN_OPTIONS"],
                              "ir_sanitizer_scope": "C++ collector and bridge instrumented; standalone llc object is not ASan-instrumented"})
    # An intentionally omitted twelfth caller root must fail the independent
    # native object-membership oracle. This guards against a no-op collection
    # or accidental rooting of the input C++ array masquerading as coverage.
    publication = f"call void @uwvm_probe_root_publish(ptr %frame, i{bits} 12)"
    negative_text = (out / (native_triple + ".ll")).read_text()
    if negative_text.count(publication) != 1:
        raise RuntimeError("negative-control root publication is not unique")
    negative_text = negative_text.replace(publication, publication.replace(" 12)", " 11)"))
    negative_ir = out / "negative.omitted-root.ll"
    negative_ir.write_text(negative_text)
    negative_opt = out / "negative.omitted-root.optimized.ll"
    command([tools["opt"], "-passes=default<O3>", "-S", str(negative_ir), "-o", str(negative_opt)],
            out, "negative.opt")
    negative_obj = out / "negative.omitted-root.o"
    command([tools["llc"], "-O3", "-filetype=obj", f"-mtriple={native_triple}", str(negative_opt),
             "-o", str(negative_obj)], out, "negative.llc")
    negative_binary = out / "shadow-root-probe-negative"
    negative_argv = argv[:-4] + [str(runtime_path), str(negative_obj), "-o", str(negative_binary)]
    negative_build, _ = command(negative_argv, out, "negative.build")
    negative_env = dict(env)
    negative_env["ASAN_OPTIONS"] += ":handle_abort=0"
    negative_run, _ = command([str(negative_binary)], out, "negative.run", negative_env,
                              allowed_exit=-signal.SIGABRT)
    negative_stderr = (out / "negative.run.stderr.txt").read_text()
    if "shadow_root_missing: key=22\n" not in negative_stderr:
        raise RuntimeError("negative control failed before proving the omitted reference was reclaimed")
    summary["native"]["negative_control"] = {"missing_root_index": 11,
        "oracle": "SIGABRT after exact membership rejection of still-live SSA reference key=22",
        "generated_ir_sha256": digest(negative_ir), "object_sha256": digest(negative_obj),
        "binary_sha256": digest(negative_binary), "build": negative_build, "run": negative_run}
    if platform.system() == "Linux":
        summary["linux_cgroup_after"] = {name: (args.cgroup / name).read_text().strip() for name in
                                          ("memory.max", "memory.swap.max", "memory.peak", "memory.events", "cpuset.cpus.effective")}
    summary["source_id_after"] = source_id(repo, out, "after")
    if summary["source_id_before"] != summary["source_id_after"]:
        raise RuntimeError("product source changed during the isolated experiment")
    summary["end_utc"] = utc()
    summary["status"] = "PASS standalone root-slot experiment; product collector remains missing"
    (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps({"status": summary["status"], "summary": str(out / "summary.json"),
                      "native": summary["native"]["run"], "targets": len(summary["targets"])}, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
