#!/usr/bin/env python3
"""Check registered target flags, paired ELF gates and installed tools in CG.

These are capability observations, not Wasm execution or codegen passes.
"""
import argparse
import json
from pathlib import Path
import subprocess
from architecture_catalog import source_inventory
from run_matrix import digest, execute, save, verify


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('config', type=Path)
    p.add_argument('output', type=Path)
    a = p.parse_args()
    config = json.loads(a.config.read_text())
    subprocess.run(['/usr/bin/bash', config['cgroup_guard']], check=True)
    verify(config['pins'])
    out = a.output
    out.mkdir(exist_ok=False)
    inventory = source_inventory(Path(config['llvm_source']))
    registrations = {r['arch']: r['has_jit'] for r in inventory['target_registrations']}
    results = {}
    # Backend names and triple enum names occasionally differ (x86 vs x86,
    # systemz vs s390x). The generator binds the original registration names.
    expected = config['registered_target_names']
    for repo, product in config['products'].items():
        log = out / (repo + '.log')
        row = execute([product], log, out, 30)
        assert row['exit'] == 0, row
        advertised, gates = {}, {}
        for line in log.read_text().splitlines():
            fields = line.split('\t')
            if fields[0] == 'registration':
                assert len(fields) == 3 and fields[1] not in advertised, fields
                advertised[fields[1]] = fields[2] == '1'
            elif fields[0] == 'gate':
                assert len(fields) == 5 and fields[1] not in gates, fields
                gates[fields[1]] = dict(arch_enum=int(fields[2]), object_format_enum=int(fields[3]),
                                      format_gate=fields[4] == '1')
        assert advertised == expected, (repo, advertised, expected)
        assert set(gates) == {r['id'] for r in config['linux_profiles']}
        for name in ('loongarch32', 'riscv32be', 'riscv64be', 'sparc32', 'sparc32le', 'hexagon', 'm68k'):
            assert not gates[name]['format_gate'], (repo, name)
        for name in ('x86_64', 'i686', 'armv5-soft', 'armv7-hard', 'aarch64', 'loongarch64',
                     's390x', 'riscv32', 'riscv64', 'mips32-r2', 'mips32el-r2', 'mips64', 'mips64el'):
            assert gates[name]['format_gate'], (repo, name)
        assert not gates['aarch64-ilp32']['format_gate'], (repo, gates['aarch64-ilp32'])
        results[repo] = dict(registrations=advertised, gates=gates,
                             product_sha256=digest(product), execution=row)
    tools = []
    for name, argv in config['tool_commands']:
        log = out / (name + '.log')
        row = execute(argv, log, out, 30)
        if name.endswith('-cpus'):
            # QEMU user-mode CPU help can deliberately exit 1 after printing
            # its list. Keep the actual status/output; this is an inventory
            # observation and grants no guest or CPU-feature qualification.
            assert row['exit'] in (0, 1) and log.stat().st_size > 0, row
            row['cpu_help_observation_only'] = True
        else:
            assert row['exit'] == 0, row
        tools.append(dict(name=name, **row))
    verify(config['pins'])
    save(out / 'summary.json', dict(capability_probe_passed=True,
        schema='uwvm-linux-target-capability-census-v1', source_inventory=inventory,
        registered_architectures=registrations, paired_results=results, tools=tools,
        config_sha256=digest(a.config), qualified_guest_runs=0,
        guest_semantics_qualified=False, codegen_qualified=False,
        scope='Actual TargetInfo registrations and paired native ELF selection gates; QEMU/tool version and CPU inventories; no foreign Wasm execution'))
    print('PASS target capability census; guest execution remains unqualified', flush=True)


if __name__ == '__main__':
    main()
