#!/usr/bin/env python3
"""SOURCE ONLY: bounded serial case facts; no invented guest script exit."""
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import stat


if not __debug__:
    raise RuntimeError('original assertion checks require an unoptimized Python host')


LIMIT = 1 << 20
LABELS = ('uwvm2-ros-eh', 'uwvm2-ros-noeh', 'uwvm2-eh', 'uwvm2-noeh')
RESULT_SCHEMA = 'uwvm-windows-raii-r5-paired-launcher-actual-guest-v1'
QUAL_SCHEMA = 'uwvm-windows-raii-r5-paired-launcher-guest-qualification-v1'


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('duplicate JSON key')
        result[key] = value
    return result


def fail_constant(value):
    raise ValueError('non-JSON numeric constant: ' + value)


def bounded_json(raw):
    assert 0 < len(raw) <= LIMIT
    value = json.loads(raw.decode('utf-8'), object_pairs_hook=unique_object,
                       parse_constant=fail_constant)
    pending = [(value, 0)]
    nodes = 0
    while pending:
        item, depth = pending.pop()
        nodes += 1
        assert nodes <= 8192 and depth <= 32
        if isinstance(item, dict):
            pending.extend((child, depth + 1) for child in item.values())
        elif isinstance(item, list):
            pending.extend((child, depth + 1) for child in item)
    return value


def exact_int(value, low, high):
    assert type(value) is int and low <= value <= high
    return value


def sha256(value):
    assert isinstance(value, str) and re.fullmatch('[0-9a-f]{64}', value)
    return value


def os_inventory(value):
    assert isinstance(value, list) and len(value) == 4
    names = set()
    for row in value:
        assert isinstance(row, dict)
        name = row['name']
        assert name in ('kernel32.dll', 'kernelbase.dll', 'ntdll.dll', 'ucrtbase.dll')
        assert name not in names
        names.add(name)
        exact_int(row['bytes'], 1, 32 << 20)
        sha256(row['sha256'])
        assert row['loaded_component_map_proof'] is False
        assert isinstance(row['path'], str) and len(row['path']) <= 32767
        assert isinstance(row['file_version'], str) and len(row['file_version']) <= 256
    return value


def accept_case_payload(raw, qualification, qualification_sha256):
    """Validate reported completed cases; retain unqualified later exits."""
    assert qualification['schema'] == QUAL_SCHEMA
    for key in ('actual_four_pe_builds_accepted',
                'actual_bridge_assembly_review_accepted',
                'actual_four_launcher_builds_accepted',
                'actual_four_launcher_wmain_assembly_review_accepted',
                'guest_OS_file_inventory_required',
                'actual_SDK_static_LLVM_runtime_provider_bound',
                'actual_GNU_unwind_imports_rejected', 'standalone_component_only'):
        assert qualification[key] is True
    for key in ('product_three_tu_abi_qualified', 'named_module_qualified',
                'new_readonly_sync_provider_qualified',
                'actual_component_loaded_DLL_maps_qualified',
                'ROS_vendored_LLVM23_full_product_qualified'):
        assert qualification[key] is False
    nonce = sha256(qualification['nonce'])
    sha256(qualification_sha256)
    value = bounded_json(raw)
    assert isinstance(value, dict) and value['schema'] == RESULT_SCHEMA
    assert value['nonce'] == nonce and value['qualification_sha256'] == qualification_sha256
    assert value['passed'] is True and value['error'] is None
    assert value['serial_transport_error'] is None
    assert value['standalone_component_only'] is True
    assert value['standalone_SDK_static_LLVM_recipe_only'] is True
    for key in ('product_three_tu_abi_qualified', 'named_module_qualified',
                'new_readonly_sync_provider_qualified',
                'ROS_vendored_LLVM23_full_product_qualified',
                'actual_component_loaded_DLL_maps_qualified'):
        assert value[key] is False
    assert value['actual_architecture'] == 'AMD64'
    assert value['runner_sha256'] == qualification['runner_sha256']
    before = os_inventory(value['guest_OS_file_inventory_before'])
    after = os_inventory(value['guest_OS_file_inventory_after'])
    assert before == after
    cases = value['cases']
    expected_cases = qualification['cases']
    assert isinstance(expected_cases, list) and len(expected_cases) == 4
    expected = {row['label']: row for row in expected_cases}
    assert set(expected) == set(LABELS)
    for label, row in expected.items():
        assert row['launcher_label'] == label
        sha256(row['pe_sha256'])
        sha256(row['launcher_pe_sha256'])
    assert isinstance(cases, list) and len(cases) == 4
    seen = set()
    for row in cases:
        label = row['label']
        assert label in expected and label not in seen
        seen.add(label)
        expect = expected[label]
        assert row['passed'] is True and row['assigned_before_resume'] is True
        assert row['launcher_label'] == label
        assert sha256(row['pe_sha256']) == expect['pe_sha256']
        assert sha256(row['launcher_pe_sha256']) == expect['launcher_pe_sha256']
        exact_int(row['actual_pid'], 1, (1 << 32) - 1)
        exact_int(row['actual_creation_filetime'], 1, (1 << 64) - 1)
        assert exact_int(row['exit_code'], 0, (1 << 32) - 1) == 0
        assert exact_int(row['owned_job_active_after_retirement'], 0, (1 << 32) - 1) == 0
        assert isinstance(row['component_stdout'], str)
        assert len(row['component_stdout'].encode('utf-8')) <= 524288
        assert row['component_stderr'] == ''
        for death, code in (('invalid-gate', 'e0000db6'), ('invalid-set-event', 'e0000db6'),
                            ('invalid-first-pc', 'e0000db7'), ('invalid-live-destruction', 'e0000db6')):
            assert 'Windows RAII isolated death: --' + death + ' exit=' + code in row['component_stdout']
        assert 'PASS Windows x64 native-step RAII: real zero/one/two instruction traps, owned GPR copy, exact handle mirrors, failed-request emptiness, clear retirement, 256 ready races and four isolated ownership deaths' in row['component_stdout']
        assert isinstance(row['launcher_stdout'], str)
        assert len(row['launcher_stdout'].encode('utf-8')) <= 524288
        assert row['launcher_stderr'] == ''
        assert re.search(r'raii-child-pid=[0-9]+ inherited-job=yes output=regular', row['launcher_stdout'])
        assert 'raii-child-exit=0' in row['launcher_stdout']
    return {'schema': 'uwvm-windows-serial-completed-case-facts-v1',
            'scope': 'four reported completed exact standalone native cases only',
            'payload_bytes': len(raw), 'payload_sha256': hashlib.sha256(raw).hexdigest(),
            'qualification_sha256': qualification_sha256,
            'reported_completed_cases': value,
            'guest_PowerShell_exit_qualified': False,
            'SerialPort_post_frame_Flush_Dispose_retirement_qualified': False,
            'actual_component_loaded_DLL_maps_qualified': False,
            'VM_boot_QMP_PIDFD_identity_qualified_by_this_parser': False,
            'whole_product_or_ROS_bundled_qualified': False}


def parse_original_serial(path, qualification, qualification_sha256):
    """Read a bounded regular host file; never execute guest bytes."""
    path = Path(path)
    info = path.lstat()
    assert stat.S_ISREG(info.st_mode) and info.st_nlink == 1
    assert info.st_uid == os.geteuid() and info.st_size <= 32 << 20
    # This helper is Linux-host-only. Refuse unknown open semantics rather
    # than replacing O_NOFOLLOW with a prior pathname check. O_NONBLOCK
    # also prevents an exchanged FIFO from blocking admission.
    flags = os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW | os.O_NONBLOCK
    owned_fd = os.open(path, flags)
    try:
        actual = os.fstat(owned_fd)
        def identity(row):
            return (row.st_dev, row.st_ino, row.st_mode, row.st_uid,
                    row.st_nlink, row.st_size, row.st_mtime_ns, row.st_ctime_ns)
        assert identity(actual) == identity(info)
        pieces = []
        total = 0
        while True:
            piece = os.read(owned_fd, min(65536, (32 << 20) + 1 - total))
            if not piece:
                break
            total += len(piece)
            assert total <= 32 << 20
            pieces.append(piece)
        raw = b''.join(pieces)
        assert len(raw) == actual.st_size
        assert identity(os.fstat(owned_fd)) == identity(actual)
        assert identity(path.lstat()) == identity(actual)
    finally:
        os.close(owned_fd)
    nonce = sha256(qualification['nonce']).encode('ascii')
    prefix = b'UWVM-WIN-RAII-R5 ' + nonce + b' '
    frames = [line for line in raw.splitlines() if line.startswith(prefix)]
    # Multiple result frames are refused, including a later contradictory one.
    assert len(frames) == 1
    pieces = frames[0].split(b' ', 4)
    assert len(pieces) == 5 and re.fullmatch(b'[1-9][0-9]{0,6}', pieces[2])
    count = int(pieces[2])
    assert count <= LIMIT and re.fullmatch(b'[0-9a-f]{64}', pieces[3])
    assert len(pieces[4]) == 4 * ((count + 2) // 3)
    payload = base64.b64decode(pieces[4], validate=True)
    assert len(payload) == count and hashlib.sha256(payload).hexdigest().encode('ascii') == pieces[3]
    result = accept_case_payload(payload, qualification, qualification_sha256)
    result['original_serial_bytes'] = len(raw)
    result['original_serial_sha256'] = hashlib.sha256(raw).hexdigest()
    return result
