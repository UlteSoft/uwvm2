#!/usr/bin/env python3
"""Stage an exact Windows PE and official cross-function/multi-object EH oracle.

Schema 1 keeps the historical static-libgcc/broker receipt readable. Schema 2
uses a current, source-bound full-JIT build receipt and includes the actual
multi-object fixture; a successful cross-link is not a Windows execution.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil


STEMS = ("eh-cross-function-catch-ref", "eh-cross-function-win64-seh")
MULTI_OBJECT_STEM = "eh-multi-object-win64-seh"
RUNNER = "run_windows_cross_eh_smoke_vm.ps1"


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def contains(path: Path, needle: bytes) -> bool:
    overlap = b""
    with path.open("rb") as stream:
        while chunk := stream.read(1 << 20):
            data = overlap + chunk
            if needle in data:
                return True
            overlap = data[-(len(needle) - 1):]
    return False


def require(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--schema", type=int, choices=(1, 2), default=1)
    parser.add_argument("--repository", choices=("ordinary", "ros"))
    parser.add_argument("--product", type=Path,
                        help="Schema 2 exact current full-JIT PE")
    parser.add_argument("--build-receipt", type=Path,
                        help="Schema 2 source/product/cgroup-bound cross-build receipt")
    parser.add_argument("--link-plan", type=Path)
    parser.add_argument("--link-results", type=Path)
    parser.add_argument("--oracle-manifest", type=Path, required=True)
    parser.add_argument("--uwvm-imports", type=Path, required=True)
    parser.add_argument("--broker-imports", type=Path)
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    oracle_path = args.oracle_manifest.resolve(strict=True)
    uwvm_imports_path = args.uwvm_imports.resolve(strict=True)
    runner_path = args.runner.resolve(strict=True)
    oracle = json.loads(oracle_path.read_text())
    uwvm_import_gate = json.loads(uwvm_imports_path.read_text())
    stage_origin = {}
    if args.schema == 1:
        require(args.link_plan is not None and args.link_results is not None and
                args.broker_imports is not None and args.product is None and
                args.build_receipt is None,
                "schema 1 requires its historical link/broker inputs")
        plan_path = args.link_plan.resolve(strict=True)
        results_path = args.link_results.resolve(strict=True)
        broker_imports_path = args.broker_imports.resolve(strict=True)
        plan = json.loads(plan_path.read_text())
        results = json.loads(results_path.read_text())
        broker_import_gate = json.loads(broker_imports_path.read_text())
        source_id = plan.get("source_id")
        require(plan.get("schema") == 1 and set(results) == {"broker", "uwvm"} and
                all(row["exit_code"] == 0 and not row["watchdog_exceeded"] and
                    row["oom_before"] == row["oom_after"] == 0 and
                    row["oom_kill_before"] == row["oom_kill_after"] == 0
                    for row in results.values()),
                "static-libgcc links did not complete under the cgroup")
        product = Path(results["uwvm"]["output"]).resolve(strict=True)
        require(digest(product) == results["uwvm"]["output_sha256"] and
                all("-static-libgcc" in plan["links"][name]["static_argv"]
                    for name in ("uwvm", "broker")),
                "diagnostic PE differs from static-libgcc link result")
        require(broker_import_gate.get("passed") is True and
                not broker_import_gate.get("forbidden_imports") and
                broker_import_gate.get("pe_sha256") == results["broker"]["output_sha256"],
                "historical broker has an unqualified GCC DLL import")
        build_sha256 = digest(results_path)
        status = "wasm-tools-parse-validate-and-wasmtime48-outcomes-passed"
        stems = STEMS
        stage_origin = {"link_plan_sha256": digest(plan_path),
                        "link_results_sha256": build_sha256,
                        "broker_import_gate_sha256": digest(broker_imports_path)}
    else:
        require(args.repository is not None and args.product is not None and
                args.build_receipt is not None and args.link_plan is None and
                args.link_results is None and args.broker_imports is None,
                "schema 2 requires repository, current PE and build receipt")
        build_path = args.build_receipt.resolve(strict=True)
        build = json.loads(build_path.read_text())
        source_id = build.get("source_id")
        product = args.product.resolve(strict=True)
        require(build.get("passed") is True and
                build.get("repository") == args.repository and
                build.get("source_id_after") == source_id and
                build.get("target") == "x86_64-w64-windows-gnu" and
                build.get("product_sha256") == digest(product) and
                build.get("cgroup_memory_max") == str(64 << 30) and
                build.get("cgroup_swap_max") == "0" and
                build.get("cgroup_cpuset") == "0,2,4,6,16-31" and
                build.get("oom_before") == build.get("oom_after") == 0 and
                build.get("oom_kill_before") == build.get("oom_kill_after") == 0,
                "current full-JIT build/source/product/cgroup receipt is incomplete")
        build_sha256 = digest(build_path)
        status = "wasm-tools-parse-validate-and-wasmtime49-outcomes-passed"
        stems = (*STEMS, MULTI_OBJECT_STEM)
        stage_origin = {"build_receipt_sha256": build_sha256}
    require(re.fullmatch(r"sha256:[0-9a-f]{64}", source_id or "") is not None and
            oracle.get("source_id") == source_id and
            oracle.get("status") == status and
            oracle.get("cgroup_memory_max") == str(64 << 30) and
            oracle.get("cgroup_swap_max") == "0" and
            oracle.get("cgroup_cpuset") == "0,2,4,6,16-31",
            "official oracle is not for the exact source/cgroup")
    require(contains(product, source_id.encode("ascii")) and
            uwvm_import_gate.get("passed") is True and
            not uwvm_import_gate.get("forbidden_imports") and
            uwvm_import_gate.get("pe_sha256") == digest(product) and
            uwvm_import_gate.get("source_id") == source_id,
            "PE source fingerprint or qualified GCC DLL import gate changed")
    require(runner_path.name == RUNNER, "wrong guest EH runner")
    oracle_rows = {row.get("stem"): row for row in oracle.get("fixtures", [])}
    require(set(stems) <= set(oracle_rows), "cross-function/multi-object EH oracle cases missing")
    origins = {"uwvm.exe": product, RUNNER: runner_path}
    for stem in stems:
        row = oracle_rows[stem]
        wasm = oracle_path.parent / (stem + ".wasm")
        require(digest(wasm) == row.get("wasm_sha256") and
                len(row.get("checks", [])) == 3 and
                all(check.get("passed") is True and check.get("exit_code") == 0
                    for check in row["checks"]),
                f"official parse/validation/Wasmtime result changed: {stem}")
        origins[stem + ".wasm"] = wasm
    output = args.output.resolve()
    require(not output.exists(), "refusing to overwrite EH guest stage")
    output.mkdir(mode=0o700, parents=True)
    files = {}
    for name, origin in origins.items():
        target = output / name
        shutil.copyfile(origin, target)
        files[name] = digest(target)
        require(files[name] == digest(origin), f"artifact changed during stage: {name}")
    qualification = {"schema": args.schema, "source_id": source_id,
                     "product_sha256": files["uwvm.exe"],
                     "build_sha256": build_sha256,
                     "runner_sha256": files[RUNNER],
                     "oracle_sha256": digest(oracle_path),
                     "files_sha256": {name: files[name] for name in
                                      ("uwvm.exe", *(stem + ".wasm" for stem in stems))}}
    if args.schema == 2:
        qualification.update(repository=args.repository, multi_object_required=True)
    qualification_path = output / "qualification.json"
    qualification_path.write_text(json.dumps(qualification, indent=2, sort_keys=True) + "\n")
    stage = {"schema": args.schema,
             "purpose": ("historical-v7-static-libgcc-diagnostic" if args.schema == 1
                         else "current-full-jit-multi-object-seh"),
             "source_id": source_id,
             **stage_origin,
             "oracle_manifest_sha256": digest(oracle_path),
             "uwvm_import_gate_sha256": digest(uwvm_imports_path),
             "files": {**files, "qualification.json": digest(qualification_path)},
             "qualification_sha256": digest(qualification_path)}
    (output / "stage.json").write_text(json.dumps(stage, indent=2, sort_keys=True) + "\n")
    print(json.dumps({"source_id": source_id,
                      "qualification_sha256": stage["qualification_sha256"],
                      "stage_sha256": digest(output / "stage.json")}, sort_keys=True))


if __name__ == "__main__":
    main()
