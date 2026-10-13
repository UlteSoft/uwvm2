#!/usr/bin/env python3
"""Verify the anchored PE, oracle fixtures, and Win11 cross-function EH result."""

import argparse
import hashlib
import json
from pathlib import Path
import re


FIXTURES = ("eh-cross-function-catch-ref", "eh-cross-function-win64-seh")
MULTI_OBJECT_FIXTURE = "eh-multi-object-win64-seh"


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


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def validate_case_boundary(row: dict, product_sha256: str) -> None:
    """Reject a successful-looking code from an unretired or timed-out child."""
    require(row.get("process_started") is True and
            row.get("owned_process_retired") is True and
            row.get("assigned_before_resume") is True and
            type(row.get("owned_job_active_after_retirement")) is int and
            row["owned_job_active_after_retirement"] == 0 and
            row.get("timed_out") is False and
            row.get("log_limit_exceeded") is False and
            row.get("stream_drain_failed") is False and
            row.get("unexpected_native_exception") is False and
            row.get("controller_error") == "" and
            row.get("evidence_error") == "",
            f"Win11 EH process boundary was not qualified: {row.get('name')}")
    require(row.get("timeout_ms") == 90000 and
            row.get("sampled_log_limit_bytes") == 8388608 and
            type(row.get("elapsed_ms")) is int and row["elapsed_ms"] >= 0 and
            row.get("product_sha256_before") == product_sha256 and
            row.get("product_sha256_after") == product_sha256,
            f"Win11 EH deadline or PE changed: {row.get('name')}")
    streams = row.get("raw_streams", {})
    require(type(streams) is dict and set(streams) == {"stdout", "stderr"},
            f"Win11 EH lacks both raw streams: {row.get('name')}")
    total = 0
    for kind in ("stdout", "stderr"):
        entry = streams[kind]
        require(type(entry) is dict and type(entry.get("bytes")) is int and
                0 <= entry["bytes"] <= 8388608 and
                isinstance(entry.get("path"), str) and bool(entry["path"]) and
                isinstance(entry.get("sha256"), str) and
                re.fullmatch(r"[0-9a-f]{64}", entry["sha256"]) is not None,
                f"Win11 EH raw stream changed: {row.get('name')}/{kind}")
        total += entry["bytes"]
    require(total <= 8388608,
            f"Win11 EH sampled log extent was exceeded: {row.get('name')}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifact-root", type=Path, required=True)
    parser.add_argument("--result", type=Path, required=True)
    parser.add_argument("--raw-log-root", type=Path,
                        help="Required schema-2 mirror of the actual guest stdout/stderr files")
    args = parser.parse_args()
    root = args.artifact_root.resolve(strict=True)
    qualification_path = root / "qualification.json"
    qualification = json.loads(qualification_path.read_text())
    result = json.loads(args.result.read_text())
    source_id = qualification.get("source_id", "")
    schema = qualification.get("schema")
    require(schema in (1, 2) and
            re.fullmatch(r"sha256:[0-9a-f]{64}", source_id) is not None,
            "qualification lacks exact source fingerprint")
    require(re.fullmatch(r"[0-9a-f]{64}", qualification.get("oracle_sha256", "")) is not None,
            "qualification lacks independent oracle SHA-256")
    for key in ("build_sha256", "runner_sha256"):
        require(re.fullmatch(r"[0-9a-f]{64}", qualification.get(key, "")) is not None,
                f"qualification lacks {key}")
    require(digest(root / "run_windows_cross_eh_smoke_vm.ps1") ==
            qualification["runner_sha256"], "guest runner differs from qualification")
    files = qualification.get("files_sha256", {})
    names = {"uwvm.exe", *(stem + ".wasm" for stem in FIXTURES)}
    if schema == 2:
        require(qualification.get("repository") in ("ordinary", "ros") and
                qualification.get("multi_object_required") is True,
                "multi-object qualification lacks repository identity")
        names.add(MULTI_OBJECT_FIXTURE + ".wasm")
    require(set(files) == names, "EH qualification has missing or unexpected artifacts")
    for name in names:
        require(digest(root / name) == files[name], f"qualified artifact changed: {name}")
    require(files["uwvm.exe"] == qualification.get("product_sha256") and
            contains(root / "uwvm.exe", source_id.encode("ascii")),
            "PE hash or embedded source fingerprint differs from qualification")
    require(result.get("passed") is True and
            result.get("source_id") == source_id and
            result.get("product_sha256", "").lower() == files["uwvm.exe"] and
            result.get("oracle_sha256", "").lower() == qualification["oracle_sha256"] and
            result.get("build_sha256", "").lower() == qualification["build_sha256"] and
            result.get("runner_sha256", "").lower() == qualification["runner_sha256"] and
            result.get("qualification_sha256", "").lower() == digest(qualification_path),
            "Win11 result does not match qualified PE, source or oracle")
    require("Windows" in result.get("os", "") and
            result.get("architecture") == "AMD64" and
            bool(result.get("os_version")),
            "EH result is not a real Windows x64 guest run")
    if schema == 2:
        require(result.get("qualification_schema") == 2 and
                result.get("repository") == qualification["repository"] and
                args.raw_log_root is not None,
                "multi-object result requires matching repository and actual raw guest logs")
        raw_root = args.raw_log_root.resolve(strict=True)
        require(raw_root.is_dir(), "raw guest log mirror is not a directory")
    expected = {}
    for policy in ("instruction", "unwind"):
        for stem in FIXTURES:
            expected[f"{stem}-{policy}"] = (True, "")
            expected[f"{stem}-{policy}-exceptions-off"] = (
                False, "--wasm-feature-enable-exceptions")
        expected[f"{FIXTURES[0]}-{policy}-function-references-off"] = (
            False, "function-references")
        if schema == 2:
            for phase in ("uncached", "cache-store", "cache-load"):
                expected[f"{MULTI_OBJECT_FIXTURE}-{policy}-{phase}"] = (True, "")
    rows = result.get("cases", [])
    require(len(rows) == len(expected) == (16 if schema == 2 else 10) and
            {row.get("name") for row in rows} == set(expected),
            "Win11 EH instruction/unwind and feature-off cases are incomplete")
    for row in rows:
        validate_case_boundary(row, files["uwvm.exe"])
        success, diagnostic = expected[row["name"]]
        require(row.get("passed") is True and
                row.get("expected_success") is success and
                row.get("diagnostic") == diagnostic and
                (row.get("exit_code") == 0) is success and
                re.fullmatch(r"[0-9a-f]{64}", row.get("log_sha256", "")) is not None,
                f"Win11 EH guest case failed: {row['name']}")
        if schema == 2:
            raw = []
            for kind in ("stdout", "stderr"):
                path = raw_root / f"{row['name']}.{kind}.log"
                entry = row["raw_streams"][kind]
                require(path.is_file() and path.stat().st_size == entry["bytes"] and
                        digest(path) == entry["sha256"],
                        f"actual guest stream differs: {row['name']}/{kind}")
                raw.append(path.read_text(encoding="utf-8", errors="replace"))
            plain = re.sub(r"\x1b\[[0-9;]*[A-Za-z]", "", "\n".join(raw))
            if row["name"].startswith(MULTI_OBJECT_FIXTURE + "-"):
                counts = [int(x) for x in re.findall(
                    r'object-load-start module="[^"]*" objects=([0-9]+)', plain)]
                count = max(counts, default=0)
                hit = "object-cache-hit module=" in plain
                load = row["name"].endswith("-cache-load")
                require(count >= 2 and row.get("parallel_object_count") == count and
                        row.get("object_cache_hit") is hit and hit is load and
                        row.get("native_eh_materialized") is True and
                        "owning-source=yes pending-plan=native" in plain and
                        "body-fallback=no" in plain and
                        "finalize-object-end module=" in plain,
                        f"native multi-object EH/cache was not executed: {row['name']}")
                argv = row.get("argv", [])
                require(isinstance(argv, list) and argv.count("-Rct") == 1 and
                        argv[argv.index("-Rct") + 1] == "1" and
                        "-WFE-exceptions" in argv and "-WFE-gc" in argv and
                        "-log-vb" in argv,
                        f"multi-object command differs: {row['name']}")
                if not row["name"].endswith("-uncached"):
                    require(row.get("cache_store_present") is True,
                            f"cache bytes were not persisted: {row['name']}")
    print(json.dumps({"passed": True, "source_id": source_id,
                      "product_sha256": files["uwvm.exe"],
                      "qualification_sha256": digest(qualification_path),
                      "result_sha256": digest(args.result),
                      "guest_cases": len(rows)}, sort_keys=True))


if __name__ == "__main__":
    main()
