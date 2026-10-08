#!/usr/bin/env python3
"""Prepare a fixed four-cell Windows regular-file launcher cross-build plan; run nothing."""

import argparse
import hashlib
import json
from pathlib import Path


SDK = Path("/home/macromodel/Documents/uwvm-validation-20260918.PwcF7M/windows-sdk")
TOOLCHAIN = Path("/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm")
SOURCE_MANIFEST_SHA = "3fc0506765f8a2b5f13e5b0efc9983ae659ba2b0453e17a7386a2ef571b6f339"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--closure", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    args = parser.parse_args()
    evidence = args.evidence.resolve()
    source = args.source.resolve(strict=True)
    closure = args.closure.resolve(strict=True)
    if hashlib.sha256((source.parent / "manifest.json").read_bytes()).hexdigest() != SOURCE_MANIFEST_SHA:
        raise RuntimeError("the immutable Windows component source changed")
    for path in (SDK / "x86_64-w64-mingw32/include/__config_site",
                 SDK / "x86_64-w64-mingw32/include/windows.h",
                 SDK / "x86_64-w64-mingw32/lib/libkernel32.a",
                 SDK / "x86_64-w64-mingw32/lib/libntdll.a"):
        if not path.is_file():
            raise RuntimeError("missing exact Windows target provider: " + str(path))
    output = args.output.resolve()
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    env = ["/usr/bin/env", "-u", "LD_PRELOAD", "-u", "LD_AUDIT",
           "-u", "UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT", "PYTHONDONTWRITEBYTECODE=1",
           "LD_LIBRARY_PATH=" + ":".join(map(str, (TOOLCHAIN / "lib",
               TOOLCHAIN / "lib/x86_64-unknown-linux-gnu"))), "RAYON_NUM_THREADS=1"]
    common = [str(TOOLCHAIN / "bin/clang++"), "--target=x86_64-w64-windows-gnu",
              "--sysroot=" + str(SDK), "-nostdinc++", "-isystem", str(SDK / "include/c++/v1"),
              "-isystem", str(SDK / "x86_64-w64-mingw32/include"), "-std=c++26",
              "-stdlib=libc++", "--rtlib=compiler-rt", "--unwindlib=libunwind",
              "-static-libgcc", "-static-libstdc++", "-fasynchronous-unwind-tables", "-DNOMINMAX=1", "-D_WIN32_WINNT=0x0A00", "-O3", "-g"]
    plan_path = output / "plan.json"
    commands = [["input-before", ["python3", str(closure), "--plan", str(plan_path), "before"]]]
    variants = []
    for repo in ("uwvm2-ros", "uwvm2"):
        root = source / repo
        for suffix, flags in (("eh", ["-fexceptions"]), ("noeh", ["-fno-exceptions"])):
            label = repo + "-" + suffix
            directory = output / label
            directory.mkdir(mode=0o700)
            obj, binary = directory / "regular-launcher.obj", directory / "regular-launcher.exe"
            dep, link_map = directory / "regular-launcher.d", directory / "link.map"
            commands.extend([
                [label + "-compile", [*env, *common, *flags, "-I" + str(root / "src"),
                    "-I" + str(root / "third-parties/fast_io/include"), "-MD", "-MF", str(dep),
                    "-c", str(root / "test/0022.qemu_platforms/windows_raii_regular_launcher.cc"), "-o", str(obj)]],
                [label + "-object-bind", ["python3", str(closure), "--plan", str(plan_path), "object", "--label", label]],
                [label + "-link", [*env, str(TOOLCHAIN / "bin/clang++"),
                    "--target=x86_64-w64-windows-gnu", "--sysroot=" + str(SDK), "-stdlib=libc++",
                    "--rtlib=compiler-rt", "--unwindlib=libunwind", "-static-libgcc", "-static-libstdc++",
                    "--ld-path=" + str(TOOLCHAIN / "bin/ld.lld"), "-L" + str(SDK / "x86_64-w64-mingw32/lib"),
                    "-L" + str(TOOLCHAIN / "lib/clang/23/lib/x86_64-w64-windows-gnu"),
                    "-municode", "-Wl,--verbose", "-Wl,-Map=" + str(link_map), str(obj), "-lkernel32", "-lntdll", "-o", str(binary)]],
                [label + "-binary-bind", ["python3", str(closure), "--plan", str(plan_path), "binary", "--label", label]],
                [label + "-imports", [*env, str(TOOLCHAIN / "bin/llvm-readobj"),
                    "--file-headers", "--coff-imports", "--unwind", str(binary)]],
                [label + "-bridge-disassembly", [*env, str(TOOLCHAIN / "bin/llvm-objdump"),
                    "--disassemble-symbols=wmain", str(obj)]],
                [label + "-coff-relocations", [*env, str(TOOLCHAIN / "bin/llvm-readobj"),
                    "--relocations", "--symbols", str(obj)]],
            ])
            variants.append({"label": label, "repository": repo, "variant": suffix,
                             "object": str(obj), "binary": str(binary), "depfile": str(dep),
                             "link_map": str(link_map), "link_log": str(evidence / (label + "-link.log"))})
    commands.append(["input-after", ["python3", str(closure), "--plan", str(plan_path), "after"]])
    command_data = (json.dumps(commands, indent=2) + "\n").encode()
    plan = {"schema": "uwvm-windows-raii-regular-launcher-build-r1-llvm-static-v1", "kind": "source-only",
            "target": "x86_64-w64-windows-gnu", "native_tests_executed": False,
            "guest_execution_accepted": False, "product_three_tu_abi_qualified": False,
            "named_module_qualified": False, "source": str(source), "sdk": str(SDK),
            "toolchain": str(TOOLCHAIN), "output": str(output), "closure": str(closure),
            "evidence": str(evidence),
            "runtime_scope": "Standalone Windows SDK ABI1 archives only; no ROS vendored full-product qualification.",
            "runtime_recipe": ["--rtlib=compiler-rt", "--unwindlib=libunwind", "-static-libgcc", "-static-libstdc++"],
            "linker_provenance": "LLVM23.1.1 MinGW --verbose; COFF Reading lines bind actual absolute CRT/archive inputs. COFF map only contains basenames and is kept as raw evidence.",
            "source_manifest_sha256": SOURCE_MANIFEST_SHA, "variants": variants,
            "commands_sha256": hashlib.sha256(command_data).hexdigest(),
            "assembly_review_required": "This launcher object is separate from the native trap bridge. Its own actual MD, COFF symbols/relocations, imports and wmain disassembly must be retained.",
            "guest_followup_required": "Fresh real x64 Windows guest under a nonbreakaway owned job, checked regular-file output, real suspended child admission/resume/wait/full32bitexit and final empty Job. This launcher alone cannot qualify trap/GPR/native stepping."}
    (output / "commands.json").write_bytes(command_data)
    plan_data = (json.dumps(plan, indent=2, sort_keys=True) + "\n").encode()
    plan_path.write_bytes(plan_data)
    print(json.dumps({"commands": len(commands), "commands_sha256": plan["commands_sha256"],
                      "plan_sha256": hashlib.sha256(plan_data).hexdigest()}))


if __name__ == "__main__":
    main()
