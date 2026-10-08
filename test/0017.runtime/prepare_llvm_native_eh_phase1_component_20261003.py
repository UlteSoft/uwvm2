#!/usr/bin/env python3
"""Emit keeper command arrays; never execute, build, patch, or admit a process."""
import argparse
import json
from pathlib import Path


def plan(source_root: str, build_root: str, clang: str, clangxx: str,
         cmake: str, ninja: str, readelf: str, nm: str, objdump: str) -> dict:
    source = Path(source_root)
    build = Path(build_root)
    if not source.is_absolute() or not build.is_absolute():
        raise ValueError("keeper source/build roots must be absolute")
    upstream = source / "llvm-runtimes" / "upstream"
    leaf = source / "component" / "test" / "0017.runtime"
    component = leaf / "llvm_native_eh_phase1_component_20261003"
    include = str(upstream / "libunwind" / "include")
    steps = [{
        "kind": "root_review_then_staging_only_patch",
        "source_review_approved": False,
        "cwd": str(upstream),
        "check_argv": ["git", "apply", "--check", str(leaf / "llvm_native_eh_phase1_observer_provider_20261003.patch")],
        "apply_argv": ["git", "apply", str(leaf / "llvm_native_eh_phase1_observer_provider_20261003.patch")],
        "requires": "Root must review exact patch; never apply to either live vendor tree.",
    }]
    for enabled in (False, True):
        name = "on" if enabled else "off"
        runtime_build = build / f"runtimes-{name}"
        component_build = build / f"component-{name}"
        flags = "-DUWVM_EXPERIMENTAL_NATIVE_EH_PHASE1_OBSERVER=1" if enabled else ""
        configure = [cmake, "-S", str(upstream / "runtimes"), "-B", str(runtime_build),
                     "-G", "Ninja", "-C", str(source / "llvm-runtimes" / "paired-static.cmake"),
                     f"-DCMAKE_C_COMPILER={clang}", f"-DCMAKE_CXX_COMPILER={clangxx}",
                     f"-DCMAKE_MAKE_PROGRAM={ninja}", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                     f"-DCMAKE_C_FLAGS={flags}", f"-DCMAKE_CXX_FLAGS={flags}",
                     f"-DLIBCXXABI_LIBUNWIND_INCLUDES={include}"]
        steps.extend([
            {"kind": "file_api_query", "variant": name,
             "required_empty_query_files": [str(runtime_build / ".cmake/api/v1/query/codemodel-v2"),
                                             str(runtime_build / ".cmake/api/v1/query/cache-v2")],
             "admission": "all mutations inside keeper cgroup"},
            {"kind": "runtime_configure", "variant": name, "argv": configure,
             "admission": "64GiB swap0 approved E cores"},
            {"kind": "runtime_build", "variant": name,
             "argv": [cmake, "--build", str(runtime_build), "--parallel", "16", "--target",
                      "cxx_static", "cxxabi_static", "unwind_static", "cxx-headers"],
             "admission": "64GiB swap0 approved E cores"},
            {"kind": "resolve_actual_paired_targets_and_options", "variant": name,
             "file_api_root": str(runtime_build / ".cmake/api/v1/reply"),
             "required_bindings": [f"EH_{name.upper()}_CXX_INCLUDE", f"EH_{name.upper()}_CXX_ARCHIVE",
                                   f"EH_{name.upper()}_CXXABI_ARCHIVE", f"EH_{name.upper()}_UNWIND_ARCHIVE"],
             "requirements": ["exact paired macro in actual C/C++ compile_commands",
                              "actual generated __config_site and cxx-headers bytes",
                              "exceptions RTTI threads ON and matching preset options",
                              "target libc/sysroot and compiler-rt actual driver closure",
                              "no bootstrap system C++ headers in final component"]},
        ])
        # Bindings are explicit keeper resolutions, never guessed lib paths.
        component_configure = [cmake, "-S", str(component), "-B", str(component_build), "-G", "Ninja",
                               f"-DCMAKE_C_COMPILER={clang}", f"-DCMAKE_CXX_COMPILER={clangxx}",
                               f"-DCMAKE_MAKE_PROGRAM={ninja}", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                               f"-DEH_OBSERVER_ON={'ON' if enabled else 'OFF'}",
                               f"-DEH_UNWIND_INCLUDE={include}",
                               f"-DEH_FAST_IO_INCLUDE={source / 'fast_io' / 'include'}"]
        for key in ("CXX_INCLUDE", "CXX_ARCHIVE", "CXXABI_ARCHIVE", "UNWIND_ARCHIVE"):
            component_configure.append(f"-DEH_{key}=${{EH_{name.upper()}_{key}}}")
        binary = str(component_build / "uwvm-native-eh-phase1-component")
        steps.extend([
            {"kind": "component_configure", "variant": name, "argv": component_configure,
             "requires_resolved_bindings": True, "admission": "64GiB swap0 approved E cores"},
            {"kind": "component_build", "variant": name,
             "argv": [cmake, "--build", str(component_build), "--parallel", "2"],
             "admission": "64GiB swap0 approved E cores"},
            {"kind": "inspect_dynamic_dependencies", "variant": name, "argv": [readelf, "-d", binary],
             "reject_dependency_names": ["libc++.so", "libc++abi.so", "libunwind.so", "libstdc++.so", "libgcc_s.so"]},
            {"kind": "inspect_actual_provider_symbols", "variant": name, "argv": [nm, "-n", binary],
             "required_paired_symbols": ["__cxa_throw", "__cxa_rethrow", "__gxx_personality_v0", "_Unwind_RaiseException", "_Unwind_Backtrace"],
             "observer_symbols_required": enabled,
             "observer_symbol_names": ["uwvm_eh_observer_arm_v1", "uwvm_eh_observer_bind_primary_v1", "uwvm_eh_observer_disarm_v1"]},
            {"kind": "inspect_cfi", "variant": name, "argv": [readelf, "-wf", binary],
             "require_actual_plain_chain_fde_without_personality_lsda": True},
            {"kind": "inspect_plain_chain_code", "variant": name,
             "argv": [objdump, "-dr", "--disassemble-symbols=uwvm_eh_observer_plain_c_chain_v1", binary],
             "require_genuine_recursive_calls_and_no_tail_elision": True},
            {"kind": "execute_component", "variant": name, "argv": [binary],
             "admission": "64GiB swap0 approved P core; bind actual PID/TIDs source archives ELF and loader",
             "expected": {"observer_enabled": enabled, "cases": 150, "workers": 4, "rounds": 32,
                          "first_search_finishes": 136 if enabled else 0,
                          "first_search_prefix_only": True, "complete_original_trace_observed": False,
                          "diagnostic_replacement": False, "vm_qualified": False, "performance_qualified": False}},
        ])
    return {
        "schema": "uwvm-native-eh-phase1-component-keeper-plan-v1",
        "source_review_approved": False, "provider_patch_applied": False,
        "execute_ready": False, "native_compiled": False, "native_executed": False,
        "first_scope": "Linux-x86_64-Clang-ELF-paired-static-native-component",
        "requires_actual_source_toolchain_provider_and_cgroup_guardian": True,
        "memory_max_bytes": 64 * 1024 ** 3, "swap_max_bytes": 0,
        "steps": steps,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", required=True)
    parser.add_argument("--build-root", required=True)
    for key, default in [("clang", "clang"), ("clangxx", "clang++"), ("cmake", "cmake"),
                         ("ninja", "ninja"), ("readelf", "readelf"), ("nm", "llvm-nm"),
                         ("objdump", "llvm-objdump")]:
        parser.add_argument("--" + key, default=default)
    args = parser.parse_args()
    print(json.dumps(plan(args.source_root, args.build_root, args.clang, args.clangxx,
                          args.cmake, args.ninja, args.readelf, args.nm, args.objdump),
                     indent=2, sort_keys=True))
