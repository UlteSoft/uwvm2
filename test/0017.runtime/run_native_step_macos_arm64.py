#!/usr/bin/env python3
"""Qualify ARM64 Mach native stepping under a 4 GiB process-tree RSS cap."""

import argparse
import pathlib
import platform
import plistlib
import shutil
import subprocess
import sys
import tempfile


def run_limited(watchdog: pathlib.Path, argv: list[str], *, expected: int = 0) -> None:
    command = [sys.executable, str(watchdog), "--", *argv]
    completed = subprocess.run(command, check=False)
    if completed.returncode != expected:
        raise RuntimeError(f"expected exit {expected}, got {completed.returncode}: {argv}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--skip-enhanced-entitlement", action="store_true")
    args = parser.parse_args()
    if platform.system() != "Darwin" or platform.machine() != "arm64":
        parser.error("this test requires native macOS ARM64")

    root = pathlib.Path(__file__).resolve().parents[2]
    watchdog = root / "test/0017.runtime/macos_rss_limit.py"
    source = root / "test/0017.runtime/native_step_macos_arm64.cpp"
    sdk = subprocess.check_output(["xcrun", "--show-sdk-path"], text=True).strip()
    spec = pathlib.Path(sdk) / "usr/include/mach/mach_exc.defs"
    with tempfile.TemporaryDirectory(prefix="uwvm-macos-native-step-") as temporary:
        output = pathlib.Path(temporary)
        mig_server = output / "mach_exc_server.c"
        mig_header = output / "mach_exc.h"
        mig_object = output / "mach_exc_server.o"
        executable = output / "native_step"
        run_limited(watchdog, [
            "xcrun", "mig", "-DMACH_EXC_SERVER_TASKIDTOKEN_STATE=1",
            "-server", str(mig_server), "-header", str(mig_header),
            "-user", "/dev/null", str(spec),
        ])
        run_limited(watchdog, [
            "xcrun", "clang", "-std=c17", "-O2", "-isysroot", sdk,
            "-c", str(mig_server), "-o", str(mig_object),
        ])
        run_limited(watchdog, [
            "xcrun", "clang++", "-std=c++20", "-O2", "-Wall", "-Wextra",
            "-Werror", "-isysroot", sdk, "-pthread", str(source),
            str(mig_object), "-framework", "Security",
            "-framework", "CoreFoundation", "-o", str(executable),
        ])
        run_limited(watchdog, [str(executable)])
        if not args.skip_enhanced_entitlement:
            restricted = output / "native_step_enhanced"
            shutil.copy2(executable, restricted)
            entitlement = output / "entitlements.plist"
            entitlement.write_bytes(plistlib.dumps({
                "com.apple.security.hardened-process.platform-restrictions": 1,
            }))
            run_limited(watchdog, [
                "codesign", "--force", "--sign", "-",
                "--entitlements", str(entitlement), str(restricted),
            ])
            run_limited(watchdog, [str(restricted)], expected=2)
            print("MACOS_ENHANCED_RESTRICTION_FAIL_CLOSED_PASS", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
