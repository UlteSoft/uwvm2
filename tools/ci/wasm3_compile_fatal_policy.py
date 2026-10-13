#!/usr/bin/env python3
"""Classify Linux x86-64 compile-validation traps using the actual stopped PC.

A printed error is insufficient evidence for accepting a signal. The optional
GDB replay must stop in the product's compile-validation fatal helper at the
compiler-generated ud2 instruction. This policy does not accept runtime traps,
other signals, debugger failures, or evidence from a different executable.
"""

import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess


ANSI = re.compile(rb"\x1b\[[0-?]*[ -/]*[@-~]")
PC_MARKER = "UWVM_COMPILE_FATAL_PC_JSON="
FATAL_FUNCTION = "print_and_terminate_compile_validation_error"


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def plain_output(raw):
    return ANSI.sub(b"", raw).decode(errors="replace")


def pc_evidence(raw):
    """Read one debugger-produced PC record; reject ambiguous or partial output."""
    records = [line[len(PC_MARKER):] for line in plain_output(raw).splitlines()
               if line.startswith(PC_MARKER)]
    if len(records) != 1:
        return None
    try:
        value = json.loads(records[0])
    except (TypeError, ValueError):
        return None
    if not isinstance(value, dict):
        return None
    return value if valid_pc_record(value) else None


def valid_pc_record(value):
    if not isinstance(value, dict):
        return False
    pc = value.get("pc")
    instruction = value.get("instruction")
    function = value.get("function")
    program_state = value.get("program_state")
    if (type(pc) is not int or pc <= 0 or
            not isinstance(function, str) or FATAL_FUNCTION not in function or
            not isinstance(instruction, str) or instruction.strip() != "ud2" or
            value.get("instruction_bytes") != "0f0b" or
            not isinstance(program_state, str) or
            "It stopped with signal SIGILL" not in program_state):
        return False
    return True


def negative_classification(status, diagnostic_present, proof=None):
    """Keep regular diagnostic exits and individually proven fatal traps distinct."""
    if not diagnostic_present:
        return "MISSING_REQUIRED_DIAGNOSTIC", False
    if status > 0:
        return "EXPECT_COMPILE_DIAGNOSTIC_EXIT", True
    if (status == -signal.SIGILL and isinstance(proof, dict) and
            proof.get("passed") is True and valid_pc_record(proof.get("pc"))):
        return "EXPECT_COMPILE_FATAL_TRAP", True
    return ("UNEXPECTED_SIGNAL" if status < 0 else "UNEXPECTED_SUCCESS"), False


def prove_compile_fatal_trap(command, diagnostic, debugger, output,
                             *, binary_sha256, environment=None, timeout=45):
    """Replay this exact negative command and save its PC/instruction/backtrace."""
    command = [str(item) for item in command]
    binary = Path(command[0]).resolve(strict=True)
    debugger = Path(debugger).resolve(strict=True)
    output = Path(output)
    before_binary_sha = sha(binary)
    if before_binary_sha != binary_sha256:
        raise RuntimeError("fatal-PC inferior differs from the qualified product executable")
    with binary.open("rb") as stream:
        header = stream.read(20)
    if (len(header) != 20 or header[:6] != b"\x7fELF\x02\x01" or
            int.from_bytes(header[18:20], "little") != 62):
        raise RuntimeError("compile-fatal PC policy requires a Linux x86-64 ELF")
    # This code runs in GDB's Python interpreter after the inferior stops. The
    # instruction and bytes come from selected_frame().pc(), not a guessed
    # offset, a text search in the module, or a printed product diagnostic.
    pc_command = (
        "python import gdb,json; f=gdb.selected_frame(); p=int(f.pc()); "
        "i=f.architecture().disassemble(p,count=1)[0]['asm']; "
        "b=gdb.selected_inferior().read_memory(p,2).tobytes().hex(); "
        "s=gdb.execute('info program',to_string=True); "
        "print('" + PC_MARKER + "'+json.dumps(dict(pc=p,function=f.name(),"
        "instruction=i,instruction_bytes=b,program_state=s),sort_keys=True))"
    )
    probe = [str(debugger), "-nx", "-q", "-batch",
             "-ex", "set pagination off", "-ex", "set confirm off",
             "-ex", "set startup-with-shell off",
             "-ex", "handle SIGILL stop print nopass",
             "-ex", "run", "-ex", pc_command, "-ex", "bt 8",
             "--args", *command]
    timed_out = False
    process = subprocess.Popen(probe, env=environment, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, start_new_session=True)
    try:
        stdout, stderr = process.communicate(timeout=timeout)
        raw, status = stdout + stderr, process.returncode
    except subprocess.TimeoutExpired:
        timed_out = True
        # GDB owns a traced inferior. Stop the complete replay group on timeout,
        # so a killed debugger cannot leave an unobserved product process alive.
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        stdout, stderr = process.communicate()
        raw = stdout + stderr
        status = -124
    output.write_bytes(raw)
    evidence = pc_evidence(raw)
    after_binary_sha = sha(binary)
    passed = (not timed_out and status == 0 and evidence is not None and
              after_binary_sha == before_binary_sha and
              diagnostic in plain_output(raw))
    return {"passed": passed, "debugger_exit": status, "timed_out": timed_out,
            "command": probe, "inferior_command": command,
            "binary_sha256": before_binary_sha, "binary_after_sha256": after_binary_sha,
            "debugger_sha256": sha(debugger),
            "policy_sha256": sha(Path(__file__)), "log": str(output),
            "log_sha256": sha(output), "pc": evidence,
            "scope": "this exact executable and this exact negative command"}
