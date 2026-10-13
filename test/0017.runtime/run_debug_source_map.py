#!/usr/bin/env python3
"""Build/test the bounded Wasm DWARF line map inside the Linux test cgroup.

Pass --wasm-clang when a Wasm-capable Clang is available IN the cgroup. The
native LLVM/JIT-only Clang used by other uwvm2 tests has no Wasm backend.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess


def run(argv: list[str], **kwargs: object) -> None:
    subprocess.run(argv, check=True, **kwargs)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--cxx", default="/toolchain/bin/clang++")
    parser.add_argument("--wasm-clang", type=Path)
    parser.add_argument("--wasm-ld", default="/toolchain/bin/wasm-ld")
    parser.add_argument("--rustc", type=Path)
    parser.add_argument("--fixture-dir", type=Path,
                        help="Prebuilt C/C++ DWARF4/5 Wasm from a separately bounded compiler scope")
    args = parser.parse_args()
    root = args.source_root.resolve()
    test = root / "test/0017.runtime"
    run(["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")])
    args.out.mkdir(parents=True, exist_ok=True)
    binary = args.out / "debug_source_map"
    run([
        args.cxx, "-std=c++23", "-stdlib=libc++", "-fuse-ld=lld",
        "-rtlib=compiler-rt", "-unwindlib=libunwind", "-O1", "-g0",
        "-Wall", "-Wextra", "-Werror", f"-I{root / 'src'}",
        f"-I{root / 'third-parties/fast_io/include'}",
        str(test / "debug_source_map.cc"), "-o", str(binary),
    ])
    run([str(binary)])
    if args.wasm_clang or args.fixture_dir:
        for language, fixture, expected in (
            ("c", "debug_source_c.c", "debug_source_c.c"),
            ("c++", "debug_source_cpp.cc", "debug_source_cpp.cc"),
        ):
            for version in (4, 5):
                stem = f"{language.replace('+', 'p')}-dwarf{version}"
                if args.fixture_dir:
                    wasm_file = args.fixture_dir / f"debug-source-{stem}.wasm"
                else:
                    object_file = args.out / f"{stem}.o"
                    wasm_file = args.out / f"{stem}.wasm"
                    run([
                        str(args.wasm_clang), "--target=wasm32-unknown-unknown",
                        f"-gdwarf-{version}", "-g", "-O0", "-nostdlib", "-c",
                        str(test / "fixtures" / fixture), "-o", str(object_file),
                    ])
                    run([args.wasm_ld, "--no-entry", "--export-all",
                         str(object_file), "-o", str(wasm_file)])
                run([str(binary), str(wasm_file), expected])
    else:
        print("SKIP real C/C++ -g Wasm: pass a Wasm-capable --wasm-clang in this cgroup")
    if args.rustc:
        wasm_file = args.out / "rust-dwarf5.wasm"
        run([
            str(args.rustc), "--target", "wasm32-unknown-unknown",
            "-C", "panic=abort", "-C", "debuginfo=2", "-C", "opt-level=0",
            str(test / "fixtures/debug_source_rust.rs"), "-o", str(wasm_file),
        ])
        run([str(binary), str(wasm_file), "debug_source_rust.rs"])
    else:
        print("SKIP real Rust -g Wasm: pass --rustc with wasm32 target installed in this cgroup")


if __name__ == "__main__":
    main()
