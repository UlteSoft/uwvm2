#!/usr/bin/env python3
"""Record the actual Linux boot, PMU environment and executable dependencies."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess

from build_products import admission, digest


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--arena', type=Path, required=True)
    p.add_argument('--recipes', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--products-root', type=Path,
                   help='explicit immutable UWVM product directory; defaults to arena/products')
    a = p.parse_args()
    products_root = a.products_root if a.products_root is not None else a.arena / 'products'
    data = {'admission': admission(), 'boot_id': Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
            'kernel': list(os.uname()), 'executables': {}, 'settings': {}, 'products_root': str(products_root)}
    paths = ['/sys/devices/cpu_core/cpus', '/sys/devices/cpu_atom/cpus',
             '/sys/devices/system/cpu/cpu21/cpufreq/scaling_governor',
             '/sys/devices/system/cpu/cpu21/cpufreq/scaling_min_freq',
             '/sys/devices/system/cpu/cpu21/cpufreq/scaling_max_freq',
             '/proc/sys/kernel/perf_event_paranoid', '/proc/sys/kernel/perf_event_max_sample_rate',
             '/proc/sys/kernel/perf_cpu_time_max_percent', '/proc/sys/kernel/nmi_watchdog']
    for name in paths:
        path = Path(name)
        data['settings'][name] = path.read_text().strip() if path.is_file() else None
    data['cpu_info_first'] = Path('/proc/cpuinfo').read_text().split('\n\n')[0]
    recipes = json.loads(a.recipes.read_text())
    binaries = {name: {'binary': str(products_root / name / 'uwvm')}
                for name in ['uwvm2', 'uwvm2-stable', 'uwvm2-ros']}
    binaries.update({name: recipes[name] for name in ['wasmtime', 'wavm', 'wasmedge']})
    binaries['vtune'] = {'binary': '/opt/intel/oneapi/vtune/latest/bin64/vtune'}
    for name, item in binaries.items():
        binary = Path(item['binary'])
        env = dict(os.environ, **item.get('environment', {}))
        result = subprocess.run(['ldd', str(binary)], env=env, capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, (name, result.returncode, result.stdout, result.stderr)
        deps = {}
        for line in result.stdout.splitlines():
            match = re.search(r'(?:=>\s*)?(/[^\s]+)\s+\(', line)
            if match:
                path = Path(match[1])
                deps[str(path)] = {'resolved_path': str(path.resolve(strict=True)), 'sha256': digest(path)}
        missing = re.findall(r'^\s*(\S+)\s+=>\s+not found\s*$', result.stdout, re.M)
        auxiliary_tags = {}
        if missing:
            # Intel's libstdc++ selector advertises optional AUXILIARY versions
            # and successfully loads the highest installed version. Missing
            # required DT_NEEDED libraries must still reject the environment.
            auxiliary = set()
            for path in deps:
                q = subprocess.run(['readelf', '-d', path], capture_output=True, text=True, timeout=30)
                names = re.findall(r'\(AUXILIARY\).*?\[([^\]]+)\]', q.stdout)
                if names:
                    auxiliary_tags[path] = names
                    auxiliary.update(names)
            assert result.returncode == 0 and set(missing) <= auxiliary, (name, missing)
        data['executables'][name] = {'path': str(binary), 'resolved_path': str(binary.resolve(strict=True)),
                                     'sha256': digest(binary), 'ldd_exit': result.returncode,
                                     'ldd_stdout': result.stdout, 'ldd_stderr': result.stderr,
                                     'dependencies': deps, 'unresolved_optional_auxiliary_names': missing,
                                     'auxiliary_dynamic_tags': auxiliary_tags}
        if name == 'vtune':
            version = subprocess.run([str(binary), '-version'], env=env, capture_output=True, text=True, timeout=30)
            assert version.returncode == 0, version.stderr
            data['executables'][name]['version_stdout'] = version.stdout
    a.out.write_text(json.dumps(data, indent=2) + '\n')


if __name__ == '__main__':
    main()
