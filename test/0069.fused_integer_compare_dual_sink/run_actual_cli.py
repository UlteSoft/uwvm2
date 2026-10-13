#!/usr/bin/env python3
"""Finite keeper-only all22 integer compare real called-start+unused-body admission recipe.
The actual matching product/tools/source/provider closure must be fresh; this
runner does not compile variants or certify C++/BMI/ASM/performance.
"""
import argparse
import hashlib
import json
import pathlib
import re
import resource
import subprocess
import sys

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def invoke(argv, negative=False):
    result = subprocess.run(argv, stdin=subprocess.DEVNULL, capture_output=True, timeout=120, check=False)
    if len(result.stdout) + len(result.stderr) > 1048576: raise RuntimeError("bounded actual output over one MiB")
    output = result.stdout.decode("utf-8", "replace") + result.stderr.decode("utf-8", "replace")
    if negative:
        forbidden = re.compile(r"unknown (?:option|feature|argument)|unexpected argument|unrecognized option|no such file|permission denied|failed to (?:open|read)|not enabled|(?:support|feature).*disabled|requires.*feature", re.I)
        clean = re.sub(r"\x1b\[[0-9;]*m", "", output)
        passed = result.returncode > 0 and re.search(r"\(offset=(?:0x)?[0-9a-fA-F]+\)", clean) and re.search(r"i(?:32|64)\.(?:eqz|eq|ne|lt_s|lt_u|gt_s|gt_u|le_s|le_u|ge_s|ge_u)", clean) and re.search(r"numeric operand type mismatch|operand stack underflow", clean, re.I) and not forbidden.search(clean)
    else: passed = result.returncode == 0
    if not passed: raise RuntimeError(json.dumps(dict(argv=argv, returncode=result.returncode, output=output)))
    return dict(argv=argv, returncode=result.returncode, output=output, negative_code_diagnostic=negative)
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--uwvm", type=pathlib.Path, required=True)
    parser.add_argument("--wasm-tools", type=pathlib.Path, required=True)
    parser.add_argument("--source-id", required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--ros", action="store_true")
    parser.add_argument("--configuration")
    args = parser.parse_args()
    here = pathlib.Path(__file__).resolve().parent; root = here.parents[1]
    subprocess.run(["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    args.output.mkdir(exist_ok=False)
    names = ["integer-compare", "unused-compare-mismatch", "unused-dead-concrete", "unused-heap-bottom", "unused-first-arity", "unused-noexn"]
    targets = []; receipts = []
    for name in names:
        target = args.output / (name + ".wasm")
        receipts.append(invoke([str(args.wasm_tools), "parse", str(here / (name + ".wat")), "-o", str(target)]))
        command = [str(args.wasm_tools), "validate", str(target)]
        actual = subprocess.run(command, stdin=subprocess.DEVNULL, capture_output=True, timeout=60, check=False)
        text = actual.stdout.decode("utf-8", "replace") + actual.stderr.decode("utf-8", "replace")
        if len(actual.stdout) + len(actual.stderr) > 1048576: raise RuntimeError("bounded official diagnostic")
        if name == names[0]: good = actual.returncode == 0
        else:
            good = actual.returncode > 0 and re.search(r"\(at offset 0x[0-9a-fA-F]+\)", text) and re.search(r"type mismatch|expected[^\n]*(?:i32|i64)|not enough[^\n]*stack|stack[^\n]*(?:underflow|empty)|pop[^\n]*empty", text, re.I) and not re.search(r"unknown (?:option|feature|argument)|no such file|permission denied|not enabled|requires.*feature", text, re.I)
        if not good: raise RuntimeError(json.dumps(dict(argv=command,returncode=actual.returncode,output=text)))
        receipts.append(dict(argv=command,returncode=actual.returncode,output=text,official_body_offset_required=name!=names[0]))
        targets.append(target)
    sys.path.insert(0, str(root / "test/0014.llvm_jit"))
    from run_wasm3_multi_memory import configurations
    feature_flags = ["-WFE-gc", "-WFE-function-references", "-WFE-tail-call", "-WFE-memory64", "-WFE-exceptions"]
    profiles = []
    for label, base in configurations(args.uwvm.resolve(), args.ros):
        profiles.append((label, [x for x in base[:-1] if x != "-WFE-multi-memory"] + ["-Rct", "0", *feature_flags, "--run"]))
    profiles.append(("validator", [str(args.uwvm.resolve()), "-m", "validation", *feature_flags, "--run"]))
    if args.configuration: profiles = [profile for profile in profiles if profile[0] == args.configuration]
    if not profiles: raise RuntimeError("no actual current profiles")
    for label, command in profiles:
        for index, target in enumerate(targets):
            receipt = invoke(command + [str(target)], negative=index != 0)
            receipt.update(configuration=label,wasm_sha256=digest(target),genuine_called_start_source=index==0,called_start_execution_configuration=index==0 and label!="validator",unused_invalid_function=index!=0)
            receipts.append(receipt)
    summary = dict(source_id=args.source_id,steps=receipts,product_sha256=digest(args.uwvm),official_tool_sha256=digest(args.wasm_tools),fresh_compilation_qualified=False,module_BMI_qualified=False,ASM_qualified=False,performance_qualified=False,whole_tiered_single_walk=False)
    (args.output / "summary.json").write_text(json.dumps(summary,indent=2)+"\n")
    print(json.dumps(dict(actual_steps=len(receipts),ASM=False,performance=False)))
if __name__ == "__main__": main()
