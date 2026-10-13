#!/usr/bin/env python3
"""Pure staging counterexamples; never runs a compiler, product or Win32 API.

Run only by the remote keeper. These synthetic records verify refusal rules,
and are not original build receipts or official compiler/Windows qualification.
"""
from pathlib import Path
import tempfile
import unittest
import stage_windows_debug_current_vm as stage


class StageCounterexamples(unittest.TestCase):
    def test_regular_and_delayed_imports_both_retained(self):
        text = 'Import {\n  Name: KERNEL32.dll\n}\nDelayImport {\n  Name: LLVM23.dll\n}\n'
        self.assertEqual(stage.imports(text), ['KERNEL32.dll', 'LLVM23.dll'])

    def test_unknown_dll_is_never_assumed_system(self):
        self.assertFalse(stage.is_system_dll('LLVM23.dll'))
        self.assertFalse(stage.is_system_dll('libunwind.dll'))
        self.assertFalse(stage.is_system_dll('libwinpthread-1.dll'))
        self.assertTrue(stage.is_system_dll('api-ms-win-core-file-l1-1-0.dll'))

    def test_import_path_refused(self):
        with self.assertRaises(ValueError):
            stage.imports('Import {\n  Name: ..\\LLVM23.dll\n}\n')

    def test_file_arguments_exclude_output_but_keep_consumed_object(self):
        with tempfile.TemporaryDirectory() as directory:
            cwd = Path(directory)
            obj, output = cwd / 'runtime.o', cwd / 'uwvm.exe'
            obj.write_bytes(b'synthetic object data'); output.write_bytes(b'synthetic output data')
            self.assertEqual(stage.file_arguments(['clang++', str(obj), '-o', str(output)], cwd), {obj.resolve()})

    def command_record(self, directory):
        cwd = Path(directory)
        tool, log, output = cwd / 'clang++', cwd / 'raw.log', cwd / 'result.o'
        for path in (tool, log, output):
            path.write_bytes(b'synthetic nonexecuted data')
        return {'argv': [str(tool), '-c', 'source.cc', '-o', str(output)], 'cwd': str(cwd), 'returncode': 0,
                'tool_sha256': stage.sha(tool), 'log': str(log), 'log_sha256': stage.sha(log),
                'output': str(output), 'output_sha256': stage.sha(output)}

    def test_direct_record_is_checked_as_nonexecuted_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            row = self.command_record(directory)
            output, argv, cwd = stage.verify_command(row)
            self.assertEqual(output, Path(row['output']).resolve())
            self.assertEqual(argv, row['argv']); self.assertEqual(cwd, Path(directory).resolve())

    def test_env_prefix_does_not_turn_env_into_compiler_provenance(self):
        with tempfile.TemporaryDirectory() as directory:
            row = self.command_record(directory)
            wrapper = Path(directory) / 'env'; wrapper.write_bytes(b'synthetic nonexecuted wrapper')
            row['argv'].insert(0, str(wrapper)); row['tool_sha256'] = stage.sha(wrapper)
            with self.assertRaises(ValueError):
                stage.verify_command(row)

    def test_embedded_linker_response_is_not_untracked_input_proof(self):
        with tempfile.TemporaryDirectory() as directory:
            row = self.command_record(directory); row['argv'].insert(1, '-Wl,@original-link.rsp')
            with self.assertRaises(ValueError):
                stage.verify_command(row)

    def test_original_raw_log_mutation_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            row = self.command_record(directory)
            Path(row['log']).write_bytes(b'changed')
            with self.assertRaises(ValueError):
                stage.verify_command(row)

    def test_failed_command_not_requalified_by_present_output(self):
        with tempfile.TemporaryDirectory() as directory:
            row = self.command_record(directory); row['returncode'] = 1
            with self.assertRaises(ValueError):
                stage.verify_command(row)

    def test_response_file_is_not_invented_link_input_proof(self):
        with tempfile.TemporaryDirectory() as directory:
            row = self.command_record(directory); row['argv'].insert(1, '@missing-original.rsp')
            with self.assertRaises(ValueError):
                stage.verify_command(row)

    def test_linked_output_identity_not_just_matching_contents(self):
        with tempfile.TemporaryDirectory() as directory:
            row = self.command_record(directory)
            other = Path(directory) / 'other.o'; other.write_bytes(b'synthetic nonexecuted data')
            row['output'] = str(other)
            with self.assertRaises(ValueError):
                stage.verify_command(row)

    def test_pe_headers_must_be_actual_bounded_amd64_pe32plus(self):
        with tempfile.TemporaryDirectory() as directory:
            pe = Path(directory) / 'header-only.exe'
            data = bytearray(90); data[:2] = b'MZ'; data[60:64] = (64).to_bytes(4, 'little')
            data[64:68] = b'PE\0\0'; data[68:70] = (0xaa64).to_bytes(2, 'little')
            data[88:90] = (0x20b).to_bytes(2, 'little'); pe.write_bytes(data)
            with self.assertRaises(ValueError):
                stage.pe_x64(pe)


if __name__ == '__main__':
    unittest.main()
