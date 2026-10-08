"""Synthetic accounting/closure negatives; no Mach API or child execution."""
import ctypes
import errno
from types import SimpleNamespace
from pathlib import Path
import tempfile
import unittest
import macos_owned_debug_process_rss as owner
import macos_debug_current_build_contract as contract


class PhysicalBudget(unittest.TestCase):
    def test_public_name_accounting_layout(self):
        self.assertEqual(ctypes.sizeof(owner.MachTaskBasicInfo), 48)
        self.assertEqual(owner.MachTaskBasicInfo.resident_max.offset, 16)

    def test_sample_includes_parent_and_witness(self):
        self.assertEqual(owner.check_rss(100, [200, owner.RSS_STOP - 301]), owner.RSS_STOP - 1)
        with self.assertRaises(RuntimeError):
            owner.check_rss(100, [200, owner.RSS_STOP - 300])

    def test_sum_of_actual_peaks_is_conservative(self):
        self.assertEqual(owner.peak_upper(1 << 30, 1 << 30, (2 << 30) - 1), owner.PHYSICAL_LIMIT - 1)
        with self.assertRaises(RuntimeError):
            owner.peak_upper(1 << 30, 1 << 30, 2 << 30)

    def test_invalid_counter_not_sample_fallback(self):
        for values in ((-1, 0, 0), (0, -1, 0), (0, 0, -1), (True, 0, 0), (owner.PHYSICAL_LIMIT, 0, 0)):
            with self.assertRaises(RuntimeError):
                owner.peak_upper(*values)

    def test_birth_remains_precise_after_reparent(self):
        value = owner.Birth(17, 200, 3, 501, 17)
        self.assertTrue(owner.identity_matches(value, value))
        self.assertFalse(owner.identity_matches(value, owner.Birth(17, 200, 4, 501, 17)))


class RoleSampleControls(unittest.TestCase):
    def test_bounded_actual_samples_and_no_role_double_count(self):
        receipt = {}
        births = [owner.Birth(17, 200, 3, 501, 17), owner.Birth(18, 200, 4, 501, 17)]
        for value in range(owner.MAX_RETAINED_RSS_SAMPLES + 2):
            owner.record_rss_sample(receipt, [('root', births[0], value), ('witness', births[1], 10)], value + 10)
        self.assertEqual(len(receipt['rss_samples_retained']), owner.MAX_RETAINED_RSS_SAMPLES)
        self.assertEqual(receipt['rss_observation_count'], owner.MAX_RETAINED_RSS_SAMPLES + 2)
        self.assertEqual(receipt['rss_sample_role_maxima']['17']['sampled_max_rss_bytes'], owner.MAX_RETAINED_RSS_SAMPLES + 1)
        with self.assertRaises(RuntimeError): owner.record_rss_sample(receipt, [('root', births[0], 1), ('witness', births[0], 1)], 2)

    def test_role_pid_reuse_is_not_a_new_sample_permission(self):
        receipt = {}; birth = owner.Birth(17, 200, 3, 501, 17)
        owner.record_rss_sample(receipt, [('root', birth, 10)], 10)
        with self.assertRaises(RuntimeError):
            owner.record_rss_sample(receipt, [('root', owner.Birth(17, 200, 4, 501, 17), 5)], 5)


class BsdLookup(unittest.TestCase):
    def lookup(self, size=136, error=0, returned_pid=17, status=5):
        calls = []

        class Proc:
            def proc_pidinfo(self, pid, flavor, arg, output, count):
                calls.append((pid, flavor, arg, count))
                # Borrow only the test-owned byref buffer; no native API or
                # unknown process pointer is accessed by this synthetic oracle.
                value = ctypes.cast(output, ctypes.POINTER(owner.BsdInfo)).contents
                value.pid, value.status = returned_pid, status
                value.start_sec, value.start_usec = 200, 3
                value.uid, value.pgid = 501, 17
                ctypes.set_errno(error)
                return size

        native = owner.MacNative.__new__(owner.MacNative)  # do not load Mach/libproc
        native.proc = Proc()
        return native, calls

    def test_unreaped_zombie_lookup_explicitly_requested(self):
        native, calls = self.lookup()
        value = native.bsd(17)
        self.assertEqual(calls, [(17, 3, 1, 136)])
        self.assertEqual(value.status, 5)
        self.assertEqual(native.birth(value), owner.Birth(17, 200, 3, 501, 17))

    def test_partial_wrong_pid_and_permission_failure_rejected(self):
        for size, error, returned_pid in ((135, 0, 17), (136, 0, 18), (0, errno.EACCES, 0), (0, 0, 0)):
            native, calls = self.lookup(size, error, returned_pid)
            with self.assertRaises(RuntimeError) as failure:
                native.bsd(17, missing=True)
            self.assertEqual(calls, [(17, 3, 1, 136)])
            text = str(failure.exception)
            for detail in (f'pid=17', f'bytes={size}', f'errno={error}', f'returned_pid={returned_pid}', 'status=5'):
                self.assertIn(detail, text)

    def test_missing_only_for_explicit_absent_lookup(self):
        for error in (errno.ESRCH, errno.ENOENT):
            native, calls = self.lookup(0, error, 0)
            self.assertIsNone(native.bsd(17, missing=True))
            with self.assertRaises(RuntimeError) as failure:
                native.bsd(17)
            self.assertIn(f'errno={error}', str(failure.exception))
            self.assertEqual(calls, [(17, 3, 1, 136), (17, 3, 1, 136)])


class Retirement(unittest.TestCase):
    def command(self, *, changed=False, unknown=False, missing_root=False):
        supervisor_pid = owner.os.getpid()
        root_pid, witness_pid = supervisor_pid + 10000, supervisor_pid + 10001
        identities = {root_pid: owner.Birth(root_pid, 200, 3, 501, root_pid),
                      witness_pid: owner.Birth(witness_pid, 200, 4, 501, root_pid),
                      supervisor_pid: owner.Birth(supervisor_pid, 200, 2, 501, supervisor_pid)}
        values = {}
        for pid, birth in identities.items():
            info = owner.BsdInfo()
            info.pid, info.start_sec, info.start_usec = pid, birth.seconds, birth.microseconds
            info.uid, info.pgid, info.status = birth.uid, birth.pgid, 5 if pid == root_pid else 2
            values[pid] = info
        if changed:
            values[witness_pid].start_usec += 1
        members = [root_pid, witness_pid]
        if unknown:
            info = owner.BsdInfo()
            info.pid, info.pgid, info.status = witness_pid + 1, root_pid, 2
            values[info.pid] = info; members.append(info.pid)
        if missing_root:
            members.remove(root_pid)

        class Native:
            def __init__(self): self.rss_calls = 0
            def bsd(self, pid, missing=False): return values[pid]
            birth = staticmethod(owner.MacNative.birth)
            def members(self, group): return members
            def rss(self, pid, info):
                self.rss_calls += 1
                raise RuntimeError('synthetic task RSS unavailable during teardown')
            def swap(self): return {'used_bytes': 0, 'swapped_count': 0, 'swapins': 0, 'swapouts': 0}

        native = Native()
        command = owner.Owned.__new__(owner.Owned)  # no spawn or admission path
        command.closed, command.child, command.root = False, object(), identities[root_pid]
        command.expected = {pid: identities[pid] for pid in (root_pid, witness_pid)}
        command.orphan, command.attestations = identities[witness_pid], []
        command.peak, command.samples, command.receipt = 1234, 7, {'orphan_requested': True}
        command.lease = SimpleNamespace(native=native, birth=identities[supervisor_pid], swap_before=native.swap())
        return command, native

    def test_retirement_is_not_a_zero_or_qualified_rss_sample(self):
        command, native = self.command()
        self.assertEqual(command.check(retiring=True).status, 5)
        self.assertEqual(native.rss_calls, 0)
        self.assertEqual((command.peak, command.samples), (1234, 7))
        self.assertFalse(command.receipt['retirement_rss_measurement']['qualified'])

    def test_retirement_still_rejects_birth_unknown_live_and_missing_anchor(self):
        for setting in ('changed', 'unknown', 'missing_root'):
            command, native = self.command(**{setting: True})
            with self.assertRaises(RuntimeError): command.check(retiring=True)
            self.assertEqual(native.rss_calls, 0)

    def test_normal_admission_keeps_strict_task_rss(self):
        command, native = self.command()
        with self.assertRaises(RuntimeError): command.check()
        self.assertEqual(native.rss_calls, 1)
        self.assertNotIn('retirement_rss_measurement', command.receipt)

    def test_normal_rss_error_preserves_original_errno_and_actual_bsd_status(self):
        class Proc:
            def proc_pidinfo(self, pid, flavor, arg, output, count):
                self.assertion = (pid, flavor, arg, count)
                ctypes.set_errno(errno.ESRCH)
                return 0
        native = owner.MacNative.__new__(owner.MacNative)
        native.proc = Proc()
        info = owner.BsdInfo()
        info.pid, info.status = 17, 2
        def again(pid, missing=False):
            ctypes.set_errno(errno.EACCES)  # must not replace the task call's errno
            return info
        native.bsd = again
        with self.assertRaises(RuntimeError) as failure: native.rss(17, info)
        self.assertEqual(native.proc.assertion, (17, 4, 0, 96))
        for detail in ('pid=17', 'bytes=0', f'errno={errno.ESRCH}', 'initial_status=2', 'returned_status=2'):
            self.assertIn(detail, str(failure.exception))


class Closure(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.cwd = Path(self.temp.name)
        for name in ('sdk', 'include', 'other', 'sdk/extra'):
            (self.cwd / name).mkdir(exist_ok=True)
        (self.cwd / 'forced.h').write_text('// synthetic header identity only\n')

    def layout(self, flags, cwd=None):
        return contract.layout_contract(flags, self.cwd if cwd is None else cwd)

    def flags(self):
        return ['clang++', '--target=arm64-apple-macos14', '-isysroot', 'sdk', '-std=c++26', '-stdlib=libc++',
                '-fexceptions', '-funwind-tables', '-Iinclude', '-include', 'forced.h', '-DUWVM=2', '-DUWVM_USE_LLVM_JIT=1', '-DUWVM_RUNTIME_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA=0']

    def loads(self):
        return '\n'.join([
            'fresh:',
            '\t/usr/lib/libSystem.B.dylib (compatibility version 1.0.0, current version 1351.0.0)',
            '\t/System/Library/Frameworks/Security.framework/Versions/A/Security (compatibility version 1.0.0, current version 1.0.0)',
            '\t/System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation (compatibility version 1.0.0, current version 1.0.0)',
        ]) + '\n'

    def test_identical_macro_contract(self):
        self.assertEqual(self.layout(self.flags()), self.layout(self.flags()))
        changed = [*self.flags()[:-1], '-DUWVM_RUNTIME_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA=1']
        self.assertNotEqual(self.layout(self.flags()), self.layout(changed))

    def test_identity_macro_not_layout_permission(self):
        self.assertEqual(self.layout(self.flags()), self.layout(self.flags() + ['-DUWVM_BUILD_SOURCE_ID=example']))
        self.assertNotEqual(self.layout(self.flags()), self.layout(self.flags() + ['-DUWVM_SINGLE_THREAD=1']))

    def test_main_runtime_native_capture_layout_macro_difference(self):
        flags = self.flags()
        captured = flags + ['-DUWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT=1']
        disabled = flags + ['-DUWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT=0']
        self.assertNotEqual(self.layout(flags), self.layout(captured))
        self.assertNotEqual(self.layout(disabled), self.layout(captured))
        self.assertEqual(self.layout(flags), self.layout(flags + ['-DUWVM2_BUILD_SOURCE_ID=example']))
        with self.assertRaises(ValueError):
            self.layout(captured + ['-DUWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT=0'])

    def test_eh_or_arch_mismatch_rejected(self):
        for flags in (self.flags() + ['-fno-exceptions'], self.flags() + ['-fno-unwind-tables'],
                      [*self.flags()[:1], '--target=x86_64-apple-macos14', *self.flags()[2:]]):
            with self.assertRaises(ValueError):
                self.layout(flags)

    def test_duplicate_macro_or_unresolved_target_rejected(self):
        for flags in (self.flags() + ['-DUWVM_USE_LLVM_JIT=1'], self.flags() + ['--target'],
                      self.flags() + ['-include'], ['clang++', '-fexceptions', '-funwind-tables']):
            with self.assertRaises(ValueError):
                self.layout(flags)

    def test_all_nonidentity_macros_compared(self):
        for define in ('-D_LIBCPP_ABI_VERSION=2', '-DOTHER_NATIVE_LAYOUT=1'):
            self.assertNotEqual(self.layout(self.flags()), self.layout(self.flags() + [define]))
        self.assertNotEqual(self.layout(self.flags()), self.layout([v if v != '-DUWVM=2' else '-DUWVM=3' for v in self.flags()]))
        with self.assertRaises(ValueError): self.layout(self.flags() + ['-DOTHER_NATIVE_LAYOUT=1', '-UOTHER_NATIVE_LAYOUT'])

    def test_actual_search_sdk_and_forced_header_compared(self):
        original = self.layout(self.flags())
        for extra in (['-isystem', 'other'], ['-isystemother'], ['-F', 'sdk/extra'], ['-Fsdk/extra'], ['-iframeworksdk/extra']):
            self.assertNotEqual(original, self.layout(self.flags() + extra))
        self.assertNotEqual(original, self.layout([v if v != '-Iinclude' else '-Iother' for v in self.flags()]))
        second = self.cwd / 'second'; second.mkdir()
        (second / 'sdk').mkdir(); (second / 'include').mkdir(); (second / 'forced.h').write_text('// distinct source\n')
        self.assertNotEqual(original, self.layout(self.flags(), second))
        for invalid in (self.flags() + ['-isysroot', 'sdk'], self.flags() + ['-I'], self.flags() + ['-include-pch']):
            with self.assertRaises(ValueError): self.layout(invalid)

    def test_complete_abi_flags_and_opaque_argv_refusal(self):
        original = self.layout(self.flags())
        for flag in ('-fshort-wchar', '-fpack-struct=1', '-fclang-abi-compat=18', '-fno-rtti', '-funsigned-char'):
            self.assertNotEqual(original, self.layout(self.flags() + [flag]))
        for extra in (['-fno-cxx-exceptions'], ['-fno-asynchronous-unwind-tables'], ['@actual.rsp'],
                      ['-Xclang', '-opaque'], ['-Xpreprocessor', '-DLAYOUT=1'], ['-fmodule-file=old.pcm']):
            with self.assertRaises(ValueError): self.layout(self.flags() + extra)
        for valid, invalid in (('-std=c++26', '-std=c++23'), ('-stdlib=libc++', '-stdlib=libstdc++')):
            with self.assertRaises(ValueError): self.layout([v if v != valid else invalid for v in self.flags()])
        for target in ('arm64-unknown-apple', 'arm64-apple-ios14', 'arm64-apple-macos14-extra'):
            with self.assertRaises(ValueError): self.layout([*self.flags()[:1], '--target=' + target, *self.flags()[2:]])

    def test_official_formatter_system_closure(self):
        self.assertEqual(len(contract.system_loads(self.loads(), Path('fresh'))), 3)
        self.assertEqual(contract.system_loads('fresh:\n', Path('fresh'), rpaths=True), [])

    def test_external_dylib_and_unqualified_rpath_rejected(self):
        for path in ('@rpath/libLLVM.dylib', '/tmp/libLLVM.dylib', '/usr/lib/../tmp/libX.dylib'):
            text = self.loads() + '\t' + path + ' (compatibility version 1.0.0, current version 1.0.0)\n'
            with self.assertRaises(ValueError):
                contract.system_loads(text, Path('fresh'))
        with self.assertRaises(ValueError):
            contract.system_loads('fresh:\n/usr/local/lib\n', Path('fresh'), rpaths=True)

    def test_malformed_missing_duplicate_oracle_rejected(self):
        for text in ('PASS', self.loads().replace('/usr/lib/libSystem.B.dylib', '/usr/lib/libc++.1.dylib'),
                     self.loads() + self.loads().splitlines()[1] + '\n'):
            with self.assertRaises(ValueError):
                contract.system_loads(text, Path('fresh'))

    def test_selected_search_option_bounds(self):
        self.assertEqual(contract.option_values(['clang++', '-isysroot', '/sdk', '-F/sdk/extra'], ('-isysroot', '-F')), ['/sdk', '/sdk/extra'])
        with self.assertRaises(ValueError):
            contract.option_values(['clang++', '-isysroot'], ('-isysroot',))


if __name__ == '__main__':
    unittest.main()
