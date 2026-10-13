#!/usr/bin/env python3
"""Record that every live QEMU thread is in the bounded Linux test scope."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re


EXPECTED_CPUS = sorted([0, 2, 4, 6, *range(16, 32)])


def cpuset(value: str) -> list[int]:
    result: set[int] = set()
    for part in value.strip().split(','):
        if '-' in part:
            first, last = (int(x) for x in part.split('-', 1))
            if first > last:
                raise ValueError('reversed cpuset range')
            result.update(range(first, last + 1))
        elif part:
            result.add(int(part))
    return sorted(result)


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu-pid', type=int, required=True)
    parser.add_argument('--linux-scope', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--source-id')
    parser.add_argument('--product', type=Path)
    parser.add_argument('--expected-product-sha256')
    parser.add_argument('--label')
    args = parser.parse_args()
    if not re.fullmatch(r'docker-[0-9a-f]{64}\.scope', args.linux_scope):
        raise ValueError('expected the exact Linux test Docker scope')
    if args.qemu_pid <= 1:
        raise ValueError('invalid QEMU PID')
    if args.source_id is not None and not re.fullmatch(r'sha256:[0-9a-f]{64}', args.source_id):
        raise ValueError('invalid frozen source ID')
    if args.expected_product_sha256 is not None and args.product is None:
        raise ValueError('an expected product hash requires --product')
    if args.label is not None and not re.fullmatch(r'[A-Za-z0-9_.-]{1,80}', args.label):
        raise ValueError('invalid evidence label')
    base = Path('/sys/fs/cgroup/system.slice') / args.linux_scope
    memory_max = (base / 'memory.max').read_text().strip()
    swap_max = (base / 'memory.swap.max').read_text().strip()
    cpus = cpuset((base / 'cpuset.cpus.effective').read_text())
    if memory_max != '68719476736' or swap_max != '0' or cpus != EXPECTED_CPUS:
        raise ValueError('Linux test scope does not have the qualified limits')
    process = Path('/proc') / str(args.qemu_pid)
    cmdline = (process / 'cmdline').read_bytes()
    command = [os.fsdecode(value) for value in cmdline.split(b'\0') if value]
    qemu_images = [Path(value).resolve(strict=True) for value in command
                   if Path(value).name.startswith('qemu-system-x86_64')]
    if len(qemu_images) != 1:
        raise ValueError('PID is not an x86-64 QEMU system emulator')
    qemu_image = qemu_images[0]
    threads = sorted(int(path.name) for path in (process / 'task').iterdir()
                     if path.name.isdecimal())
    if len(threads) < 2:
        raise ValueError('QEMU thread inventory is incomplete')
    members = {int(value) for value in (base / 'cgroup.threads').read_text().splitlines()}
    expected_path = f'0::/system.slice/{args.linux_scope}'
    for thread in threads:
        if thread not in members:
            raise ValueError(f'QEMU thread {thread} is outside the Linux test scope')
        cgroup = (process / 'task' / str(thread) / 'cgroup').read_text().strip()
        if cgroup != expected_path:
            raise ValueError(f'QEMU thread {thread} has unexpected cgroup {cgroup!r}')
        status = (process / 'task' / str(thread) / 'status').read_text()
        allowed = next((line.split(':', 1)[1].strip() for line in status.splitlines()
                        if line.startswith('Cpus_allowed_list:')), None)
        if allowed is None or not set(cpuset(allowed)).issubset(set(EXPECTED_CPUS)):
            raise ValueError(f'QEMU thread {thread} escaped the 20-CPU affinity')
    output = args.output.resolve()
    if not output.is_relative_to(Path('/tmp')) and not output.is_relative_to(Path('/dev/shm')):
        raise ValueError('VM cgroup evidence must be written to temporary storage')
    result = {
        'schema': 1,
        'date_utc': datetime.now(timezone.utc).isoformat(),
        'linux_test_scope': args.linux_scope,
        'memory_max': memory_max,
        'memory_swap_max': swap_max,
        'cpuset_cpus_effective': cpus,
        'memory_current': (base / 'memory.current').read_text().strip(),
        'qemu_pid': args.qemu_pid,
        'qemu_tids': threads,
        'qemu_executable': str(qemu_image),
        'qemu_executable_sha256': digest(qemu_image),
        'qemu_cmdline_sha256': hashlib.sha256(cmdline).hexdigest(),
    }
    if args.source_id is not None:
        result['source_id'] = args.source_id
    if args.label is not None:
        result['label'] = args.label
    if args.product is not None:
        product = args.product.resolve(strict=True)
        product_sha256 = digest(product)
        if args.expected_product_sha256 is not None and \
                product_sha256.lower() != args.expected_product_sha256.lower():
            raise ValueError('product hash differs from the cross-build certificate')
        result['product'] = str(product)
        result['product_sha256'] = product_sha256
    with output.open('x') as stream:
        json.dump(result, stream, indent=2, sort_keys=True)
        stream.write('\n')
    print(json.dumps(result, sort_keys=True))


if __name__ == '__main__':
    main()
