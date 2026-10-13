#!/usr/bin/env python3
"""Compile original frozen TargetInfo sources and run the capability census."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess
from architecture_catalog import source_inventory, LINUX_PROFILES
from run_matrix import digest, execute, save, verify


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('config', type=Path)
    a = p.parse_args()
    config = json.loads(a.config.read_text())
    config_hash = digest(a.config)
    subprocess.run(['/usr/bin/bash', config['cgroup_guard']], check=True)
    verify(config['pins'])
    source_manifest = json.loads(Path(config['source_manifest']).read_text())
    source_pins = {str(Path(config['source_root']) / name): value
                   for name, value in source_manifest['files'].items()}
    verify(source_pins)
    out = Path(config['output'])
    out.mkdir(exist_ok=False)
    root = Path(config['llvm_source'])
    inventory = source_inventory(root)
    families = {row['family'] for row in inventory['target_registrations'] if row['has_jit']}
    # Some non-host backends append generated instruction tables to TargetInfo
    # (WebAssembly). They remain covered by the complete source inventory;
    # this executable registers every family advertising JIT, including BPF.
    files = sorted(path for path in (root / 'lib/Target').glob('*/TargetInfo/*TargetInfo.cpp')
                   if path.parts[-3] in families)
    initializers = []
    for path in files:
        matches = re.findall(r'\b(LLVMInitialize\w+TargetInfo)\s*\(\s*\)', path.read_text())
        assert len(matches) == 1, (path, matches)
        initializers += matches
    header = out / 'target_census_generated.h'
    header.write_text('\n'.join(
        ['#pragma once'] + [f'extern "C" void {name}();' for name in initializers] +
        ['inline void initialize_census_target_infos() {'] +
        [f'    {name}();' for name in initializers] + ['}',
         'struct census_profile { char const* name; char const* triple; };',
         'inline constexpr census_profile census_profiles[] {'] +
        ['    {' + json.dumps(row['id']) + ', ' + json.dumps(row['triple']) + '},'
         for row in LINUX_PROFILES] + ['};', '']))
    amalgamated = out / 'all_target_infos.cc'
    amalgamated.write_text('\n'.join('#include ' + json.dumps(str(path)) for path in files) +
                          '\n#include ' + json.dumps(config['census_source']) + '\n')
    products, dependencies, commands = {}, {}, []
    for repo in ('uwvm2', 'uwvm2-ros'):
        product = out / (repo + '-target-census')
        dep = out / (repo + '.d')
        argv = [*config['build_prefixes'][repo],
                *['-I' + str(path.parent.parent) for path in files],
                '-I' + str(out), '-MD', '-MF', str(dep), str(amalgamated),
                *config['link_flags'], '-O1', '-g0', '-o', str(product)]
        row = execute(argv, out / (repo + '-build.log'), out, 180)
        commands.append(row)
        assert row['exit'] == 0, row
        text = dep.read_text().replace('\\\n', ' ')
        for name in shlex.split(text.split(':', 1)[1]):
            path = Path(name)
            dependencies[str(path)] = digest(path)
        products[repo] = str(product)
    execution = dict(cgroup_guard=config['cgroup_guard'], llvm_source=str(root), products=products,
        pins={**config['pins'], **dependencies, **{path: digest(path) for path in products.values()}},
        registered_target_names={row['registered_name']: row['has_jit']
                                 for row in inventory['target_registrations'] + inventory['named_alias_registrations']
                                 if row['family'] in families},
        linux_profiles=LINUX_PROFILES, tool_commands=config['tool_commands'])
    runtime_config = out / 'execution-config.json'
    save(runtime_config, execution)
    row = execute(['/usr/bin/python3', config['census_runner'], str(runtime_config), str(out / 'results')],
                  out / 'census.log', out, 600)
    assert row['exit'] == 0, row
    verify(source_pins)
    verify(config['pins'])
    verify(dependencies)
    assert digest(a.config) == config_hash
    summary = json.loads((out / 'results/summary.json').read_text())
    assert summary['capability_probe_passed'] and summary['qualified_guest_runs'] == 0
    save(out / 'build-qualification.json', dict(capability_probe_passed=True,
        source_identities=source_manifest['identities'], original_configuration_sha256=config_hash,
        commands=commands, actual_postbuild_dependency_pins=dependencies,
        summary_sha256=digest(out / 'results/summary.json'), qualified_guest_runs=0,
        scope='Native capability census, not foreign VM codegen or semantics; host-dependent preprocessor branches require native guest builds'))
    print('PASS paired target census builds; no foreign guest qualification', flush=True)


if __name__ == '__main__':
    main()
