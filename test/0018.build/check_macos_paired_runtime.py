#!/usr/bin/env python3
"""Run real native exception/thread paths against a ROS CMake runtime contract.

Usage: python3 check_macos_paired_runtime.py <bundled-runtimes build> <output>
Run under the local build memory supervisor; this driver is not a memory cap.
"""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import shlex
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runtime_build", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if platform.system() != "Darwin":
        parser.error("This fixture requires native macOS execution")
    build = args.runtime_build.resolve(strict=True)
    output = args.output.resolve()
    if not (build / "complete.txt").is_file():
        raise RuntimeError("Runtime source build has no completion receipt")
    inputs = json.loads((build / "runtime-inputs.json").read_text())
    if (build / "complete.txt").read_text().strip() != inputs["identity"]:
        raise RuntimeError("Runtime identity differs from its completion receipt")
    for name, key in (("libc++.a", "libcxx"), ("libc++abi.a", "libcxxabi"),
                      ("libunwind.a", "libunwind")):
        digest = hashlib.sha256((build / "lib" / name).read_bytes()).hexdigest()
        if digest != inputs["archives"][key].lower():
            raise RuntimeError("Runtime archive differs from its recorded build: " + name)
    site = hashlib.sha256((build / "include/c++/v1/__config_site").read_bytes()).hexdigest()
    if site != inputs["config_site_sha256"].lower():
        raise RuntimeError("Generated libc++ ABI configuration differs from its build")
    cache = {}
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        if line and not line.startswith(("#", "//")) and "=" in line:
            key, value = line.split("=", 1)
            cache[key.split(":", 1)[0]] = value
    reply = build / ".cmake/api/v1/reply"
    index = json.loads(sorted(reply.glob("index-*.json"))[-1].read_text())
    model = json.loads((reply / index["reply"]["codemodel-v2"]["jsonFile"]).read_text())
    config = model["configurations"]
    if len(config) != 1 or config[0]["name"] != "Release":
        raise RuntimeError("Expected the paired Release runtime contract")
    target = next(t for t in config[0]["targets"] if t["name"] == "uwvm_ros_runtimes_contract")
    contract = json.loads((reply / target["jsonFile"]).read_text())
    arguments = []
    archives = set()
    for fragment in contract["link"]["commandFragments"]:
        if fragment["role"] == "flags" and fragment.get("backtrace") is None:
            continue
        for argument in shlex.split(fragment["fragment"]):
            if argument.endswith(".a") and not argument.startswith("-"):
                archive = (build / argument).resolve(strict=True)
                archive.relative_to(build)
                archives.add(archive.name)
                argument = str(archive)
            arguments.append(argument)
    if archives != {"libc++.a", "libc++abi.a", "libunwind.a"}:
        raise RuntimeError("The consumer must use exactly the paired runtime archives")
    compiler = cache["CMAKE_CXX_COMPILER"]
    command = [compiler, "-Qunused-arguments", "-nostdinc++", "-isystem",
               str(build / "include/c++/v1"), "-std=c++17", "-O3",
               "-Wall", "-Wextra", "-Werror", "-pthread"]
    if cache.get("CMAKE_OSX_SYSROOT"):
        command += ["-isysroot", cache["CMAKE_OSX_SYSROOT"]]
    if cache.get("CMAKE_CXX_COMPILER_TARGET"):
        command += ["--target=" + cache["CMAKE_CXX_COMPILER_TARGET"]]
    command += shlex.split(cache.get("CMAKE_EXE_LINKER_FLAGS", ""))
    source = Path(__file__).parent / "fixtures/macos_paired_runtime/probe.cpp"
    command += [str(source), "-o", str(output)] + arguments
    output.parent.mkdir(parents=True, exist_ok=True)
    receipt = {"command": command, "runtime_build": str(build),
               "runtime_inputs": inputs, "passed": False}
    try:
        subprocess.run(command, check=True)
        subprocess.run([str(output)], check=True, timeout=30)
        undefined = subprocess.check_output(["/usr/bin/nm", "-u", str(output)], text=True)
        for symbol in ("__Unwind_RaiseException", "__Unwind_Resume",
                       "___gxx_personality_v0", "___cxa_throw"):
            if symbol in undefined.split():
                raise RuntimeError("Exception path escaped to an external provider: " + symbol)
        libraries = subprocess.check_output(["/usr/bin/otool", "-L", str(output)], text=True)
        if "/usr/lib/libc++" in libraries:
            raise RuntimeError("The static consumer unexpectedly imports system libc++")
        receipt.update(passed=True, undefined_symbols=undefined, linked_libraries=libraries)
    finally:
        output.with_suffix(".json").write_text(json.dumps(receipt, indent=2) + "\n")
    print("PASS: native static initialization, integer catch, thread, exception_ptr rethrow and cleanup")


if __name__ == "__main__":
    main()
