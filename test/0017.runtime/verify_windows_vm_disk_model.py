#!/usr/bin/env python3
"""Reject a VM disk controller that cannot boot the qualified Windows image."""

import argparse
import hashlib
import json
from pathlib import Path
import shlex


def sha256(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def disk_model(script: Path) -> dict[str, str]:
    tokens = shlex.split(script.read_text().replace('\\\n', ' '), comments=True)
    devices = [tokens[index + 1] for index, token in enumerate(tokens[:-1])
               if token == '-device']
    drives = [tokens[index + 1] for index, token in enumerate(tokens[:-1])
              if token == '-drive']
    if any(device.split(',', 1)[0] == 'nvme' for device in devices):
        raise ValueError(f'{script}: NVMe changes the Windows boot controller')
    controllers = [device for device in devices
                   if device.split(',', 1)[0] == 'virtio-scsi-pci']
    disks = [device for device in devices
             if device.split(',', 1)[0] == 'scsi-hd']
    if len(controllers) != 1 or len(disks) != 1:
        raise ValueError(f'{script}: expected one virtio-scsi-pci and one scsi-hd')
    def options(value: str) -> dict[str, str]:
        return dict(part.split('=', 1) for part in value.split(',')[1:] if '=' in part)
    controller = options(controllers[0]).get('id')
    disk = options(disks[0])
    if not controller or disk.get('bus') != controller + '.0' or not disk.get('drive'):
        raise ValueError(f'{script}: scsi-hd is not attached to its controller')
    if sum(options(drive).get('id') == disk['drive'] for drive in drives) != 1:
        raise ValueError(f'{script}: scsi-hd does not have exactly one backing drive')
    return {'controller': 'virtio-scsi-pci', 'controller_id': controller,
            'disk': 'scsi-hd', 'drive_id': disk['drive']}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference-launcher', type=Path, required=True)
    parser.add_argument('--candidate-launcher', type=Path, required=True)
    parser.add_argument('--guest-overlay', type=Path, required=True)
    parser.add_argument('--expected-overlay-sha256', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(args.output)
    if len(args.expected_overlay_sha256) != 64 or any(
            ch not in '0123456789abcdef' for ch in args.expected_overlay_sha256):
        raise ValueError('expected a lowercase SHA-256 hex digest')
    reference = disk_model(args.reference_launcher)
    candidate = disk_model(args.candidate_launcher)
    if candidate != reference:
        raise ValueError('candidate disk topology differs from the reference launcher')
    overlay_sha256 = sha256(args.guest_overlay)
    if overlay_sha256 != args.expected_overlay_sha256:
        raise ValueError('guest overlay differs from the frozen template')
    evidence = {'schema': 1, 'disk_model': candidate,
                'reference_launcher': str(args.reference_launcher.resolve()),
                'reference_launcher_sha256': sha256(args.reference_launcher),
                'candidate_launcher': str(args.candidate_launcher.resolve()),
                'candidate_launcher_sha256': sha256(args.candidate_launcher),
                'guest_overlay': str(args.guest_overlay.resolve()),
                'guest_overlay_sha256': overlay_sha256}
    args.output.write_text(json.dumps(evidence, indent=2, sort_keys=True) + '\n')
    print(json.dumps(evidence, sort_keys=True))


if __name__ == '__main__':
    main()
