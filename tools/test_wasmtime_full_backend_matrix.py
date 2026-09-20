#!/usr/bin/env python3
"""Lightweight unit tests for wasmtime_full_backend_matrix.py."""

from __future__ import annotations

import base64
import contextlib
import hashlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Any
from unittest import mock

THIS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(THIS_DIR))

import wasmtime_full_backend_matrix as matrix


WASMTIME_ALIGNMENT_DIAGNOSTIC = (
    "error while executing at wasm backtrace:\n"
    "  1: Pointer not aligned to 4: Region { start: 1, len: 8 }"
)
OBJDUMP_HEADER = "fixture.wasm:\tfile format wasm 0x1\n\nSection Details:\n"
OBJDUMP_START_ENTRY = OBJDUMP_HEADER + "Start:\n - start function: 0\n"
OBJDUMP_EXPORTED_ENTRY = (
    OBJDUMP_HEADER
    + "Type[1]:\n - type[0] () -> nil\n"
    + "Function[1]:\n - func[0] sig=0 <main>\n"
    + 'Export[1]:\n - func[0] <main> -> "main"\n'
)
WAT_FIXTURE = b'(module (func (export "main")))\n'
WASM_FIXTURE = b"\x00asm\x01\x00\x00\x00"


def result(
    outcome: str,
    *,
    stdout: str = "",
    stderr: str = "",
    returncode: int = 1,
    timed_out: bool = False,
) -> matrix.ProcessResult:
    stdout_bytes = stdout.encode("utf-8")
    return matrix.ProcessResult(
        argv=["runtime"],
        returncode=returncode,
        timed_out=timed_out,
        elapsed_ms=0.0,
        stdout=stdout,
        stderr=stderr,
        stdout_bytes_base64=base64.b64encode(stdout_bytes).decode("ascii"),
        stdout_bytes_sha256=hashlib.sha256(stdout_bytes).hexdigest(),
        stdout_bytes_size=len(stdout_bytes),
        outcome=outcome,
    )


class WasiPointerAlignmentTrapTests(unittest.TestCase):
    def test_uwvm_marker_is_canonicalized(self) -> None:
        stderr = (
            "uwvm: [fatal] WASI Preview 1 guest pointer alignment trap: "
            "fd_write.iovs (ciovec), guest offset = 1, required alignment = 4 bytes"
        )
        self.assertEqual(matrix.classify(-4, False, "", stderr), "trap:wasi-pointer-alignment")

    def test_uwvm_marker_requires_fatal_prefix(self) -> None:
        stderr = (
            "WASI Preview 1 guest pointer alignment trap: "
            "fd_write.iovs (ciovec), guest offset = 1, required alignment = 4 bytes"
        )
        self.assertEqual(matrix.classify(1, False, "", stderr), "exit:1")

    def test_wasmtime_region_diagnostic_is_canonicalized(self) -> None:
        stderr = "error while executing at wasm backtrace:\n  1: Pointer not aligned to 4: Region { start: 1, len: 8 }"
        self.assertEqual(matrix.classify(1, False, "", stderr), "trap:wasi-pointer-alignment")

    def test_cross_runtime_alignment_traps_compare_equal(self) -> None:
        uwvm = result(
            "trap:wasi-pointer-alignment",
            stderr="WASI Preview 1 guest pointer alignment trap",
            returncode=-4,
        )
        wasmtime = result(
            "trap:wasi-pointer-alignment",
            stderr="Pointer not aligned to 4: Region { start: 1, len: 8 }",
        )
        self.assertTrue(matrix.same_result(uwvm, wasmtime))

    def test_alignment_and_unrelated_traps_do_not_compare_equal(self) -> None:
        alignment = result("trap:wasi-pointer-alignment")
        memory_oob = result("trap:memory-oob")
        self.assertFalse(matrix.same_result(alignment, memory_oob))

    def test_generic_alignment_wording_is_not_canonicalized(self) -> None:
        self.assertEqual(matrix.classify(1, False, "", "return pointer not aligned"), "exit:1")
        self.assertEqual(matrix.classify(1, False, "", "pointer not aligned"), "exit:1")

    def test_wasmtime_region_requires_backtrace_context(self) -> None:
        stderr = "Pointer not aligned to 4: Region { start: 1, len: 8 }"
        self.assertEqual(matrix.classify(1, False, "", stderr), "exit:1")

    def test_non_abi_alignment_and_wrong_region_shape_are_not_canonicalized(self) -> None:
        context = "error while executing at wasm backtrace:\n"
        self.assertEqual(
            matrix.classify(1, False, "", context + "Pointer not aligned to 16: Region { start: 1, len: 8 }"),
            "exit:1",
        )
        self.assertEqual(
            matrix.classify(1, False, "", context + "Pointer not aligned to 4: Region { len: 8, start: 1 }"),
            "exit:1",
        )

    def test_alignment_text_on_stdout_is_not_canonicalized(self) -> None:
        self.assertEqual(matrix.classify(1, False, WASMTIME_ALIGNMENT_DIAGNOSTIC, ""), "exit:1")

    def test_success_and_timeout_take_precedence_over_diagnostics(self) -> None:
        self.assertEqual(matrix.classify(0, False, "", WASMTIME_ALIGNMENT_DIAGNOSTIC), "ok")
        self.assertEqual(matrix.classify(124, True, "", WASMTIME_ALIGNMENT_DIAGNOSTIC), "timeout")

    def test_core_wasm_unaligned_access_is_not_canonicalized(self) -> None:
        self.assertEqual(matrix.classify(1, False, "", "unaligned i32.load at address 1"), "exit:1")


class RawStdoutComparisonTests(unittest.TestCase):
    def test_all_matching_outcomes_still_require_matching_stdout_bytes(self) -> None:
        for outcome in ("ok", "exit:7", "trap:unreachable", "timeout"):
            with self.subTest(outcome=outcome):
                self.assertFalse(matrix.same_result(result(outcome, stdout="left"), result(outcome, stdout="right")))
                self.assertTrue(matrix.same_result(result(outcome, stdout="same"), result(outcome, stdout="same")))

    def test_invalid_utf8_is_preserved_and_compared_as_raw_bytes(self) -> None:
        def capture(raw: bytes) -> matrix.ProcessResult:
            completed = matrix.subprocess.CompletedProcess(["runtime"], 1, stdout=raw, stderr=b"")
            with mock.patch.object(matrix.subprocess, "run", return_value=completed):
                return matrix.run_process(["runtime"], Path.cwd(), 1.0)

        first = capture(b"\xff")
        second = capture(b"\xfe")
        self.assertEqual(first.stdout, r"\xff")
        self.assertEqual(first.stdout_bytes_base64, "/w==")
        self.assertEqual(first.stdout_bytes_sha256, hashlib.sha256(b"\xff").hexdigest())
        self.assertEqual(first.stdout_bytes_size, 1)
        self.assertNotIn("\ufffd", first.stdout)
        self.assertFalse(matrix.same_result(first, second))
        json.dumps(matrix.asdict(first), allow_nan=False)


class BinaryEntryStateTests(unittest.TestCase):
    def test_start_and_exported_void_entry_are_callable(self) -> None:
        self.assertEqual(matrix.binary_entry_state(OBJDUMP_START_ENTRY), "callable")
        self.assertEqual(matrix.binary_entry_state(OBJDUMP_EXPORTED_ENTRY), "callable")

    def test_parseable_module_without_entry_is_confirmed(self) -> None:
        self.assertEqual(matrix.binary_entry_state(OBJDUMP_HEADER), "confirmed-no-entry")

    def test_empty_or_partial_output_is_unparseable(self) -> None:
        self.assertEqual(matrix.binary_entry_state(""), "unparseable")
        self.assertEqual(matrix.binary_entry_state("Start:\n"), "unparseable")
        self.assertEqual(matrix.binary_entry_state("Section Details:\n"), "unparseable")


class ProvenanceProbeTests(unittest.TestCase):
    def test_successful_version_probe_records_text(self) -> None:
        probe = matrix.probe_command([sys.executable, "--version"], Path.cwd(), 2.0)
        self.assertEqual(probe["status"], "ok")
        self.assertIn("Python", probe["text"])

    def test_missing_probe_is_structured_and_non_fatal(self) -> None:
        probe = matrix.probe_command(["/definitely/missing/uwvm-matrix-tool"], Path.cwd(), 2.0)
        self.assertEqual(probe["status"], "unavailable")
        self.assertEqual(probe["result"]["outcome"], "spawn-error")
        self.assertIsNotNone(probe["error"])

    def test_collected_provenance_is_strict_json(self) -> None:
        executable = Path(sys.executable)
        provenance = matrix.collect_provenance(
            Path.cwd(),
            executable,
            executable,
            executable,
            executable,
            executable,
            executable,
            2.0,
        )
        encoded = json.dumps(provenance, allow_nan=False)
        self.assertIn('"corpus"', encoded)
        self.assertIn('"tools"', encoded)
        self.assertEqual(provenance["product_profile"], matrix.PRODUCT_PROFILE)
        self.assertEqual(provenance["driver"]["sha256"], matrix.file_fingerprint(Path(matrix.__file__))["sha256"])
        expected_executable_hash = matrix.file_fingerprint(executable.resolve())["sha256"]
        self.assertEqual(
            set(provenance["tools"]),
            {"uwvm_int", "uwvm_llvm", "wasmtime", "wat2wasm", "wasm_validate", "wasm_objdump"},
        )
        for tool in provenance["tools"].values():
            self.assertEqual(tool["sha256"], expected_executable_hash)

    def test_corpus_revision_is_extracted_from_git_probe(self) -> None:
        revision = "0123456789abcdef0123456789abcdef01234567"
        revision_probe = {
            "status": "ok",
            "text": revision,
            "error": None,
            "result": {"stdout": revision + "\n"},
        }
        status_probe = {
            "status": "ok",
            "text": None,
            "error": None,
            "result": {"stdout": ""},
        }
        with mock.patch.object(matrix, "probe_command", side_effect=[revision_probe, status_probe]):
            provenance = matrix.corpus_git_provenance(Path("/corpus"), 2.0)
        self.assertEqual(provenance["revision"], revision)
        self.assertFalse(provenance["dirty"])
        self.assertFalse(provenance["tracked_changes"])
        self.assertFalse(provenance["untracked_changes"])
        self.assertFalse(provenance["submodule_changes"])
        self.assertEqual(provenance["status"], [])

    def test_dirty_corpus_status_is_not_hidden_by_clean_head_revision(self) -> None:
        revision = "0123456789abcdef0123456789abcdef01234567"
        revision_probe = {
            "status": "ok",
            "text": revision,
            "error": None,
            "result": {"stdout": revision + "\n"},
        }
        status_probe = {
            "status": "ok",
            "text": "1 .M N... 100644 100644 100644 abc def all/example.wat\n? generated.wat",
            "error": None,
            "result": {"stdout": "1 .M N... 100644 100644 100644 abc def all/example.wat\n? generated.wat\n"},
        }
        with mock.patch.object(matrix, "probe_command", side_effect=[revision_probe, status_probe]):
            provenance = matrix.corpus_git_provenance(Path("/corpus"), 2.0)
        self.assertTrue(provenance["dirty"])
        self.assertTrue(provenance["tracked_changes"])
        self.assertTrue(provenance["untracked_changes"])
        self.assertFalse(provenance["submodule_changes"])
        self.assertEqual(
            provenance["status"],
            ["1 .M N... 100644 100644 100644 abc def all/example.wat", "? generated.wat"],
        )

    def test_submodule_status_is_reported_separately(self) -> None:
        revision = "0123456789abcdef0123456789abcdef01234567"
        revision_probe = {
            "status": "ok",
            "text": revision,
            "error": None,
            "result": {"stdout": revision + "\n"},
        }
        status_probe = {
            "status": "ok",
            "text": "1 .M S.M. 160000 160000 160000 abc def nested-corpus",
            "error": None,
            "result": {"stdout": "1 .M S.M. 160000 160000 160000 abc def nested-corpus\n"},
        }
        with mock.patch.object(matrix, "probe_command", side_effect=[revision_probe, status_probe]):
            provenance = matrix.corpus_git_provenance(Path("/corpus"), 2.0)
        self.assertTrue(provenance["dirty"])
        self.assertTrue(provenance["tracked_changes"])
        self.assertFalse(provenance["untracked_changes"])
        self.assertTrue(provenance["submodule_changes"])

    def test_non_object_git_output_is_reported_without_inventing_a_revision(self) -> None:
        revision_probe = {
            "status": "ok",
            "text": "not-a-git-object",
            "error": None,
            "result": {"stdout": "not-a-git-object\n"},
        }
        status_probe = {
            "status": "failed",
            "text": None,
            "error": None,
            "result": {"stdout": ""},
        }
        with mock.patch.object(matrix, "probe_command", side_effect=[revision_probe, status_probe]):
            provenance = matrix.corpus_git_provenance(Path("/corpus"), 2.0)
        self.assertIsNone(provenance["revision"])
        self.assertIsNone(provenance["dirty"])
        self.assertIsNone(provenance["tracked_changes"])
        self.assertIsNone(provenance["untracked_changes"])
        self.assertIsNone(provenance["submodule_changes"])
        self.assertEqual(provenance["revision_probe"]["status"], "failed")
        self.assertIn("non-object-id", provenance["revision_probe"]["error"])


class FileFingerprintTests(unittest.TestCase):
    def test_chunked_fingerprint_records_path_size_and_sha256(self) -> None:
        payload = b"chunked-file-fingerprint"
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "payload.bin"
            path.write_bytes(payload)
            with mock.patch.object(matrix, "HASH_CHUNK_SIZE", 3):
                fingerprint = matrix.file_fingerprint(path)
        self.assertEqual(fingerprint["path"], str(path))
        self.assertEqual(fingerprint["size"], len(payload))
        self.assertEqual(fingerprint["sha256"], hashlib.sha256(payload).hexdigest())


class ReleaseGateSummaryTests(unittest.TestCase):
    @staticmethod
    def case(status: str, classification: str, **extra: object) -> dict[str, object]:
        return {"relative_path": "fixture.wat", "status": status, "classification": classification, **extra}

    def test_every_skip_class_is_unexpected_without_an_allowlist(self) -> None:
        cases = [self.case(status, "skipped", skip_allowlist_eligible=True) for status in matrix.SKIP_STATUS_COUNTERS]
        gate = matrix.release_gate_summary(
            cases,
            strict=True,
            minimum_executed=1,
            allowed_skip_statuses=(),
        )
        self.assertFalse(gate["qualified"])
        self.assertEqual(gate["executed"]["total"], 0)
        self.assertEqual(gate["skipped"]["total"], len(matrix.SKIP_STATUS_COUNTERS))
        self.assertEqual(gate["skipped"]["unexpected_total"], len(matrix.SKIP_STATUS_COUNTERS))
        self.assertEqual(
            [violation["kind"] for violation in gate["violations"]],
            ["minimum-executed-not-met", "unexpected-skips"],
        )

    def test_allowlisted_selection_skip_and_executed_case_qualify(self) -> None:
        cases = [
            self.case("passed", "executed"),
            self.case(
                "preceding-profile-skipped",
                "skipped",
                skip_detail="valid-under-preceding-profile",
                skip_allowlist_eligible=True,
            ),
        ]
        gate = matrix.release_gate_summary(
            cases,
            strict=True,
            minimum_executed=1,
            allowed_skip_statuses=("preceding-profile-skipped",),
        )
        self.assertTrue(gate["qualified"])
        self.assertEqual(gate["executed"]["total"], 1)
        self.assertEqual(gate["skipped"]["expected_total"], 1)
        self.assertEqual(gate["skipped"]["unexpected_total"], 0)
        self.assertTrue(cases[1]["skip_expected"])

    def test_allowlist_does_not_hide_an_operational_failure(self) -> None:
        cases = [
            self.case("passed", "executed"),
            self.case(
                "no-binary-entry-skipped",
                "skipped",
                skip_detail="wasm-objdump-timeout",
                skip_allowlist_eligible=False,
            ),
        ]
        gate = matrix.release_gate_summary(
            cases,
            strict=True,
            minimum_executed=1,
            allowed_skip_statuses=("no-binary-entry-skipped",),
        )
        self.assertFalse(gate["qualified"])
        self.assertEqual(gate["skipped"]["unexpected_total"], 1)
        self.assertFalse(cases[1]["skip_expected"])

    def test_strict_gate_never_waives_reference_mismatch(self) -> None:
        cases = [self.case("reference-mismatch", "executed")]
        strict = matrix.release_gate_summary(
            cases,
            strict=True,
            minimum_executed=1,
            allowed_skip_statuses=(),
            allow_reference_mismatch=True,
        )
        compatibility = matrix.release_gate_summary(
            cases,
            strict=False,
            minimum_executed=1,
            allowed_skip_statuses=(),
            allow_reference_mismatch=True,
        )
        self.assertFalse(strict["qualified"])
        self.assertFalse(strict["allow_reference_mismatch_effective"])
        self.assertTrue(compatibility["qualified"])
        self.assertTrue(compatibility["allow_reference_mismatch_effective"])


class ReleaseGateMainTests(unittest.TestCase):
    def invoke(
        self,
        process_results: list[matrix.ProcessResult],
        *extra_arguments: str,
        fixture_count: int = 1,
        materialize_wasm: bool = True,
        stale_wasm: bytes | None = None,
    ) -> tuple[int, dict[str, Any], str]:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "corpus"
            source.mkdir()
            for index in range(fixture_count):
                (source / f"fixture-{index}.wat").write_bytes(WAT_FIXTURE)
            work = root / "work"
            if stale_wasm is not None:
                stale_dir = work / "artifacts" / matrix.artifact_stem("fixture-0.wat")
                stale_dir.mkdir(parents=True)
                (stale_dir / "input.wasm").write_bytes(stale_wasm)
            arguments = [
                "--source-root",
                str(source),
                "--uwvm",
                sys.executable,
                "--wasmtime",
                sys.executable,
                "--wat2wasm",
                sys.executable,
                "--wasm-validate",
                sys.executable,
                "--wasm-objdump",
                sys.executable,
                "--work-dir",
                str(work),
                *extra_arguments,
            ]
            stdout = io.StringIO()
            stderr = io.StringIO()
            process_results_iterator = iter(process_results)

            def run_mocked_process(command: list[str], cwd: Path, timeout: float) -> matrix.ProcessResult:
                del cwd, timeout
                process_result = next(process_results_iterator)
                if materialize_wasm and process_result.outcome == "ok" and "-o" in command:
                    output = Path(command[command.index("-o") + 1])
                    output.write_bytes(WASM_FIXTURE)
                process_record = matrix.asdict(process_result)
                process_record["argv"] = list(command)
                return matrix.ProcessResult(**process_record)

            with (
                mock.patch.object(matrix, "collect_provenance", return_value={}),
                mock.patch.object(matrix, "run_process", side_effect=run_mocked_process) as run_mock,
                contextlib.redirect_stdout(stdout),
                contextlib.redirect_stderr(stderr),
            ):
                returncode = matrix.main(arguments)
            self.process_argvs = [list(call.args[0]) for call in run_mock.call_args_list]
            final_wasm = work / "artifacts" / matrix.artifact_stem("fixture-0.wat") / "input.wasm"
            self.final_wasm = final_wasm.read_bytes() if final_wasm.is_file() else None
            report = json.loads((work / "report.json").read_text(encoding="utf-8"))
            return returncode, report, stderr.getvalue()

    def parser_error(self, *extra_arguments: str) -> str:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "corpus"
            source.mkdir()
            (source / "fixture.wat").write_text('(module (func (export "main")))\n', encoding="utf-8")
            arguments = [
                "--source-root",
                str(source),
                "--uwvm",
                sys.executable,
                "--wasmtime",
                sys.executable,
                "--wat2wasm",
                sys.executable,
                "--wasm-validate",
                sys.executable,
                "--wasm-objdump",
                sys.executable,
                "--work-dir",
                str(root / "work"),
                *extra_arguments,
            ]
            stderr = io.StringIO()
            with contextlib.redirect_stderr(stderr), self.assertRaises(SystemExit) as raised:
                matrix.main(arguments)
            self.assertEqual(raised.exception.code, 2)
            return stderr.getvalue()

    def test_feature_override_rejects_runtime_argument_injection(self) -> None:
        for injected in ("--mode", "--run", "-Rint", "-Raot"):
            with self.subTest(injected=injected):
                stderr = self.parser_error(f"--uwvm-feature-arg={injected}")
                self.assertIn("invalid choice", stderr)

    def test_feature_override_accepts_only_one_known_switch(self) -> None:
        stderr = self.parser_error(
            "--uwvm-feature-arg=--wasm-feature-mvp",
            "--uwvm-feature-arg=--wasm-feature-wasm2",
        )
        self.assertIn("at most once", stderr)

    def test_strict_requires_explicit_reviewed_floor(self) -> None:
        stderr = self.parser_error("--strict")
        self.assertIn("requires an explicit --minimum-executed", stderr)

    def test_strict_rejects_reference_mismatch_waiver(self) -> None:
        stderr = self.parser_error(
            "--strict",
            "--minimum-executed",
            "1",
            "--allow-reference-mismatch",
        )
        self.assertIn("mutually exclusive", stderr)

    def test_compatibility_mode_reports_but_does_not_enforce_skip_gate(self) -> None:
        returncode, report, stderr = self.invoke([result("exit:1")])
        self.assertEqual(returncode, 0)
        self.assertEqual(report["schema"], "uwvm2-wasmtime-full-backend-matrix-v6")
        self.assertFalse(report["release_gate"]["qualified"])
        self.assertEqual(report["counters"]["executed"], 0)
        self.assertEqual(report["counters"]["unexpected_skipped"], 1)
        self.assertIn("did not enforce", stderr)

    def test_all_profile_rejection_cannot_be_allowlisted(self) -> None:
        ok = result("ok", returncode=0)
        returncode, report, _ = self.invoke(
            [ok, result("exit:1")],
            "--strict",
            "--minimum-executed",
            "1",
            "--feature-profile",
            "all",
            "--allow-skip",
            "profile-validation-skipped",
        )
        self.assertEqual(returncode, 1)
        self.assertEqual(report["cases"][0]["skip_detail"], "all-profile-validation-rejected")
        self.assertFalse(report["cases"][0]["skip_expected"])
        self.assertEqual(report["counters"]["unexpected_skipped"], 1)

    def test_empty_objdump_output_is_unparseable_and_not_allowlisted(self) -> None:
        ok = result("ok", returncode=0)
        returncode, report, _ = self.invoke(
            [ok, ok, ok],
            "--strict",
            "--minimum-executed",
            "1",
            "--allow-skip",
            "no-binary-entry-skipped",
        )
        self.assertEqual(returncode, 1)
        self.assertEqual(report["cases"][0]["binary_entry_state"], "unparseable")
        self.assertEqual(report["cases"][0]["skip_detail"], "wasm-objdump-unparseable")
        self.assertFalse(report["cases"][0]["skip_expected"])

    def test_confirmed_no_entry_is_allowlistable_but_does_not_count_as_executed(self) -> None:
        ok = result("ok", returncode=0)
        objdump = result("ok", stdout=OBJDUMP_HEADER, returncode=0)
        returncode, report, _ = self.invoke(
            [ok, ok, objdump],
            "--strict",
            "--minimum-executed",
            "1",
            "--allow-skip",
            "no-binary-entry-skipped",
        )
        self.assertEqual(returncode, 1)
        self.assertEqual(report["cases"][0]["binary_entry_state"], "confirmed-no-entry")
        self.assertTrue(report["cases"][0]["skip_expected"])
        self.assertEqual(report["counters"]["expected_skipped"], 1)
        self.assertEqual(report["counters"]["executed"], 0)

    def test_strict_mode_fails_each_previous_false_pass_skip(self) -> None:
        ok = result("ok", returncode=0)
        scenarios = (
            ("wat-compile-skipped", [result("exit:1")], ()),
            ("profile-validation-skipped", [ok, result("exit:1")], ()),
            (
                "preceding-profile-skipped",
                [ok, ok, ok],
                ("--feature-profile", "wasm2-core", "--only-profile-delta"),
            ),
            ("no-binary-entry-skipped", [ok, ok, ok], ()),
        )
        for expected_status, process_results, arguments in scenarios:
            with self.subTest(status=expected_status):
                returncode, report, _ = self.invoke(
                    process_results,
                    "--strict",
                    "--minimum-executed",
                    "1",
                    *arguments,
                )
                self.assertEqual(returncode, 1)
                self.assertEqual(report["cases"][0]["status"], expected_status)
                self.assertEqual(report["cases"][0]["classification"], "skipped")
                self.assertFalse(report["release_gate"]["qualified"])

    def test_allowlisted_skip_still_cannot_satisfy_minimum_execution(self) -> None:
        ok = result("ok", returncode=0)
        returncode, report, _ = self.invoke(
            [ok, result("exit:1")],
            "--strict",
            "--minimum-executed",
            "1",
            "--allow-skip",
            "profile-validation-skipped",
        )
        self.assertEqual(returncode, 1)
        self.assertEqual(report["counters"]["expected_skipped"], 1)
        self.assertEqual(report["counters"]["unexpected_skipped"], 0)
        self.assertEqual(
            report["release_gate"]["violations"],
            [{"actual": 0, "kind": "minimum-executed-not-met", "minimum": 1}],
        )

    def test_allowlist_does_not_hide_preceding_validator_timeout(self) -> None:
        ok = result("ok", returncode=0)
        timed_out = result("timeout", returncode=124, timed_out=True)
        returncode, report, _ = self.invoke(
            [ok, ok, timed_out],
            "--strict",
            "--minimum-executed",
            "1",
            "--feature-profile",
            "wasm2-core",
            "--only-profile-delta",
            "--allow-skip",
            "preceding-profile-skipped",
        )
        self.assertEqual(returncode, 1)
        self.assertEqual(report["cases"][0]["skip_detail"], "preceding-validator-timeout")
        self.assertEqual(report["counters"]["expected_skipped"], 0)
        self.assertEqual(report["counters"]["unexpected_skipped"], 1)

    def test_strict_success_executes_both_backends_and_reference(self) -> None:
        ok = result("ok", returncode=0)
        objdump = result("ok", stdout=OBJDUMP_START_ENTRY, returncode=0)
        returncode, report, _ = self.invoke(
            [ok, ok, objdump, ok, ok, ok],
            "--strict",
            "--minimum-executed",
            "1",
        )
        self.assertEqual(returncode, 0)
        self.assertTrue(report["release_gate"]["qualified"])
        self.assertEqual(report["product_profile"], matrix.PRODUCT_PROFILE)
        self.assertEqual(report["counters"]["executed"], 1)
        self.assertEqual(report["counters"]["skipped"], 0)
        case = report["cases"][0]
        self.assertEqual(case["wat"]["sha256"], hashlib.sha256(WAT_FIXTURE).hexdigest())
        self.assertEqual(case["wat"]["size"], len(WAT_FIXTURE))
        self.assertEqual(case["wasm"]["sha256"], hashlib.sha256(WASM_FIXTURE).hexdigest())
        self.assertEqual(case["wasm"]["size"], len(WASM_FIXTURE))
        self.assertEqual(self.final_wasm, WASM_FIXTURE)
        self.assertNotEqual(case["wat2wasm"]["argv"][-1], case["wasm_path"])
        self.assertIn(".wat2wasm-", case["wat2wasm"]["argv"][-1])
        self.assertEqual(self.process_argvs[-2][1:5], ["-Rcm", "full", "-Rcc", "int"])

    def test_wat2wasm_ok_without_output_is_an_unexpected_compile_skip(self) -> None:
        returncode, report, _ = self.invoke(
            [result("ok", returncode=0)],
            "--strict",
            "--minimum-executed",
            "1",
            materialize_wasm=False,
        )
        self.assertEqual(returncode, 1)
        case = report["cases"][0]
        self.assertEqual(case["status"], "wat-compile-skipped")
        self.assertEqual(case["skip_detail"], "wat2wasm-ok-without-output")
        self.assertFalse(case["skip_allowlist_eligible"])
        self.assertNotIn("wasm", case)
        self.assertIsNone(self.final_wasm)
        self.assertEqual(len(self.process_argvs), 1)

    def test_stale_wasm_is_removed_and_never_reused_after_false_success(self) -> None:
        returncode, report, _ = self.invoke(
            [result("ok", returncode=0)],
            "--strict",
            "--minimum-executed",
            "1",
            materialize_wasm=False,
            stale_wasm=b"stale-output",
        )
        self.assertEqual(returncode, 1)
        self.assertEqual(report["cases"][0]["skip_detail"], "wat2wasm-ok-without-output")
        self.assertIsNone(self.final_wasm)
        self.assertEqual(len(self.process_argvs), 1)

    def test_matching_outcome_with_different_stdout_is_a_backend_mismatch(self) -> None:
        ok = result("ok", returncode=0)
        objdump = result("ok", stdout=OBJDUMP_START_ENTRY, returncode=0)
        returncode, report, _ = self.invoke(
            [
                ok,
                ok,
                objdump,
                result("ok", stdout="same", returncode=0),
                result("ok", stdout="same", returncode=0),
                result("ok", stdout="different", returncode=0),
            ],
            "--strict",
            "--minimum-executed",
            "1",
        )
        self.assertEqual(returncode, 1)
        self.assertEqual(report["cases"][0]["status"], "backend-mismatch")

    def test_validation_mode_also_compares_success_stdout_bytes(self) -> None:
        ok = result("ok", returncode=0)
        returncode, report, _ = self.invoke(
            [
                ok,
                ok,
                result("ok", stdout="left", returncode=0),
                result("ok", stdout="right", returncode=0),
            ],
            "--strict",
            "--minimum-executed",
            "1",
            "--mode",
            "validate",
        )
        self.assertEqual(returncode, 1)
        self.assertEqual(report["cases"][0]["status"], "backend-mismatch")

    def test_selection_contract_records_filters_and_limit_counts(self) -> None:
        ok = result("ok", returncode=0)
        objdump = result("ok", stdout=OBJDUMP_START_ENTRY, returncode=0)
        returncode, report, _ = self.invoke(
            [ok, ok, objdump, ok, ok, ok],
            "--strict",
            "--minimum-executed",
            "1",
            "--expected-selected",
            "1",
            "--include",
            "fixture-",
            "--exclude",
            "never-match",
            "--max-cases",
            "1",
            fixture_count=2,
        )
        self.assertEqual(returncode, 0)
        selection = report["selection"]
        self.assertEqual(selection["include_patterns"], ["fixture-"])
        self.assertEqual(selection["exclude_patterns"], ["never-match"])
        self.assertEqual(selection["max_cases"], 1)
        self.assertEqual(selection["selected_before_limit"], 2)
        self.assertEqual(selection["selected_after_limit"], 1)
        self.assertEqual(selection["limit_truncated"], 1)
        self.assertTrue(report["release_gate"]["expected_selected_met"])

    def test_expected_selected_mismatch_fails_strict_gate(self) -> None:
        ok = result("ok", returncode=0)
        objdump = result("ok", stdout=OBJDUMP_START_ENTRY, returncode=0)
        returncode, report, _ = self.invoke(
            [ok, ok, objdump, ok, ok, ok],
            "--strict",
            "--minimum-executed",
            "1",
            "--expected-selected",
            "2",
        )
        self.assertEqual(returncode, 1)
        self.assertFalse(report["release_gate"]["expected_selected_met"])
        self.assertIn("expected-selected-mismatch", [item["kind"] for item in report["release_gate"]["violations"]])

    def test_strict_mode_rejects_equal_engine_timeouts(self) -> None:
        ok = result("ok", returncode=0)
        objdump = result("ok", stdout=OBJDUMP_START_ENTRY, returncode=0)
        timed_out = result("timeout", returncode=124, timed_out=True)
        returncode, report, _ = self.invoke(
            [ok, ok, objdump, timed_out, timed_out, timed_out],
            "--strict",
            "--minimum-executed",
            "1",
        )
        self.assertEqual(returncode, 1)
        self.assertEqual(report["cases"][0]["status"], "operational-failure")
        self.assertEqual(report["counters"]["operational_failure"], 1)
        self.assertEqual(
            report["release_gate"]["violations"],
            [
                {
                    "by_status": {"operational-failure": 1},
                    "count": 1,
                    "kind": "result-failures",
                }
            ],
        )

    def test_strict_mode_enforces_configured_minimum_execution(self) -> None:
        ok = result("ok", returncode=0)
        objdump = result("ok", stdout=OBJDUMP_START_ENTRY, returncode=0)
        returncode, report, _ = self.invoke(
            [ok, ok, objdump, ok, ok, ok],
            "--strict",
            "--minimum-executed",
            "2",
        )
        self.assertEqual(returncode, 1)
        self.assertEqual(report["release_gate"]["executed"]["total"], 1)
        self.assertFalse(report["release_gate"]["minimum_executed_met"])


if __name__ == "__main__":
    unittest.main()
