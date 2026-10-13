#!/usr/bin/env python3
"""Extract the exact checkpoint branch for isolated controller tests.

Runtime calls are replaced by private-namespace API-result doubles in the C++
fixture. No native capsule, pause proof, host gate or VM restore is fabricated.
"""
import argparse
import hashlib
import json
from pathlib import Path

BEGIN = b"            if(query.operation==wasip1_state::action::checkpoint_save ||"
END = b"            result.wasip1_state_values = ::uwvm2::runtime::lib::llvm_jit_debug_query_wasip1_state_host_api("

def extract(source):
    if source.count(BEGIN) != 1 or source.count(END) != 1:
        raise ValueError("checkpoint branch anchors are not unique")
    first, last = source.index(BEGIN), source.index(END)
    if last <= first:
        raise ValueError("checkpoint branch extent is invalid")
    return source[first:last]

def generate(source, destination):
    raw = source.read_bytes()
    branch = extract(raw)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(branch)
    proof = {"controller": str(source), "controller_sha256": hashlib.sha256(raw).hexdigest(),
        "fragment": str(destination), "fragment_sha256": hashlib.sha256(branch).hexdigest(),
        "start_byte": raw.index(BEGIN), "end_byte": raw.index(END),
        "exact_source_bytes": True, "runtime_api_result_doubles": True}
    destination.with_suffix(".json").write_text(json.dumps(proof, indent=2) + "\n")
    return proof

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("controller", type=Path)
    parser.add_argument("fragment", type=Path)
    args = parser.parse_args()
    print(json.dumps(generate(args.controller, args.fragment)))
