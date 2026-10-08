#!/usr/bin/env python3
"""Standalone embedded DWARF Stage1 tests; remote controlled Linux only.

No product/runtime values or external DWARF loaders are exercised. This runner
links the separate SDK's DWARF component without changing production closure.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import platform
import shlex
import subprocess
from pathlib import Path


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    for tool in ("cxx", "llvm-config", "llvm-dwarfdump", "wasm-tools", "wasm-clang", "wasm-ld", "rustc"):
        parser.add_argument("--" + tool, type=Path, required=True)
    parser.add_argument("--cxxflag", action="append", default=[])
    parser.add_argument("--ldflag", action="append", default=[])
    args = parser.parse_args()
    if platform.system() != "Linux":
        parser.error("execute only in the keeper's remote Linux cgroup")
    root = args.source_root.resolve()
    subprocess.run(["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    test = root / "test/0017.runtime"
    commands: list[dict[str, object]] = []

    def run(argv: list[str], name: str) -> str:
        completed = subprocess.run(argv, text=True, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, check=False, timeout=180)
        (out / f"{name}.log").write_text(completed.stdout)
        commands.append({"argv": argv, "log": f"{name}.log", "returncode": completed.returncode})
        if completed.returncode != 0:
            raise RuntimeError(f"{name} failed; inspect {out / (name + '.log')}")
        return completed.stdout

    versions = {}
    for name in ("cxx", "llvm_config", "llvm_dwarfdump", "wasm_tools", "wasm_clang", "wasm_ld", "rustc"):
        versions[name] = run([str(getattr(args, name)), "--version"], "version-" + name)
    sdk = shlex.split(run([str(args.llvm_config), "--cxxflags"], "llvm-cxxflags"))
    sdk = [flag for flag in sdk if not flag.startswith("-std=") and flag != "-fno-exceptions"]
    libs = shlex.split(run([str(args.llvm_config), "--libs", "debuginfodwarf"], "llvm-dwarf-libs"))
    link = shlex.split(run([str(args.llvm_config), "--ldflags"], "llvm-ldflags"))
    system = shlex.split(run([str(args.llvm_config), "--system-libs", "debuginfodwarf"], "llvm-system-libs"))
    common = [str(args.cxx), *sdk, "-std=c++23", "-O1", "-g0", "-fexceptions",
              "-Wall", "-Wextra", "-Werror", f"-I{root / 'src'}",
              f"-I{root / 'third-parties/fast_io/include'}", *args.cxxflag]
    types_binary, index_binary = out / "debug_source_dwarf_types", out / "debug_source_dwarf_index"
    run([*common, str(test / "debug_source_dwarf_types.cc"), "-o", str(types_binary), *args.ldflag], "compile-types")
    run([*common, "-DUWVM_USE_LLVM_JIT", str(test / "debug_source_dwarf_index.cc"), "-o", str(index_binary),
         *link, *libs, *system, *args.ldflag], "compile-index")
    run([str(types_binary)], "component-types")
    run([str(index_binary)], "component-index")
    fixtures = []
    for language, filename in (("c", "debug_source_meta_c.c"), ("cpp", "debug_source_meta_cpp.cc"),
                               ("rust", "debug_source_meta_rust.rs")):
        source = test / "fixtures" / filename
        for version in (4, 5):
            for opt in (0, 1):
                stem = f"{language}-dwarf{version}-O{opt}"
                wasm = out / f"{stem}.wasm"
                if language == "rust":
                    run([str(args.rustc), "--edition=2021", "--target", "wasm32-unknown-unknown",
                         "-C", "panic=abort", "-C", "debuginfo=2", "-C", f"dwarf-version={version}",
                         "-C", "split-debuginfo=off", "-C", f"opt-level={opt}", "-C", "codegen-units=1",
                         str(source), "-o", str(wasm)], f"compile-{stem}")
                else:
                    obj = out / f"{stem}.o"
                    run([str(args.wasm_clang), "--target=wasm32-unknown-unknown", "-g", f"-gdwarf-{version}",
                         f"-O{opt}", "-nostdlib", "-c", str(source), "-o", str(obj)], f"compile-{stem}")
                    run([str(args.wasm_ld), "--no-entry", "--export-all", str(obj), "-o", str(wasm)], f"link-{stem}")
                run([str(args.wasm_tools), "validate", str(wasm)], f"wasm-validate-{stem}")
                run([str(args.llvm_dwarfdump), "--verify", str(wasm)], f"dwarf-verify-{stem}")
                oracle = run([str(args.llvm_dwarfdump), "--debug-info", "--debug-loc", "--debug-loclists",
                              "--debug-ranges", "--debug-rnglists", str(wasm)], f"dwarf-oracle-{stem}")
                if "DW_TAG_formal_parameter" not in oracle or "DW_TAG_variable" not in oracle:
                    raise RuntimeError(f"{stem}: official tool did not find parameter/local metadata")
                if opt == 1 and "DW_TAG_inlined_subroutine" not in oracle:
                    raise RuntimeError(f"{stem}: optimized fixture lacks actual inline DIE")
                run([str(index_binary), str(wasm), filename, str(int(opt == 1))], f"metadata-{stem}")
                fixtures.append({"name": stem, "source_sha256": digest(source), "wasm_sha256": digest(wasm),
                                 "inline_required": opt == 1})
    headers = ["source_dwarf_types.h", "source_dwarf_types.cppm", "source_dwarf_index.h", "source_dwarf_index.cppm"]
    summary = {"stage": "embedded-metadata-only", "runtime_values": False, "versions": versions,
               "components": {name: digest(root / "src/uwvm2/uwvm/debugger" / name) for name in headers},
               "types_binary_sha256": digest(types_binary), "index_binary_sha256": digest(index_binary),
               "fixtures": fixtures, "commands": commands}
    (out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"Stage1 metadata PASS: {len(fixtures)} official-tool fixtures; {out / 'summary.json'}")


if __name__ == "__main__":
    main()
