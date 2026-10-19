#!/usr/bin/env python3
"""UNEXECUTED REVIEWED SUCCESSOR: actual original CLI v7 origin qualification.
Original recovered template remains unchanged in predecessor/upstream evidence.

This never constructs a cache_context/Runtime ticket or calls a fake provider.
A memory codec or a safely disabled cache cannot satisfy the warm-hit assertions.
Only run after the original SSH Linux 64GiB cgroup window is explicitly released.
"""

# Future Linux only. The keeper admits this direct entry/Python/stdlib before
# startup; no running source can retroactively authenticate its interpreter.
if '__cache_source_context__' not in globals():
    import sys as _source_sys
    if _source_sys.flags.optimize != 0:
        raise RuntimeError('qualification requires unoptimized startup')
    assert __name__ == '__main__' and _source_sys.platform == 'linux'
    assert isinstance(globals().get('__uwvm_entry_captured_bytes__'), bytes), 'use the admitted captured-entry keeper literal'
    assert globals().get('__uwvm_entry_keeper_identity__') == 'cae2c05f7df6e9a2a94a84f5333eeef5c1946d96ea8b54cfe59eee5e17d23d21'
    assert _source_sys.flags.isolated and _source_sys.flags.no_site and _source_sys.flags.safe_path, 'start the admitted entry with -I -S'
    import os as _source_os
    import json as _source_json
    import hashlib as _source_hashlib
    import stat as _source_stat
    import types as _source_types
    import importlib.machinery as _source_machinery
    from pathlib import Path as _SourcePath
    def _source_read_once(path, cap):
        assert path.is_absolute() and path.resolve(strict=True) == path
        fd = _source_os.open(path, _source_os.O_RDONLY | _source_os.O_CLOEXEC | _source_os.O_NOFOLLOW)
        try:
            first = _source_os.fstat(fd)
            assert _source_stat.S_ISREG(first.st_mode) and first.st_uid == _source_os.geteuid() and first.st_size <= cap
            raw = bytearray()
            while len(raw) <= cap:
                part = _source_os.read(fd, min(65536, cap + 1 - len(raw)))
                if not part:
                    break
                raw.extend(part)
            last = _source_os.fstat(fd)
            identity = lambda s: (s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns, s.st_ctime_ns)
            assert identity(first) == identity(last) and len(raw) == first.st_size <= cap
            return bytes(raw)
        finally:
            _source_os.close(fd)
    _guard_path = _SourcePath(_source_os.environ['UWVM2_CACHE_EXECUTION_GUARD'])
    _guard_bytes = _source_read_once(_guard_path, 1 << 20)
    assert _source_hashlib.sha256(_guard_bytes).hexdigest() == _source_os.environ['UWVM2_CACHE_EXECUTION_GUARD_SHA256']
    _guard = _source_json.loads(_guard_bytes)
    _membership = _SourcePath('/proc/self/cgroup').read_text().strip()
    assert _guard['release_note'].strip() and _membership == _guard['membership'] and _membership.startswith('0::/') and '\n' not in _membership
    _cgroup = (_SourcePath('/sys/fs/cgroup') / _membership[3:].lstrip('/')).resolve(strict=True)
    assert _cgroup.stat().st_ino == _guard['cgroup_inode']
    assert _SourcePath('/proc/sys/kernel/random/boot_id').read_text().strip() == _guard['boot_id']
    assert (_cgroup / 'memory.max').read_text().strip() == '68719476736'
    assert (_cgroup / 'memory.swap.max').read_text().strip() == '0'
    _repository = _SourcePath(__file__).resolve(strict=True).parents[2]
    _helper_path = _repository / 'test/0014.llvm_jit/cache_cli_process.py'
    _helper_bytes = _source_read_once(_helper_path, 2 << 20)
    assert _source_hashlib.sha256(_helper_bytes).hexdigest() == '3475afc5e7f08f8cddb45d009d901412280532e88aca45bcbd6d963b395ef3d7'
    _prefix = '_uwvm_cache_' + _source_os.urandom(16).hex()
    _helper_name = _prefix + '.cache_cli_process'
    assert _prefix not in _source_sys.modules and _helper_name not in _source_sys.modules
    _package = _source_types.ModuleType(_prefix)
    _package.__package__, _package.__path__ = _prefix, []
    _package.__spec__ = _source_machinery.ModuleSpec(_prefix, loader=None, is_package=True)
    _helper = _source_types.ModuleType(_helper_name)
    _helper.__file__, _helper.__package__, _helper.__cached__ = str(_helper_path), _prefix, None
    _helper.__spec__ = _source_machinery.ModuleSpec(_helper_name, loader=None, origin=str(_helper_path))
    _helper.__spec__.has_location = True
    _source_sys.modules[_prefix], _source_sys.modules[_helper_name] = _package, _helper
    _package.cache_cli_process = _helper
    try:
        # Exactly the SAME captured bytes which passed the hard Source SHA.
        exec(compile(_helper_bytes, str(_helper_path), 'exec', dont_inherit=True, optimize=0), _helper.__dict__, _helper.__dict__)
        __cache_source_context__ = _helper.admit_source_context(_repository, _guard, _helper, _helper_bytes, _package)
        __cache_source_context__.bind_entry(globals())
    except BaseException:
        if '__cache_source_context__' in globals():
            __cache_source_context__.close(audit=False)
        for _name, _owned in ((_helper_name, _helper), (_prefix, _package)):
            if _source_sys.modules.get(_name) is _owned:
                del _source_sys.modules[_name]
        raise
else:
    __cache_source_context__.assert_loaded_scope(globals())


import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import signal
import stat
import struct
import sys

MAX_BLOB = 32 << 20
READY_STDOUT = b'CACHE-ORACLE42\n'
SEALED = b'[llvm-jit-cache] process-sealed dumpable=0 explicit-cli-request=1 persistence=subject-to-origin-admission\n'
HIT = re.compile(rb'^\[llvm-jit-cache\] object-cache-hit module="[^"]*" bytes=[1-9][0-9]* signature_verified=1$', re.M)
STORE = re.compile(rb'^\[llvm-jit-cache\] object-cache-store-complete module="[^"]*" status=ok bytes=[1-9][0-9]*$', re.M)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def metadata_pairs(raw):
    at = 0
    result = {}

    def leb():
        nonlocal at
        value = 0
        for shift in range(0, 70, 7):
            assert at < len(raw)
            b = raw[at]
            at += 1
            assert shift < 63 or b <= 1
            value |= (b & 127) << shift
            if not b & 128:
                return value
        raise ValueError('invalid metadata length')

    while at < len(raw):
        size = leb()
        assert size <= len(raw) - at
        key = raw[at:at + size]
        at += size
        size = leb()
        assert size <= len(raw) - at and key not in result
        result[key] = raw[at:at + size]
        at += size
    return result


def cache_files(root):
    result = {}
    for path in sorted(root.rglob('*.uwvm-ljc')):
        info = path.lstat()
        assert stat.S_ISREG(info.st_mode) and info.st_nlink == 1 and info.st_uid == os.geteuid()
        assert not info.st_mode & 0o077 and 64 <= info.st_size <= MAX_BLOB
        with path.open('rb') as stream:
            raw = stream.read(MAX_BLOB + 1)
        assert len(raw) == info.st_size
        header = struct.unpack_from('<8sIIIIQQQQQ', raw)
        magic, version, fixed, codec, signature, size, payload, isa, context, sig = header
        assert magic in (b'UWVMLJC\1', b'UWVMROS\1') and (version, fixed, signature, sig) == (7, 64, 1, 64)
        assert codec in (0, 1, 2) and size <= MAX_BLOB and len(raw) == fixed + isa + context + sig + payload
        fields = metadata_pairs(raw[fixed + isa:fixed + isa + context])
        assert fields[b'origin-auth'] == b'kernel-user-keyring-hmac-sha256-ed25519-v1'
        assert len(fields[b'self-executable-sha256']) == 32
        result[str(path.relative_to(root))] = {'sha256': hashlib.sha256(raw).hexdigest(), 'bytes': len(raw),
                                             'self_binary_sha256': fields[b'self-executable-sha256'].hex(),
                                             'isa_metadata_hex': raw[fixed:fixed + isa].hex(),
                                             'context_excluding_self': {k.hex(): v.hex() for k, v in fields.items() if k != b'self-executable-sha256'}}
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--helper-sha256', required=True)
    p.add_argument('--binary', type=Path, required=True)
    p.add_argument('--binary-sha256', required=True)
    p.add_argument('--second-binary', type=Path, required=True)
    p.add_argument('--second-binary-sha256', required=True)
    p.add_argument('--build-receipt', type=Path, required=True)
    p.add_argument('--build-receipt-sha256', required=True)
    p.add_argument('--second-build-receipt', type=Path, required=True)
    p.add_argument('--second-build-receipt-sha256', required=True)
    p.add_argument('--fixture-wat', type=Path, required=True)
    p.add_argument('--fixture-wat-sha256', required=True)
    p.add_argument('--qualification-package', type=Path, required=True)
    p.add_argument('--qualification-manifest-sha256', required=True)
    p.add_argument('--wat-tool', type=Path, required=True)
    p.add_argument('--wat-tool-sha256', required=True)
    p.add_argument('--wat-kind', choices=('wasm-tools', 'wat2wasm'), required=True)
    p.add_argument('--cgroup', type=Path, required=True)
    p.add_argument('--cgroup-inode', type=int, required=True)
    p.add_argument('--membership', required=True)
    p.add_argument('--boot', required=True)
    p.add_argument('--cpu', type=int, default=4)
    p.add_argument('--owned-rss-bytes', type=int, default=4 << 30)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    assert sys.platform == 'linux' and digest(Path(__file__)) == a.helper_sha256
    q = a.qualification_package.resolve(strict=True)
    assert digest(q / 'SOURCE-MANIFEST.json') == a.qualification_manifest_sha256
    qm = json.loads((q / 'SOURCE-MANIFEST.json').read_bytes())
    before = {name: digest(q / name) for name in qm['files']}
    assert before == qm['files']
    inputs = [(Path(__file__), a.helper_sha256), (a.binary, a.binary_sha256), (a.second_binary, a.second_binary_sha256),
              (a.build_receipt, a.build_receipt_sha256), (a.second_build_receipt, a.second_build_receipt_sha256),
              (a.fixture_wat, a.fixture_wat_sha256), (a.wat_tool, a.wat_tool_sha256)]
    assert all(digest(path) == sha for path, sha in inputs)
    assert a.binary_sha256 != a.second_binary_sha256
    # Bind the monitored directory to ACTUAL kernel membership before importing
    # the retained Runner or starting any candidate/tool command.
    assert a.membership.startswith('0::/') and '\n' not in a.membership
    assert Path('/proc/self/cgroup').read_text().strip() == a.membership
    actual_cgroup = (Path('/sys/fs/cgroup') / a.membership[3:].lstrip('/')).resolve(strict=True)
    assert a.cgroup.resolve(strict=True) == actual_cgroup, 'monitoring a different64GiB group is forbidden'
    assert actual_cgroup.stat().st_ino == a.cgroup_inode
    assert Path('/proc/sys/kernel/random/boot_id').read_text().strip() == a.boot
    a.cgroup = actual_cgroup
    common = __cache_source_context__.load_path(q / 'run_cli.py', before['run_cli.py'])
    a.binary, a.second_binary = a.binary.resolve(strict=True), a.second_binary.resolve(strict=True)
    a.wat_tool, a.cgroup = a.wat_tool.resolve(strict=True), a.cgroup.resolve(strict=True)
    assert a.cpu in os.sched_getaffinity(0) and 0 < a.owned_rss_bytes <= 8 << 30
    a.out = a.out.resolve()
    a.out.mkdir(parents=False, exist_ok=False)
    for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
        signal.signal(sig, common.interrupted)
    owner = __cache_source_context__.load_path(q / 'owned_process.py', before['owned_process.py'])
    rss = __cache_source_context__.load_path(q / 'owned_rss_observer.py', before['owned_rss_observer.py'])
    try:
        runner = common.Runner(a, owner, rss)
    except BaseException as exc:
        common.save(a.out / 'report.json', {'passed': False, 'error': repr(exc), 'no_subprocess_started': True})
        return 1
    report = {'schema': 1, 'kind': 'actual-original-cli-cache-v7', 'passed': False,
              'persistent_cache_hit_qualified': False, 'performance_claim': False,
              'component_or_model_results_used_as_runtime_evidence': False,
              'inputs_before': {str(path): sha for path, sha in inputs}, 'qualification_inputs_before': before,
              'admission_before': runner.admission, 'commands': runner.commands, 'cases': [],
              'limits': ['two real build receipts must be independently reviewed for original CLI composition',
                         'same-version proof is byte-exact actual --version output; no source labels substitute',
                         'missing keyctl/secret/PR0/Source origin means positive qualification FAIL, not cache-hit PASS',
                         'no CPUID override, proposal matrix, unsafe-FS, DBG or async race qualification is inferred']}
    try:
        versions = []
        for name, binary in [('a', a.binary), ('b', a.second_binary)]:
            row, stdout, stderr = runner.execute('version-' + name, [str(binary), '--version'], timeout=30)
            assert row['passed'] and row['exit_code'] == 0
            versions.append((stdout, stderr))
        assert versions[0] == versions[1], 'must be different real images with unchanged actual version output'
        report['version_stdout_hex'], report['version_stderr_hex'] = (data.hex() for data in versions[0])
        wasm = a.out / 'fixture.wasm'
        argv = ([str(a.wat_tool), 'parse', str(a.fixture_wat), '-o', str(wasm)] if a.wat_kind == 'wasm-tools' else
                [str(a.wat_tool), '--enable-all', str(a.fixture_wat), '-o', str(wasm)])
        tool, _, _ = runner.execute('actual-wasm-producer', argv, timeout=60)
        assert tool['passed'] and tool['exit_code'] == 0 and wasm.is_file()
        report['wasm_sha256'] = digest(wasm)
        cache = a.out / 'actual-cache'
        cache.mkdir(mode=0o700)

        def run(name, binary, want_hit):
            args = [str(binary), '-Raot', '-Rct', '0', '-Rllvm-cache-seal-process',
                    '-Rllvm-cache-path', 'path', str(cache), '-Rclog', 'err', '--run', str(wasm)]
            row, stdout, stderr = runner.execute(name, args, timeout=300)
            guest, diagnostics = common.guest_stderr(stderr)
            assert row['passed'] and row['exit_code'] == 0 and row['natural_exit_observed']
            assert stdout == READY_STDOUT and guest == b'' and diagnostics.count(SEALED) == 1
            hits = len(HIT.findall(diagnostics))
            assert (hits > 0) == want_hit, (name, hits)
            if not want_hit:
                assert STORE.search(diagnostics), 'must prove actual origin-authenticated publication, not cache-off compilation'
            report['cases'].append({'name': name, 'passed': True, 'signature_verified_hits': hits,
                                    'stdout_hex': stdout.hex(), 'guest_stderr_hex': guest.hex(),
                                    'diagnostics_sha256': hashlib.sha256(diagnostics).hexdigest()})
            return cache_files(cache)

        a_cold = run('a-cold', a.binary, False)
        assert len(a_cold) == 1 and all(v['self_binary_sha256'] == a.binary_sha256 for v in a_cold.values())
        assert run('a-warm', a.binary, True) == a_cold
        b_cold = run('b-same-version-cold', a.second_binary, False)
        assert len(b_cold) == 2 and all(b_cold[k] == v for k, v in a_cold.items())
        new_names = set(b_cold) - set(a_cold)
        assert len(new_names) == 1
        b_name = next(iter(new_names))
        assert b_cold[b_name]['self_binary_sha256'] == a.second_binary_sha256
        a_entry = next(iter(a_cold.values()))
        assert b_cold[b_name]['isa_metadata_hex'] == a_entry['isa_metadata_hex']
        assert b_cold[b_name]['context_excluding_self'] == a_entry['context_excluding_self'], 'configuration differs beyond the actual binary SHA'
        assert run('b-warm', a.second_binary, True) == b_cold
        damaged = cache / b_name
        with damaged.open('rb') as stream:
            backup = stream.read(MAX_BLOB + 1)
        assert len(backup) <= MAX_BLOB and hashlib.sha256(backup).hexdigest() == b_cold[b_name]['sha256']
        (a.out / 'b-cache-before-truncation.bin').write_bytes(backup)
        # Only this task's exact owner-checked previously authenticated leaf is
        # deliberately corrupted. Preserve the original bytes; no global cache.
        assert digest(damaged) == b_cold[b_name]['sha256']
        with damaged.open('wb') as stream:
            stream.write(backup[:32])
            stream.flush()
            os.fsync(stream.fileno())
        repaired = run('b-truncated-cache-rejected', a.second_binary, False)
        assert set(repaired) == set(b_cold) and repaired[b_name]['self_binary_sha256'] == a.second_binary_sha256
        assert run('b-after-repair-warm', a.second_binary, True) == repaired
        assert digest(wasm) == report['wasm_sha256']
        report['cache_files_final'] = repaired
        report['persistent_cache_hit_qualified'] = True
        report['passed'] = len(report['cases']) == 6 and all(row['passed'] for row in report['cases'])
    except BaseException as exc:
        report.update(passed=False, persistent_cache_hit_qualified=False, error=repr(exc))
    finally:
        for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
            signal.signal(sig, signal.SIG_IGN)
        try:
            report['admission_after'] = runner.guard(force=True)
            assert all(digest(path) == sha for path, sha in inputs)
            assert digest(q / 'SOURCE-MANIFEST.json') == a.qualification_manifest_sha256
            report['qualification_inputs_after'] = {name: digest(q / name) for name in before}
            assert report['qualification_inputs_after'] == before
            assert all(row.get('retired') and row.get('ownership', {}).get('closed') and
                       all(member.get('pidfd_retained') is False for member in row['ownership']['members']) for row in runner.commands)
            report['inputs_after'] = {str(path): digest(path) for path, _ in inputs}
        except BaseException as exc:
            report.update(passed=False, persistent_cache_hit_qualified=False, finalization_error=repr(exc))
        common.save(a.out / 'report.json', report)
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    finally:
        __cache_source_context__.close()
