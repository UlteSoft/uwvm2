#!/usr/bin/env python3
"""Execute a selected, auditable subset of official Core 3 WAST assertions.

Only i32/i64 ``assert_return`` and the numeric actions required to reach them
are translated. The original module's sections, including custom sections,
are retained in their original order. Four sections acquire one new function
type, function, export and body for a self-checking ``_start``. This does not
claim execution of every WAST assertion or full Core 3 conformance.
"""

import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import signal
import subprocess
import sys

CI_ROOT = Path(__file__).resolve().parents[3] / "tools/ci"
sys.path.insert(0, str(CI_ROOT))
from wasm3_compile_fatal_policy import negative_classification, plain_output, prove_compile_fatal_trap
from requalify_wasm3_cli_from_provenance import embedded_source_ids


SUITE_COMMIT = "b464a4cd100d98175ae6e3890db89a2e6c8302f7"
GROUPS = {
    "call_ref": ((0,), ("function-references",)),
    "return_call_ref": ((0,), ("function-references", "tail-call")),
    "try_table": ((1, 2, 4, 5), ("exceptions", "tail-call", "function-references")),
    "throw_ref": ((0,), ("exceptions",)),
    "ref_cast": ((1,), ("gc",)),
    "array": ((2, 3, 4, 5), ("gc",)),
    "memory64": ((5, 6, 7, 8), ("memory64",)),
    "table_size64": ((0,), ("table64",)),
}
# Pinned WAST commit plus these 17 group indices yielded these exact commands.
# A filtered --case run uses the matching subset and is explicitly partial.
FULL_GROUPS = 17
FULL_NUMERIC_ASSERTIONS = 177
FULL_NUMERIC_ACTIONS = 2
WASMTIME_BASE = ("all-proposals=n", "bulk-memory=y", "multi-value=y",
                  "reference-types=y", "simd=y")
WASMTIME_FEATURES = {
    "gc": ("function-references=y", "gc=y"),
    "function-references": ("function-references=y",),
    "memory64": ("memory64=y",),
    "table64": ("memory64=y",),
    "exceptions": ("exceptions=y",),
    "tail-call": ("tail-call=y",),
}
ANSI = re.compile(rb"\x1b\[[0-?]*[ -/]*[@-~]")


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def save(path, value):
    tmp = path.with_name(path.name + ".tmp")
    tmp.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")
    tmp.replace(path)


def uleb(data, pos):
    result = shift = 0
    while True:
        if pos >= len(data) or shift > 35:
            raise ValueError("truncated or oversized u32 LEB")
        byte = data[pos]
        pos += 1
        result |= (byte & 127) << shift
        if byte < 128:
            if result > 0xFFFFFFFF:
                raise ValueError("oversized u32 LEB")
            return result, pos
        shift += 7


def encode_uleb(value):
    if not 0 <= value <= 0xFFFFFFFF:
        raise ValueError("u32 section value out of range")
    result = bytearray()
    while value >= 128:
        result.append((value & 127) | 128)
        value >>= 7
    result.append(value)
    return bytes(result)


def encode_sleb(value):
    result = bytearray()
    while True:
        byte = value & 127
        value >>= 7
        finished = (value == 0 and byte < 64) or (value == -1 and byte >= 64)
        result.append(byte | (0 if finished else 128))
        if finished:
            return bytes(result)


def wasm_name(data, pos):
    length, pos = uleb(data, pos)
    end = pos + length
    if end > len(data):
        raise ValueError("truncated Wasm name")
    return data[pos:end].decode("utf-8"), end


def flattened_type_count(payload):
    """Count type indices, including every member of a recursive type group.

    The Core 3 binary type section counts *rectype entries*, while function
    type indices count their flattened subtypes. See the official binary type
    grammar: https://webassembly.github.io/spec/core/binary/types.html.
    Parsing here also rejects a malformed suffix instead of guessing an index.
    """
    def byte(pos):
        if pos >= len(payload):
            raise ValueError("truncated type section")
        return payload[pos]

    def signed_leb(pos):
        for _ in range(5):  # a heaptype index is s33
            current = byte(pos)
            pos += 1
            if current < 128:
                return pos
        raise ValueError("oversized heaptype index")

    def valtype(pos):
        kind = byte(pos)
        pos += 1
        if kind in (0x63, 0x64):
            return signed_leb(pos)
        if kind in (*range(0x69, 0x75), *range(0x7B, 0x80)):
            return pos
        raise ValueError(f"unsupported value type 0x{kind:02x}")

    def fieldtype(pos):
        if byte(pos) in (0x77, 0x78):
            pos += 1  # packed i16/i8 storage type
        else:
            pos = valtype(pos)
        if byte(pos) not in (0, 1):
            raise ValueError("invalid field mutability")
        return pos + 1

    def subtype(pos):
        if byte(pos) in (0x4F, 0x50):
            count, pos = uleb(payload, pos + 1)
            for _ in range(count):
                _, pos = uleb(payload, pos)
        kind = byte(pos)
        pos += 1
        if kind == 0x5E:  # array(fieldtype)
            return fieldtype(pos)
        if kind == 0x5F:  # struct(vec(fieldtype))
            count, pos = uleb(payload, pos)
            for _ in range(count):
                pos = fieldtype(pos)
            return pos
        if kind == 0x60:  # func(vec(valtype), vec(valtype))
            for _ in range(2):
                count, pos = uleb(payload, pos)
                for _ in range(count):
                    pos = valtype(pos)
            return pos
        raise ValueError(f"unsupported composite type 0x{kind:02x}")

    entries, pos = uleb(payload, 0)
    flattened = 0
    for _ in range(entries):
        if byte(pos) == 0x4E:  # rec(vec(subtype))
            members, pos = uleb(payload, pos + 1)
            for _ in range(members):
                pos = subtype(pos)
            flattened += members
        else:
            pos = subtype(pos)
            flattened += 1
    if pos != len(payload):
        raise ValueError("unparsed type section suffix")
    return entries, flattened


def sections_of(data):
    if data[:8] != b"\0asm\1\0\0\0":
        raise ValueError("not a Wasm v1 binary")
    pos, sections, seen = 8, [], set()
    while pos < len(data):
        section_id = data[pos]
        size, body = uleb(data, pos + 1)
        end = body + size
        if end > len(data):
            raise ValueError("section extends past input")
        if section_id and section_id in seen:
            raise ValueError(f"duplicate section {section_id}")
        seen.add(section_id)
        sections.append((section_id, data[body:end]))
        pos = end
    return sections


def imported_function_count(payload):
    if payload is None:
        return 0
    count, pos = uleb(payload, 0)
    functions = 0
    for _ in range(count):
        _, pos = wasm_name(payload, pos)
        _, pos = wasm_name(payload, pos)
        if pos >= len(payload):
            raise ValueError("truncated import kind")
        kind = payload[pos]
        pos += 1
        if kind == 0:  # function type index
            _, pos = uleb(payload, pos)
            functions += 1
        elif kind == 4:  # exception tag, attribute + type index
            if pos >= len(payload):
                raise ValueError("truncated tag import")
            pos += 1
            _, pos = uleb(payload, pos)
        else:
            raise ValueError(f"unsupported import kind {kind}")
    if pos != len(payload):
        raise ValueError("unparsed import suffix")
    return functions


def numeric_const(value):
    width = {"i32": 32, "i64": 64}[value["type"]]
    integer = int(value["value"])
    if not -(1 << (width - 1)) <= integer < (1 << width):
        raise ValueError("numeric WAST value out of range")
    if integer >= 1 << (width - 1):
        integer -= 1 << width
    return bytes((0x41 if width == 32 else 0x42,)) + encode_sleb(integer)


def numeric_command(command):
    if command["type"] not in ("assert_return", "action"):
        return False
    action = command.get("action", {})
    if action.get("type") != "invoke":
        return False
    arguments = action.get("args", [])
    expected = command.get("expected", [])
    return (all(item["type"] in ("i32", "i64") for item in arguments)
            and len(expected) <= 1
            and all(item["type"] in ("i32", "i64") for item in expected))


def grouped(commands):
    groups = []
    for command in commands:
        if command["type"] == "module":
            groups.append({"module": command, "commands": []})
        elif groups:
            groups[-1]["commands"].append(command)
    return groups


def foreign_module_invokes(group):
    """Never redirect a named action to the newest module's same-named export."""
    current_name = group["module"].get("name")
    return [command for command in group["commands"]
            if command.get("action", {}).get("type") == "invoke"
            and command["action"].get("module") not in (None, current_name)]


def wrap(original, commands):
    sections = sections_of(original)
    source = {section_id: payload for section_id, payload in sections if section_id}
    if not {1, 3, 7, 10} <= source.keys() or 8 in source:
        raise ValueError("wrapper requires type/function/export/code and no start section")
    imported = imported_function_count(source.get(2))
    types, type_index = flattened_type_count(source[1])
    _, pos = uleb(source[1], 0)
    type_payload = encode_uleb(types + 1) + source[1][pos:] + b"\x60\x00\x00"
    functions, pos = uleb(source[3], 0)
    function_payload = encode_uleb(functions + 1) + source[3][pos:] + encode_uleb(type_index)
    function_index = imported + functions
    exports, pos = uleb(source[7], 0)
    old_export_start = pos
    names = {}
    for _ in range(exports):
        name, pos = wasm_name(source[7], pos)
        if pos >= len(source[7]):
            raise ValueError("truncated export kind")
        kind = source[7][pos]
        index, pos = uleb(source[7], pos + 1)
        if kind == 0:
            names[name] = index
    if pos != len(source[7]) or "_start" in names:
        raise ValueError("invalid exports or existing _start")
    export_payload = (encode_uleb(exports + 1) + source[7][old_export_start:]
                      + b"\x06_start\x00" + encode_uleb(function_index))
    body = bytearray(b"\x00")  # zero local declarations
    for command in commands:
        action = command["action"]
        body.extend(b"".join(numeric_const(argument) for argument in action.get("args", [])))
        body.extend(b"\x10" + encode_uleb(names[action["field"]]))
        if command["type"] == "assert_return" and command.get("expected"):
            expected = command["expected"][0]
            body.extend(numeric_const(expected))
            body.extend(b"\x47" if expected["type"] == "i32" else b"\x52")
            body.extend(b"\x04\x40\x00\x0b")  # if unequal: unreachable
    body.append(0x0B)
    bodies, pos = uleb(source[10], 0)
    if bodies != functions:
        raise ValueError("function and code counts differ")
    code_payload = (encode_uleb(bodies + 1) + source[10][pos:]
                    + encode_uleb(len(body)) + body)
    replaced = {1: type_payload, 3: function_payload,
                7: export_payload, 10: code_payload}
    output = bytearray(original[:8])
    for section_id, old_payload in sections:
        payload = replaced.get(section_id, old_payload)
        output.extend(bytes((section_id,)) + encode_uleb(len(payload)) + payload)
    if [section_id for section_id, _ in sections] != [section_id for section_id, _ in sections_of(output)]:
        raise ValueError("section order changed")
    return bytes(output)


def run(command, log, expected=0, required=None, timeout=120):
    command = [str(item) for item in command]
    timed_out = False
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               start_new_session=True)
    try:
        raw, _ = process.communicate(timeout=timeout)
        status = process.returncode
    except subprocess.TimeoutExpired:
        timed_out = True
        # Stop the command's descendants too; a timed-out test must not leave
        # a product process running outside this result's observation window.
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        raw, _ = process.communicate()
        status = -124
        raw += b"\nTIMEOUT\n"
    log.write_bytes(raw)
    plain = ANSI.sub(b"", raw).decode(errors="replace")
    passed = (not timed_out and (status == 0 if expected == 0 else status > 0)
              and (required is None or required in plain))
    return {"command": command, "exit": status, "passed": passed,
            "timed_out": timed_out,
            "required_diagnostic": required, "log": str(log),
            "log_sha256": sha(log)}


def modes(ros):
    if ros:
        return {"int-full": ("-Rint",),
                "jit-full-instruction": ("-Raot", "-Rllvm-call-stack", "instruction",
                                         "-Rllvm-cache-path", "disable"),
                "jit-full-unwind": ("-Raot", "-Rllvm-call-stack", "unwind",
                                    "-Rllvm-cache-path", "disable")}
    result = {f"int-{kind}": ("-Rcc", "int", "-Rcm", kind)
              for kind in ("full", "lazy", "lazy+verification")}
    for kind in ("full", "lazy"):
        for trace in ("instruction", "unwind"):
            result[f"jit-{kind}-{trace}"] = (
                "-Rcc", "jit", "-Rcm", kind, "-Rllvm-call-stack", trace,
                "-Rllvm-cache-path", "disable")
    return result


def fingerprint(source, path):
    return subprocess.check_output([sys.executable,
        str(source / "tools/ci/wasm3_source_fingerprint.py"), source, path], text=True).strip()


def memory_events():
    return {name: int(value) for name, value in
            (line.split() for line in Path("/sys/fs/cgroup/memory.events").read_text().splitlines())}


def cgroup_state():
    root = Path("/sys/fs/cgroup")
    return {name: (root / name).read_text().strip() for name in
            ("memory.max", "memory.swap.max", "memory.current", "memory.peak",
             "cpuset.cpus.effective", "cpu.stat")}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--expected-source-id", required=True)
    parser.add_argument("--build-json", type=Path, required=True)
    parser.add_argument("--suite", type=Path, required=True)
    parser.add_argument("--wasm-tools", type=Path, required=True)
    parser.add_argument("--wasmtime", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--ros", action="store_true")
    parser.add_argument("--case", action="append", choices=GROUPS)
    parser.add_argument("--mode", action="append")
    parser.add_argument("--gdb", type=Path,
                        help="optional exact-ELF per-negative SIGILL/ud2 compile-fatal PC proof; default rejects signals")
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    subprocess.run(["bash", source / "tools/ci/require_wasm3_test_cgroup.sh"], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    output = args.out.absolute()
    output.mkdir(parents=True, exist_ok=False)
    source_id = fingerprint(source, output / "source-before.json")
    if source_id != args.expected_source_id:
        raise RuntimeError("requested frozen source ID mismatch; no Wasm conversion or execution started")
    build_path = args.build_json.resolve(strict=True)
    build = json.loads(build_path.read_text())
    binary = build_path.parent / "uwvm"
    binary_sha = sha(binary)
    if (build.get("source_id") != source_id or
            Path(build.get("source", "")).resolve() != source or
            build.get("binary_sha256") != binary_sha or
            build.get("binary_embedded_source_ids") != [source_id] or
            embedded_source_ids(binary) != {source_id}):
        raise RuntimeError("source, build and ELF are not the same product")
    suite = args.suite.resolve(strict=True)
    commit = subprocess.check_output(["git", "-C", suite, "rev-parse", "HEAD"], text=True).strip()
    tree = subprocess.check_output(["git", "-C", suite, "rev-parse", "HEAD^{tree}"], text=True).strip()
    if commit != SUITE_COMMIT or subprocess.check_output(
            ["git", "-C", suite, "status", "--porcelain"], text=True).strip():
        raise RuntimeError("official test suite checkout differs from pinned clean commit")
    wasm_tools = args.wasm_tools.resolve(strict=True)
    wasmtime = args.wasmtime.resolve(strict=True)
    version = subprocess.check_output([wasmtime, "--version"], text=True).strip()
    if not version.startswith("wasmtime 49.0.1 "):
        raise RuntimeError(f"Wasmtime 49.0.1 required: {version}")
    available = modes(args.ros)
    selected_modes = tuple(dict.fromkeys(args.mode or available))
    if any(mode not in available for mode in selected_modes):
        parser.error(f"mode must be one of: {', '.join(available)}")
    selected_cases = tuple(dict.fromkeys(args.case or GROUPS))
    before_cgroup = cgroup_state()
    before_events = memory_events()
    records, skipped, executions = [], [], []
    for stem in selected_cases:
        group_indices, features = GROUPS[stem]
        wast = suite / (stem + ".wast")
        folder = output / stem
        folder.mkdir(parents=True)
        commands_path = folder / "commands.json"
        conversion = run([wasm_tools, "json-from-wast", wast,
                          "--wasm-dir", folder, "-o", commands_path],
                         folder / "conversion.log")
        if not conversion["passed"]:
            raise RuntimeError(f"{stem}: WAST conversion failed")
        groups = grouped(json.loads(commands_path.read_text())["commands"])
        for index in group_indices:
            if index >= len(groups):
                raise RuntimeError(f"{stem}: selected group {index} missing")
            group = groups[index]
            selected = [command for command in group["commands"] if numeric_command(command)]
            assertions = sum(command["type"] == "assert_return" for command in selected)
            original_command_types = Counter(command["type"] for command in group["commands"])
            unsafe_host_actions = [command for command in group["commands"]
                                   if command["type"] == "register" or
                                   (command["type"] == "action" and
                                    not numeric_command(command))]
            selected_ids = {id(command) for command in selected}
            omitted = [command for command in group["commands"]
                       if id(command) not in selected_ids]
            other_module_actions = foreign_module_invokes(group)
            if other_module_actions:
                skipped.append({"stem": stem, "group": index,
                                "reason": "named invoke targets another module; cannot faithfully append this wrapper",
                                "assertions": assertions,
                                "original_commands_by_type": dict(original_command_types),
                                "current_module_name": group["module"].get("name"),
                                "lines": [command.get("line") for command in other_module_actions]})
                continue
            if unsafe_host_actions or not selected:
                skipped.append({"stem": stem, "group": index,
                                "reason": "unsupported host action or no numeric action/assertion",
                                "assertions": assertions,
                                "original_commands_by_type": dict(original_command_types),
                                "lines": [command.get("line") for command in unsafe_host_actions]})
                continue
            original = folder / group["module"]["filename"]
            raw = original.read_bytes()
            if 8 in dict(sections_of(raw)) or (2 in dict(sections_of(raw)) and
                    (stem != "try_table" or index not in (1, 2))):
                skipped.append({"stem": stem, "group": index,
                                "reason": "start or unsupported imports", "assertions": assertions,
                                "original_commands_by_type": dict(original_command_types)})
                continue
            try:
                wrapped = wrap(raw, selected)
            except (KeyError, ValueError) as failure:
                skipped.append({"stem": stem, "group": index,
                                "reason": "wrapper: " + str(failure), "assertions": assertions,
                                "original_commands_by_type": dict(original_command_types)})
                continue
            original_sections = sections_of(raw)
            wrapped_sections = sections_of(wrapped)
            retained = [
                {"ordinal": ordinal, "id": section_id,
                 "payload_sha256": hashlib.sha256(payload).hexdigest()}
                for ordinal, (section_id, payload) in enumerate(original_sections)
                if section_id not in (1, 3, 7, 10)
            ]
            if (len(original_sections) != len(wrapped_sections) or
                    any(old_id != new_id or
                        (old_id not in (1, 3, 7, 10) and old_payload != new_payload)
                        for (old_id, old_payload), (new_id, new_payload)
                        in zip(original_sections, wrapped_sections))):
                raise RuntimeError(f"{stem}/{index}: original section payload changed")
            wasm = folder / f"{stem}-{index}-numeric.wasm"
            wasm.write_bytes(wrapped)
            validation = run([wasm_tools, "validate", "--features", "all", wasm],
                             folder / f"{index}-validate.log")
            if not validation["passed"]:
                raise RuntimeError(f"{stem}/{index}: wrapped module does not validate")
            record = {"stem": stem, "group": index, "features": features,
                      "official_wast_sha256": sha(wast),
                      "original_module_sha256": sha(original),
                      "wrapped_module_sha256": sha(wasm),
                      "original_retained_sections": retained,
                      "assertions": assertions, "numeric_actions": len(selected) - assertions,
                      "original_commands_by_type": dict(original_command_types),
                      "lines": [command["line"] for command in selected],
                      "omitted_wast_commands": [
                          {"type": command["type"], "line": command.get("line")}
                          for command in omitted],
                      "validation": validation}
            records.append(record)
            preload = folder / groups[0]["module"]["filename"] if stem == "try_table" and index in (1, 2) else None
            oracle_features = dict.fromkeys((*WASMTIME_BASE,
                *(flag for feature in features for flag in WASMTIME_FEATURES[feature])))
            oracle = [wasmtime, "run", "-C", "cache=n", "-W", ",".join(oracle_features)]
            if preload is not None:
                oracle.extend(("--preload", "test=" + str(preload)))
            oracle.append(wasm)
            reference = run(oracle, folder / f"{index}-wasmtime49.log")
            executions.append({"stem": stem, "group": index, "engine": "wasmtime49",
                               "mode": "reference", **reference})
            if not reference["passed"]:
                raise RuntimeError(f"{stem}/{index}: Wasmtime49 oracle failed")
            for mode in selected_modes:
                enabled = ["-WFE-" + feature for feature in features]
                argv = [binary, *available[mode], "-Rct", "0", *enabled]
                if preload is not None:
                    argv.extend(("-Wpre", preload, "test"))
                argv.extend(("--run", wasm))
                result = run(argv, folder / f"{index}-{mode}.log")
                executions.append({"stem": stem, "group": index,
                                   "engine": "uwvm", "mode": mode, **result})
                save(output / "progress.json", {"fixtures": records,
                                                 "skipped": skipped,
                                                 "executions": executions})
                if not result["passed"]:
                    raise RuntimeError(f"{stem}/{index}/{mode}: official numeric assertions failed")
            # Feature rejection is a validator property, so int-full checks
            # each proposal independently while the remaining gates stay on.
            for feature in dict.fromkeys(features):
                flags = ["-WFD-" + feature if item == feature else "-WFE-" + item
                         for item in features]
                argv = [binary, *available["int-full"], "-Rct", "0", *flags]
                if preload is not None:
                    argv.extend(("-Wpre", preload, "test"))
                argv.extend(("--run", wasm))
                gate = run(argv, folder / f"{index}-{feature}-off.log", expected=1,
                           required="--wasm-feature-enable-" + feature)
                proof = None
                required = gate["required_diagnostic"]
                if gate["exit"] == -signal.SIGILL and args.gdb is not None and not gate["timed_out"]:
                    proof = prove_compile_fatal_trap(argv, required, args.gdb,
                        folder / f"{index}-{feature}-off-fatal-pc.log",
                        binary_sha256=binary_sha)
                classification, accepted = negative_classification(
                    gate["exit"], required in plain_output(Path(gate["log"]).read_bytes()), proof)
                gate.update(negative_classification=classification, fatal_pc_proof=proof,
                            passed=accepted and not gate["timed_out"])
                executions.append({"stem": stem, "group": index,
                                   "engine": "uwvm", "mode": "int-full",
                                   "disabled_feature": feature, **gate})
                save(output / "progress.json", {"fixtures": records,
                                                 "skipped": skipped,
                                                 "executions": executions})
                if not gate["passed"]:
                    raise RuntimeError(f"{stem}/{index}: {feature} feature-off gate failed")
    after_id = fingerprint(source, output / "source-after.json")
    after_cgroup = cgroup_state()
    after_events = memory_events()
    if (after_id != source_id or sha(binary) != binary_sha or
            any(after_events[name] != before_events[name] for name in ("oom", "oom_kill"))):
        raise RuntimeError("product, source or cgroup OOM changed")
    numeric_assertions = sum(row["assertions"] for row in records)
    numeric_actions = sum(row["numeric_actions"] for row in records)
    omitted_types = Counter(command["type"] for row in records
                            for command in row["omitted_wast_commands"])
    skipped_group_types = Counter()
    for row in skipped:
        skipped_group_types.update(row["original_commands_by_type"])
    coverage_complete = (len(records) == FULL_GROUPS and
                         numeric_assertions == FULL_NUMERIC_ASSERTIONS and
                         numeric_actions == FULL_NUMERIC_ACTIONS and not skipped)
    summary = {"passed": bool(records) and all(row["passed"] for row in executions)
               and not skipped and (coverage_complete if len(selected_cases) == len(GROUPS) else True),
               "scope": "selected official i32/i64 WAST assertions and numeric actions only",
               "expected_source_id": args.expected_source_id,
               "source_id": source_id, "binary_sha256": binary_sha,
               "build_json_sha256": sha(build_path), "suite_commit": commit,
               "suite_tree": tree, "wasm_tools_sha256": sha(wasm_tools),
               "wasmtime_sha256": sha(wasmtime), "wasmtime_version": version,
               "runner_sha256": sha(Path(__file__)),
               "qualification_helper_sha256": {
                   name: sha(CI_ROOT / name) for name in
                   ("wasm3_compile_fatal_policy.py", "requalify_wasm3_cli_from_provenance.py")},
               "negative_policy": "strict diagnostic exit or individual exact-ELF GDB compile-fatal PC proof; raw exit retained",
               "requested_groups": {stem: GROUPS[stem][0] for stem in selected_cases},
               "selected_modes": selected_modes,
               "fixtures": records,
               "skipped": skipped, "executions": executions,
               "numeric_assertions": numeric_assertions,
               "numeric_actions": numeric_actions,
               "omitted_commands_by_type": dict(omitted_types),
               "omitted_assertions_in_executed_groups": sum(count for kind, count in omitted_types.items()
                                                            if kind.startswith("assert_")),
               "skipped_group_commands_by_type": dict(skipped_group_types),
               "assertions_in_skipped_groups": sum(count for kind, count in skipped_group_types.items()
                                                    if kind.startswith("assert_")),
               "full_selected_subset_complete": coverage_complete,
               "full_selected_subset_expected": {"groups": FULL_GROUPS,
                                                  "numeric_assertions": FULL_NUMERIC_ASSERTIONS,
                                                  "numeric_actions": FULL_NUMERIC_ACTIONS},
               "skipped_numeric_assertions": sum(row["assertions"] for row in skipped),
               "feature_off_checks": sum("disabled_feature" in row for row in executions),
               "cgroup_before": before_cgroup, "cgroup_after": after_cgroup,
               "cgroup_events_before": before_events,
               "cgroup_events_after": after_events}
    save(output / "summary.json", summary)
    print(f"{'PASS' if summary['passed'] else 'FAIL'} "
          f"{summary['numeric_assertions']} official numeric assertions and "
          f"{summary['numeric_actions']} actions; "
          f"{len(executions)} native/reference/gate executions; {len(skipped)} skipped groups")
    if not summary["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
