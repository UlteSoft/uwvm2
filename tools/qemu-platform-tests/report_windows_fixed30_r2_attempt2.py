#!/usr/bin/env python3
"""Verify immutable original evidence and write a narrowly scoped actual report."""
import hashlib
import json
from pathlib import Path
import re
import tarfile


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    packet = Path('/tmp/uwvm-windows-fixed30-r2-attempt2-actual-small-evidence-20261003-r1')
    archive = packet.with_suffix('.tar.gz')
    manifest_bytes = (packet / 'manifest.json').read_bytes()
    assert digest(manifest_bytes) == 'be2b967eba5a20df1863150c8b3dbdad916eab6c8d46de5e8ed43f93559d15ca'
    assert archive.stat().st_size == 886613
    assert digest(archive.read_bytes()) == '4d86cd181a7d78cd50412be7976440d66fb3ed3934e99b1523e54467ba313dcb'
    manifest = json.loads(manifest_bytes)
    assert len(manifest['files']) == 62
    with tarfile.open(archive, 'r:gz') as tf:
        members = {m.name: m for m in tf.getmembers()}
        assert len(members) == 63
        payload = {}
        for name, rec in manifest['files'].items():
            member = members[name]
            assert member.isfile()
            data = tf.extractfile(member).read()
            assert len(data) == rec['bytes'] and digest(data) == rec['sha256']
            payload[name] = data
        assert tf.extractfile(members['manifest.json']).read() == manifest_bytes

    def load(name):
        return json.loads(payload[name])

    receipts = load('00-receipts.json')
    bridge = load('01-lane-retirement.json')
    ticket = load('02-f23cd5c5b4cc4cf48b56d096cfbaa9ef.json')
    commands = load('36-commands.json')
    assert len(receipts) == len(commands) == 30
    assert ticket['owner'] == '/root/qemu_platform_tests' and ticket['state'] == 'retired'
    assert ticket['suite'] == 'windows-raii-launcher-cold-build-20261003-r2'
    assert bridge['work_passed'] and bridge['owned_tasks_retired'] and bridge['supervisor_complete']
    assert bridge['executed_commands'] == bridge['planned_commands'] == 30
    assert bridge['original_receipts_sha256'] == digest(payload['00-receipts.json'])
    assert ticket['supervisor_receipt_sha256'] == digest(payload['01-lane-retirement.json'])
    before = load('33-input-before.json')
    after = load('34-input-after.json')
    assert after['input_before_sha256'] == digest(payload['33-input-before.json'])
    assert after['input_files'] == 7118
    assert not after['guest_execution_accepted'] and not after['product_three_tu_abi_qualified']
    compact_stages = []
    for receipt, command in zip(receipts, commands):
        assert receipt['label'] == command[0] and receipt['argv'] == command[1]
        assert receipt['returncode'] == 0 and receipt['passed']
        assert receipt['command_file_sha256'] == 'faf1380a382ec923a5731315cffaa5559e14d44757961acf14d0153fbeba7298'
        assert receipt['supervisor_sha256'] == '16ade4cecc5b757f0b791d9129b1cc1d335871b77955df6ffe147967415c76df'
        assert receipt['memory_max_bytes'] == 68719476736 and receipt['swap_max_bytes'] == 0
        assert receipt['aggregate_owned_rss_budget_bytes'] == 8589934592
        assert receipt['remaining_roster'] == [10154]
        assert receipt['memory_events_before'] == receipt['memory_events_after']
        assert receipt['Popen_root_pidfd_retirement']['pidfd_readable']
        assert receipt['Popen_root_pidfd_retirement']['actual_reaped_returncode'] == 0
        # A direct compiler can reap its own linker. The original supervisor's
        # PIDFD still proves retirement; its null wait status is not invented.
        assert all(x['pidfd_readable'] and x['reaped_returncode'] in (0, None) for x in receipt['retirement'])
        compact_stages.append({k: receipt[k] for k in ('label', 'argv_sha256', 'returncode', 'wall_seconds', 'aggregate_owned_rss_peak_bytes', 'log_sha256', 'Popen_root_pidfd_retirement', 'retirement')})

    variants = []
    for index, label, objindex, binindex in ((9, 'uwvm2-ros-eh', 37, 38), (16, 'uwvm2-ros-noeh', 41, 42), (23, 'uwvm2-eh', 45, 46), (30, 'uwvm2-noeh', 49, 50)):
        obj = load(f'{objindex:02d}-{label}-object.json')
        binary = load(f'{binindex:02d}-{label}-binary.json')
        disassembly_name = f'{index:02d}-{label}-bridge-disassembly.log'
        coff_name = f'{index+1:02d}-{label}-coff-relocations.log'
        asm = payload[disassembly_name].decode()
        coff = payload[coff_name].decode()
        assert 'subq\t$0x28, %rsp' in asm and 'movl\t%eax, %ecx' in asm and 'ud2' in asm
        assert re.search(r'0x5 IMAGE_REL_AMD64_REL32 _ZN12_GLOBAL__N_13runEiPPw \(\d+\)', coff)
        assert re.search(r'0xD IMAGE_REL_AMD64_REL32 __imp_ExitProcess \(\d+\)', coff)
        assert re.search(r'Name: _ZN12_GLOBAL__N_13runEiPPw\s+Value: 32\s+Section: \.text \(1\)', coff)
        assert obj['coff_machine'] == binary['pe_machine'] == 34404 and binary['pe_optional_magic'] == 523
        assert binary['GNU_unwind_runtime_rejected'] and binary['SDK_static_runtime_provider_bound']
        assert len(binary['actual_DLL_import_names']) == 11
        assert not binary['guest_execution_accepted'] and not binary['product_three_tu_abi_qualified']
        assert not binary['DLL_runtime_closure_qualified']
        assert any(p.endswith('/libclang_rt.builtins.a') for p in binary['actual_reading_inputs'])
        assert any(p.endswith('/libunwind.a') for p in binary['actual_reading_inputs'])
        assert not any('libgcc' in p.lower() or 'libstdc++' in p.lower() for p in binary['actual_reading_inputs'])
        variants.append({
            'label': label, 'actual_object': obj['object'], 'actual_PE': binary['binary'],
            'actual_MD_dependencies': len(obj['actual_dependencies']),
            'actual_archive_member_count': len(binary['actual_archive_members']),
            'actual_link_reading_inputs': binary['actual_reading_inputs'],
            'actual_static_provider_records': binary['providers'],
            'actual_DLL_import_names': binary['actual_DLL_import_names'],
            'wmain': {'stack_reservation_bytes': 40, 'call_relocation_offset': 5, 'callee': '_ZN12_GLOBAL__N_13runEiPPw', 'callee_definition_offset': 32, 'exit_relocation_offset': 13, 'exit_import': '__imp_ExitProcess', 'disassembly_evidence': manifest['files'][disassembly_name], 'COFF_evidence': manifest['files'][coff_name]},
            'compile_warning': 'Clang reports link-only runtime/stdlib flags unused at -c; actual subsequent link Reading inputs bind compiler-rt and SDK libunwind.'
        })
    report = {
        'schema': 'uwvm-windows-fixed30-r2-attempt2-actual-static-report-v1',
        'result': 'PASS: four Windows SDK standalone regular-process launcher compile/link/COFF/PE cells',
        'source_input': load('57-source-archive-record.json'),
        'source_recipe_manifest_sha256': digest(payload['53-manifest.json']),
        'setup_recipe_manifest_sha256': digest(payload['58-manifest.json']),
        'execution_plan_sha256': digest(payload['35-plan.json']),
        'commands_sha256': digest(payload['36-commands.json']),
        'supervisor_sha256': digest(payload['54-supervisor.proposal.py']),
        'closure_sha256': digest(payload['55-windows_raii_regular_launcher_r2_closure.py']),
        'checked_wrapper_sha256': digest(payload['59-lane_launch.py']),
        'actual_input_before_sha256': digest(payload['33-input-before.json']),
        'actual_input_after_sha256': digest(payload['34-input-after.json']),
        'actual_input_file_count': after['input_files'],
        'qualification': {'actual_crosscompile_and_static_link': True, 'actual_fresh_MD_and_COFF_PE_inspection': True, 'SDK_static_compiler_rt_libunwind_provider': True, 'actual_guest_execution': False, 'actual_guest_DLL_runtime_closure': False, 'native_trap_TF_GPR_NI': False, 'fresh_product_three_TU': False, 'ROS_vendored_self_contained_product': False, 'true_named_module': False, 'whole_Wasm3_or_checkpoint_restore': False},
        'limits': {'memory_max': 68719476736, 'swap_max': 0, 'owned_RSS_soft': 8589934592, 'stage_file_limit': 536870912, 'stage_log_limit': 33554432, 'suite_deadline_seconds': 600, 'CPU_pool': list(range(16, 32))},
        'actual_peak_owned_RSS_bytes': max(s['aggregate_owned_rss_peak_bytes'] for s in receipts),
        'actual_wall_seconds_sum': sum(s['wall_seconds'] for s in receipts),
        'original_stages': compact_stages,
        'actual_variants': variants,
        'original_retirement_bridge': bridge,
        'original_retired_ticket': ticket,
        'child_wait_status_scope': 'Two actual linker descendants have readable original PIDFD and null outer wait status because their compiler parent reaped them. All thirty original direct Popen children have actual wait4 status zero; null descendant status remains null.',
        'raw_evidence': {'archive_path': str(archive), 'archive_bytes': archive.stat().st_size, 'archive_sha256': digest(archive.read_bytes()), 'manifest_path': str(packet / 'manifest.json'), 'manifest_sha256': digest(manifest_bytes), 'original_payloads': 62, 'all_payload_size_SHA_checked_independently': True, 'large_input_before_kept_compressed_only': True},
        'previous_failure_preserved': 'test/0022.qemu_platforms/windows_fixed30_r2_attempt1_actual_input_failure_20261003.json',
        'next_acceptance': 'Fresh Windows guest with matching four PE images, nonbreakaway owned Job, suspended-child admission before resume, regular output files, full 32-bit child status and real kernel trap/GPR retirement. Static wmain inspection is separate from a native trap bridge.',
    }
    data = (json.dumps(report, ensure_ascii=False, indent=2) + '\n').encode()
    for repo in ('uwvm2', 'uwvm2-ros'):
        out = Path('/Users/liyinan/Documents/MacroModel/src') / repo / 'test/0022.qemu_platforms/windows_fixed30_r2_attempt2_four_static_actual_20261003.json'
        with out.open('xb') as f:
            f.write(data)
        assert out.read_bytes() == data
        print(out, len(data), digest(data))


if __name__ == '__main__':
    main()
