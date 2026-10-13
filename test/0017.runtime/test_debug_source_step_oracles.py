#!/usr/bin/env python3
"""Synthetic source-step oracle units; no compiler or product qualification.

Remote keeper cgroup only. Real tool-produced fixture/console tests remain in
run_debug_source_step_cli.py. Importing that file starts no product process.
"""
from pathlib import Path
import subprocess
import tempfile
import unittest
import run_debug_source_step_cli as runner


def section(kind: int, payload: bytes) -> bytes:
    return bytes([kind]) + runner.metadata_cli.leb(len(payload)) + payload


def module(imports: list[bytes]) -> bytes:
    # One actual no-parameter/no-result local function with one end opcode.
    # The imported-function counterexample additionally contains a real import;
    # its local function then has MODULE index one, not Code-local index zero.
    return (b'\0asm\x01\0\0\0' + section(1, b'\x01\x60\x00\x00') +
            b''.join(section(2, value) for value in imports) +
            section(3, b'\x01\x00') + section(10, b'\x01\x02\x00\x0b'))


class ImportIndexPolicy(unittest.TestCase):
    def decode(self, data: bytes):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'component.wasm'
            path.write_bytes(data)
            return runner.code_expressions(path)

    def test_actual_no_imports(self):
        self.assertEqual(self.decode(module([])), {0: 3})

    def test_actual_empty_import_vector(self):
        self.assertEqual(self.decode(module([b'\x00'])), {0: 3})

    def test_real_imported_function_refused_before_local_index_use(self):
        # import count1, module m, name f, function kind0, type index0.
        with self.assertRaisesRegex(AssertionError, 'rejects any actual import'):
            self.decode(module([b'\x01\x01m\x01f\x00\x00']))

    def test_real_memory_import_also_focused_policy_refuses(self):
        # import count1, module m, name f, memory kind2, min-only limits1.
        with self.assertRaisesRegex(AssertionError, 'rejects any actual import'):
            self.decode(module([b'\x01\x01m\x01f\x02\x00\x01']))

    def test_duplicate_empty_import_sections_refused(self):
        with self.assertRaisesRegex(AssertionError, 'duplicate actual Import'):
            self.decode(module([b'\x00', b'\x00']))

    def test_empty_import_trailing_bytes_refused(self):
        with self.assertRaisesRegex(AssertionError, 'complete empty Import vector'):
            self.decode(module([b'\x00\x00']))

    def test_truncated_import_count_refused(self):
        with self.assertRaisesRegex(ValueError, 'truncated fixture u32'):
            self.decode(module([b'\x80']))


class ActualRowDumpFormats(unittest.TestCase):
    # These numeric schemas follow LLVM14 and LLVM18/23 Row::dump, rather
    # than pretending these synthetic rows came from a compiled source module.
    def test_llvm14_without_op_index(self):
        rows = runner.line_sequences('0x0000000000000003 7 4 1 0 2 is_stmt\n'
                                     '0x0000000000000005 7 4 1 0 0 end_sequence\n')
        at = runner.line_at(rows, 4)
        self.assertEqual((at['line'], at['column'], at['discriminator'], at['op_index']), (7, 4, 2, 0))
        with self.assertRaisesRegex(AssertionError, 'one official line row'):
            runner.line_at(rows, 5)

    def test_llvm18_23_op_index_and_duplicate_row(self):
        rows = runner.line_sequences('0x0000000000000003 7 4 1 0 2 0 is_stmt\n'
                                     '0x0000000000000003 8 5 1 0 9 0 is_stmt\n'
                                     '0x0000000000000005 8 5 1 0 0 0 end_sequence\n')
        at = runner.line_at(rows, 3)
        self.assertEqual((at['line'], at['column'], at['discriminator']), (8, 5, 9))

    def test_nonzero_op_index_refused(self):
        with self.assertRaisesRegex(AssertionError, 'VLIW operation index'):
            runner.line_sequences('0x0000000000000003 7 4 1 0 2 1 is_stmt\n')

    def test_unterminated_sequence_refused(self):
        with self.assertRaisesRegex(AssertionError, 'complete official line sequences'):
            runner.line_sequences('0x0000000000000003 7 4 1 0 2 0 is_stmt\n')

    def test_decreasing_sequence_refused(self):
        with self.assertRaisesRegex(AssertionError, 'addresses decrease'):
            runner.line_sequences('0x0000000000000004 7 4 1 0 2 0 is_stmt\n'
                                  '0x0000000000000003 7 4 1 0 2 0 end_sequence\n')


if __name__ == '__main__':
    guard = Path(__file__).resolve().parents[2] / 'tools/ci/require_wasm3_test_cgroup.sh'
    subprocess.run(['bash', str(guard)], check=True)
    unittest.main()
