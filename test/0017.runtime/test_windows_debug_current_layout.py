"""Synthetic R5 Win64 layout/COFF negatives; no native APIs or compiler execution."""
from pathlib import Path
import tempfile
import unittest
import windows_debug_current_layout_contract as layout


class LayoutControls(unittest.TestCase):
    def flags(self, directory):
        return ['clang++', '--target=x86_64-w64-windows-gnu', '--sysroot=' + str(directory), '-std=c++26',
                '-stdlib=libc++', '-fexceptions', '-fasynchronous-unwind-tables', '-DUWVM=2', '-DUWVM_USE_LLVM_JIT=1',
                '-DUWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT=1']

    def test_same_prefix_and_identity_only_exception(self):
        with tempfile.TemporaryDirectory() as directory:
            flags, cwd = self.flags(directory), Path(directory)
            self.assertEqual(layout.layout_contract(flags, cwd), layout.layout_contract(flags + ['-DUWVM2_BUILD_SOURCE_ID=example'], cwd))
            self.assertNotEqual(layout.layout_contract(flags, cwd), layout.layout_contract(flags + ['-D_LIBCPP_ABI_VERSION=2'], cwd))

    def test_native_capture_and_other_namespace_macros_are_not_ignored(self):
        with tempfile.TemporaryDirectory() as directory:
            flags, cwd = self.flags(directory), Path(directory)
            enabled = flags + ['-DUWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT=1']
            self.assertNotEqual(layout.layout_contract(flags, cwd), layout.layout_contract(enabled, cwd))
            self.assertNotEqual(layout.layout_contract(flags + ['-DUWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT=0'], cwd), layout.layout_contract(enabled, cwd))
            with self.assertRaises(ValueError): layout.layout_contract(enabled + ['-UUWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT'], cwd)

    def test_eh_target_and_incomplete_response_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            flags, cwd = self.flags(directory), Path(directory)
            for extra in (['-fno-exceptions'], ['-fno-unwind-tables'], ['--target=i686-w64-windows-gnu'], ['@unexpanded.rsp'], ['-include']):
                with self.assertRaises(ValueError): layout.layout_contract(flags + extra, cwd)

    def test_sdk_header_and_forced_include_difference_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            cwd=Path(directory); header=cwd/'owned.h'; header.write_bytes(b'synthetic source metadata')
            flags=self.flags(directory)
            self.assertNotEqual(layout.layout_contract(flags,cwd), layout.layout_contract(flags+['-include',str(header)],cwd))
            self.assertNotEqual(layout.layout_contract(flags,cwd), layout.layout_contract(flags+['-I',str(cwd)],cwd))

    def coff(self):
        return '\nFile: actual.lib(member.obj)\nFormat: COFF-x86-64\nArch: x86_64\nAddressSize: 64bit\nImageFileHeader {\n  Machine: IMAGE_FILE_MACHINE_AMD64 (0x8664)\n}\n'

    def test_native_qualification_switch_must_be_explicit_in_both_tus(self):
        with tempfile.TemporaryDirectory() as directory:
            flags, cwd = self.flags(directory), Path(directory)
            enable = '-DUWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT=1'
            absent = [value for value in flags if value != enable]
            bare = '-DUWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT'
            for value in (absent, absent + ['-DUWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT=0'],
                          absent + ['-DUWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT=2'],
                          absent + ['-UUWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT'],
                          flags + [bare], absent + [bare, bare]):
                with self.assertRaises(ValueError): layout.layout_contract(value, cwd)
            # Bare-D and explicit =1 are the same real gate value. This does not
            # qualify a native VM or permit rewriting original captured argv.
            explicit = layout.layout_contract(flags, cwd)
            self.assertEqual(explicit, layout.layout_contract(absent + [bare], cwd))
            self.assertEqual(explicit, layout.layout_contract(absent + ['-D', bare[2:]], cwd))
            self.assertNotEqual(layout.layout_contract(flags + ['-DOTHER_LAYOUT_MACRO'], cwd),
                                layout.layout_contract(flags + ['-DOTHER_LAYOUT_MACRO=1'], cwd))

    def test_actual_formatter_all_member_headers_required(self):
        self.assertEqual(layout.coff_amd64_headers(self.coff()+self.coff()),2)
        for text in ('PASS', self.coff().replace('COFF-x86-64','ELF64-x86-64'), self.coff().replace('0x8664','0xaa64'), self.coff().replace('AddressSize: 64bit\n','')):
            with self.assertRaises(ValueError): layout.coff_amd64_headers(text)

    def test_hidden_mixed_arch_member_not_qualified(self):
        with self.assertRaises(ValueError):
            layout.coff_amd64_headers(self.coff()+self.coff().replace('IMAGE_FILE_MACHINE_AMD64 (0x8664)','IMAGE_FILE_MACHINE_ARM64 (0xaa64)'))


if __name__ == '__main__':
    unittest.main()
