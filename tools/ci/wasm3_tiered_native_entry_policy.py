#!/usr/bin/env python3
"""Prove a forced T1 entry in an actual source-bound Linux x86-64 ELF.

Disabling T0 and T2 takes the direct LLVM lazy path and does not increment the
T0-to-native switch counter. Stop its real raw-entry indirect call, single-step
into the generated executable mapping, then require the guest to return normally.
This is a native entry witness, not a T2 witness or a timing measurement.
"""

import hashlib
import json
from pathlib import Path
import re
import subprocess


RAW_HELPER = "_ZN5uwvm27runtime3lib12_GLOBAL__N_145try_invoke_runtime_llvm_jit_raw_defined_entryEmmPvmPKvmb"
CALL = re.compile(r"^\s*([0-9a-f]+):[^\n]*\tcallq?\s+\*%([a-z0-9]+)\s*$", re.M)
MAPPING = re.compile(r"^[ \t]*(0x[0-9a-f]+)[ \t]+(0x[0-9a-f]+)[ \t]+0x[0-9a-f]+[ \t]+0x[0-9a-f]+[ \t]+([rwxps-]+)(?:[ \t]+([^\n]*))?$", re.M)


def sha(path):
    with Path(path).open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def prepare_native_entry_site(binary, out, *, llvm_nm, llvm_objdump):
    """Derive the instruction offset from this ELF, never a prior candidate."""
    symbols = subprocess.check_output([str(llvm_nm), "-S", "--defined-only", str(binary)], text=True)
    matches = [line.split() for line in symbols.splitlines() if line.endswith(" " + RAW_HELPER)]
    if len(matches) != 1 or len(matches[0]) != 4:
        raise RuntimeError("one actual raw-entry helper symbol is required")
    address, size = (int(value, 16) for value in matches[0][:2])
    command = [str(llvm_objdump), "-d", "--start-address=" + hex(address),
               "--stop-address=" + hex(address + size), str(binary)]
    assembly = subprocess.check_output(command, text=True)
    path = out / "forced-t1-raw-helper.objdump"
    path.write_text(assembly)
    calls = CALL.findall(assembly)
    if "file format elf64-x86-64" not in assembly or len(calls) != 1:
        raise RuntimeError("the actual x86-64 raw helper must have one indirect entry call")
    call_address, register = int(calls[0][0], 16), calls[0][1]
    if not address <= call_address < address + size:
        raise RuntimeError("raw-entry call falls outside its actual ELF symbol")
    return {"symbol": RAW_HELPER, "elf_symbol_address": address, "elf_symbol_size": size,
            "call_offset": call_address - address, "target_register": register,
            "disassembly_command": command, "disassembly_sha256": sha(path),
            "llvm_nm_sha256": sha(llvm_nm), "llvm_objdump_sha256": sha(llvm_objdump)}


def parse_native_entry_log(text):
    called = re.findall(r"UWVM_T1_RAW_CALL pc=(0x[0-9a-f]+) target=(0x[0-9a-f]+) local_function=(\d+) module=(\d+)", text)
    entered = re.findall(r"UWVM_T1_RAW_ENTER pc=(0x[0-9a-f]+) target=(0x[0-9a-f]+)", text)
    if len(called) != 1 or len(entered) != 1:
        return {"entered_generated_raw_entry": False, "reason": "one call and one actual stepped PC are required"}
    call_pc, target, local, module = called[0]
    actual, expected = entered[0]
    target_value = int(target, 16)
    mappings = [{"start": int(start, 16), "end": int(end, 16), "permissions": permissions,
                 "path": (path or "").strip()} for start, end, permissions, path in MAPPING.findall(text)
                if int(start, 16) <= target_value < int(end, 16)]
    executable = (len(mappings) == 1 and "x" in mappings[0]["permissions"] and
                  (not mappings[0]["path"] or mappings[0]["path"].startswith("[anon")))
    normal = "exited normally" in text
    return {"entered_generated_raw_entry": (target_value != 0 and actual == expected == target and
                                            call_pc != target and executable and normal),
            "raw_call_pc": call_pc, "generated_target": target, "actual_stepped_pc": actual,
            "local_function_index": int(local), "module_id": int(module),
            "target_mappings": mappings, "guest_returned_normally": normal}


def prove_forced_t1_raw_entry(command, debugger, evidence_path, site, *, binary_sha256, environment):
    if sha(command[0]) != binary_sha256:
        raise RuntimeError("actual ELF differs before forced T1 entry proof")
    if (command.count("-Rtiered-disable-t0") != 1 or command.count("-Rtiered-disable-t2") != 1 or
            command[command.index("-Rcc") + 1] != "tiered" or command[command.index("-Rcm") + 1] != "lazy"):
        raise RuntimeError("raw-entry proof is only for the explicit T0/T2-disabled lazy mode")
    script = evidence_path.with_suffix(".gdb")
    register = "$" + site["target_register"]
    script.write_text(
        "set pagination off\nset confirm off\nset print thread-events off\n"
        "set breakpoint pending off\nset disable-randomization off\n"
        "tbreak *" + site["symbol"] + "+" + hex(site["call_offset"]) + "\nrun\n"
        "printf \"UWVM_T1_RAW_CALL pc=%p target=%p local_function=%lu module=%lu\\n\", $pc, " + register + ", $r13, $rbp\n"
        "set $uwvm_t1_target=" + register + "\nstepi\n"
        "printf \"UWVM_T1_RAW_ENTER pc=%p target=%p\\n\", $pc, $uwvm_t1_target\n"
        "x/4i $pc\ninfo proc mappings\ncontinue\n")
    actual_command = list(map(str, command))
    # Preserve the first execution's compiler log as a separate qualification input.
    if "-Rclog" in actual_command:
        index = actual_command.index("-Rclog")
        if actual_command[index + 1] != "file":
            raise RuntimeError("file compiler logging is required")
        actual_command[index + 2] = str(evidence_path.with_suffix(".compile.log"))
    argv = [str(debugger), "--batch", "--nx", "-q", "-x", str(script), "--args", *actual_command]
    timed_out = False
    try:
        process = subprocess.run(argv, env=environment, capture_output=True, timeout=180)
        raw, status = process.stdout + process.stderr, process.returncode
    except subprocess.TimeoutExpired as error:
        raw, status = (error.stdout or b"") + (error.stderr or b""), -124
        timed_out = True
    evidence_path.write_bytes(raw)
    proof = parse_native_entry_log(raw.decode(errors="replace"))
    proof.update(command=argv, debugger_exit=status, timed_out=timed_out,
                 debugger_sha256=sha(debugger), binary_sha256=binary_sha256,
                 gdb_script_sha256=sha(script), log_sha256=sha(evidence_path), raw_entry_site=site,
                 native_entry_policy_sha256=sha(Path(__file__)))
    proof["entered_generated_raw_entry"] = (proof["entered_generated_raw_entry"] and
                                            status == 0 and not timed_out and sha(command[0]) == binary_sha256)
    evidence_path.with_suffix(".json").write_text(json.dumps(proof, indent=2) + "\n")
    return proof
