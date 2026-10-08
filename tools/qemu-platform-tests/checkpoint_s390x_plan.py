#!/usr/bin/env python3
"""Prepare exact cold checkpoint commands; never executes a compiler or QEMU."""

import argparse
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--toolchain", type=Path, required=True)
    parser.add_argument("--deps", type=Path, required=True)
    parser.add_argument("--closure", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    toolchain = args.toolchain.resolve(strict=True)
    deps = args.deps.resolve(strict=True)
    closure = args.closure.resolve(strict=True)
    source_manifest = json.loads((source.parent / "manifest.json").read_text())
    if source_manifest["checkpoint_format_version"] != 3:
        raise RuntimeError("this fresh cold plan requires checkpoint schema 3")
    output = args.output.resolve()
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    target = "s390x-linux-gnu"
    sysroot = deps / "usr" / target
    gcc = deps / "usr/lib/gcc-cross" / target / "15"
    clang = toolchain / "bin/clang++"
    qemu = deps / "usr/bin/qemu-s390x"
    for path in (clang, toolchain / "bin/ld.lld", qemu, gcc / "libgcc.a", sysroot / "lib/ld64.so.1"):
        if not path.is_file():
            raise RuntimeError(f"missing exact cross input: {path}")
    env = ["/usr/bin/env", "-u", "LD_PRELOAD", "-u", "LD_AUDIT",
           "-u", "UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT",
           "LD_LIBRARY_PATH=" + ":".join(map(str, (toolchain / "lib",
               toolchain / "lib/x86_64-unknown-linux-gnu", deps / "usr/lib/x86_64-linux-gnu"))),
           "RAYON_NUM_THREADS=1", "UWVM_TEST_CPUSET=0,2,4,6,16-31", "PYTHONDONTWRITEBYTECODE=1"]
    compile_base = [str(clang), "--target=" + target, "--sysroot=" + str(deps),
                    "--gcc-install-dir=" + str(gcc), "-std=c++23", "-O3", "-g0",
                    "-nostdinc++", "-isystem", str(sysroot / "include/c++/15"),
                    "-isystem", str(sysroot / "include/c++/15" / target),
                    "-isystem", str(sysroot / "include/c++/15/backward"),
                    "-isystem", str(sysroot / "include"), "-fno-fast-math"]
    link_base = [str(clang), "--target=" + target, "--sysroot=" + str(deps),
                 "--gcc-install-dir=" + str(gcc), "--ld-path=" + str(toolchain / "bin/ld.lld"),
                 "-L" + str(sysroot / "lib"), "-L" + str(gcc), "-Wl,--trace"]
    commands = [["input-before", ["python3", str(closure), "--plan", str(output / "plan.json"), "before"]]]
    variants = []
    for repo in ("uwvm2-ros", "uwvm2"):
        root = source / repo
        for variant, extra in (("eh", []), ("noeh", ["-fno-exceptions"])):
            label = repo + "-" + variant
            directory = output / label
            directory.mkdir(mode=0o700)
            object_path = directory / "checkpoint.o"
            binary = directory / "checkpoint-codec"
            dump = directory / "canonical.dmp"
            depfile = directory / "checkpoint.d"
            mapfile = directory / "link.map"
            commands.extend([
                [label + "-compile", [*env, *compile_base, *extra,
                    "-I" + str(root / "src"), "-I" + str(root / "third-parties/fast_io/include"),
                    "-MD", "-MF", str(depfile), "-c",
                    str(root / "test/0017.runtime/debug_checkpoint_codec.cc"), "-o", str(object_path)]],
                [label + "-object-bind", ["python3", str(closure), "--plan", str(output / "plan.json"),
                                         "object", "--label", label]],
                [label + "-link", [*env, *link_base, str(object_path),
                                  "-Wl,-Map=" + str(mapfile), "-o", str(binary)]],
                [label + "-binary-bind", ["python3", str(closure), "--plan", str(output / "plan.json"),
                                         "binary", "--label", label]],
                [label + "-qemu", [*env, str(qemu), "-U", "LD_LIBRARY_PATH", "-E",
                    "LD_LIBRARY_PATH=" + str(sysroot / "lib"), "-L", str(sysroot),
                    str(binary), str(dump), "--require-big"]],
                [label + "-golden", ["python3", str(root / "test/0017.runtime/checkpoint_codec_golden.py"),
                                    "--compare", str(dump)]],
                [label + "-output-bind", ["python3", str(closure), "--plan", str(output / "plan.json"),
                                         "output", "--label", label]],
            ])
            variants.append({"label": label, "repository": repo, "variant": variant,
                             "object": str(object_path), "binary": str(binary), "dump": str(dump),
                             "depfile": str(depfile), "link_map": str(mapfile)})
    commands.append(["input-after", ["python3", str(closure), "--plan", str(output / "plan.json"), "after"]])
    command_bytes = (json.dumps(commands, indent=2) + "\n").encode()
    plan = {"schema": "uwvm-checkpoint-s390x-cold-v1", "kind": "source-only",
            "native_tests_executed": False, "target": target, "target_elf_class": 2,
            "target_elf_data": 2, "target_elf_machine": 22,
            "full_vm_continuation_restore_accepted": False,
            "paired_runtime_or_product_qualification": False,
            "target_cxx_provider": "existing GCC15 target libstdc++ (standalone codec only)",
            "source": str(source), "toolchain": str(toolchain), "deps": str(deps),
            "checkpoint_format_version": 3,
            "output": str(output), "closure": str(closure), "variants": variants,
            "commands_sha256": hashlib.sha256(command_bytes).hexdigest(),
            "canonical_model_bytes": 4347,
            "canonical_model_sha256": "731ddd5ee0febc08055ac4f19faf34fd63af7f0ea9690b658622c5dabee9ff01"}
    (output / "commands.json").write_bytes(command_bytes)
    (output / "plan.json").write_text(json.dumps(plan, indent=2, sort_keys=True) + "\n")
    print(json.dumps({"commands": len(commands), "commands_sha256": plan["commands_sha256"],
                      "plan_sha256": hashlib.sha256((output / "plan.json").read_bytes()).hexdigest()}))


if __name__ == "__main__":
    main()
