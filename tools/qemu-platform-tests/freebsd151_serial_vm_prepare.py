#!/usr/bin/env python3
"""Finite FreeBSD SDK serial-guest binder proposal; invoke no native tool.

The clean-image checksum is bound to the original approved ONE actual receipt.
The reviewed guardian owns the separate fixed qemu-img, genisoimage and QEMU
commands and all retirement.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import stat
import sys

B = Path('/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924')
SOURCE = B / 'qemu-platform-tests/freebsd-serial-vm-source-20261003-r2'
RUN = B / 'evidence/qemu-freebsd151-sdk-oracle-vm-20261003-r2'
COLD = B / 'qemu-platform-tests/freebsd-sdk-oracle-cold-20261003-r2-planfix/cold-qualified-r2.json'
COLD_SHA = 'e6a0f31823819ca265eab8b206e7778fae77627198ac93d1146e6ef145f92d97'
CLEAN_RECORD = B / 'qemu-platform-tests/freebsd-clean-image-actual-20261003-r2/clean-image-actual.json'
# The original clean-image R2 task completed and retired before this source
# freeze. Callers cannot substitute its checksum through arguments/environment.
CLEAN_RECORD_SHA = 'b6b1f9841cf01db1deeed0041f8e74fc8dd24c4205ff106db37c604a4e4ac7d8'
CLEAN = CLEAN_RECORD.parent / 'FreeBSD-15.1-clean.qcow2'
Q = Path('/home/macromodel/.local/opt/qemu-10.2.1/usr')
QEMU = Q / 'bin/qemu-system-x86_64'
BIOS = Q / 'share/seabios/bios-256k.bin'
PROVIDERS = SOURCE / 'host-providers-source-static.json'
PROVIDERS_SHA = 'f3b7f80dd230c20c2a4c96f825ca62d253b0326f39ca6e846868fe72e7eecce5'
SEED = SOURCE / 'freebsd151_serial_seed_r2.py'
SEED_SHA = 'ed4c2a275a3d8e4194dc6977badf920459ff531b92092961a5e792847b7919ee'
CONTROLLER = SOURCE / 'freebsd151_serial_controller.py'
CONTROLLER_SHA = '4991b6c5a04f5f7a9d89a100d9aa075367033d9e784ba883af9b86f47fe105cc'
NONCE = '1484804590f0b302f8e90716dc0bf4f4e1afac358472e77e4b5af191e8724b43'
IPC = Path('/tmp/uwvm-bsd151-' + NONCE[:24])
BOOT = '83f582ec-ff7f-41ce-9631-2c0fbb865aeb'
CG_NAME = '/system.slice/docker-bec3a6e4e013ba07c1583e46eeb81707c90c59375ae1c208a6bb87cc4f773f45.scope'
CG = Path('/sys/fs/cgroup') / CG_NAME.lstrip('/')
LABELS = ('uwvm2-ros-eh', 'uwvm2-ros-noeh', 'uwvm2-eh', 'uwvm2-noeh')
MAX_OVERLAY = 512 << 20
MAX_ISO = 16 << 20
RESERVE = 8 << 30


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def identity(path, *, private=False):
    path = Path(path)
    before = path.lstat()
    if (not stat.S_ISREG(before.st_mode) or path.resolve(strict=True) != path
            or before.st_uid not in (0, 1000) or before.st_nlink < 1):
        raise RuntimeError('resolved root/UID1000 regular input required: ' + str(path))
    if private and (before.st_uid != 1000 or before.st_nlink != 1 or stat.S_IMODE(before.st_mode) & 0o077):
        raise RuntimeError('owned single-link private regular input required: ' + str(path))
    return {'device': before.st_dev, 'inode': before.st_ino, 'uid': before.st_uid,
            'gid': before.st_gid, 'links': before.st_nlink, 'mode': stat.S_IMODE(before.st_mode),
            'bytes': before.st_size, 'mtime_ns': before.st_mtime_ns, 'ctime_ns': before.st_ctime_ns}


def bound(path, expected, *, maximum=64 << 10):
    before = identity(path)
    if not 1 <= before['bytes'] <= maximum or sha(path) != expected or identity(path) != before:
        raise RuntimeError('fixed bounded source/metadata checksum changed: ' + str(path))
    return before


def new_file(path, data, mode=0o600):
    with path.open('xb') as stream:
        stream.write(data)
    path.chmod(mode)


def admission():
    if (sys.platform != 'linux' or os.getuid() != 1000 or os.geteuid() != 1000
            or Path('/proc/sys/kernel/random/boot_id').read_text().strip() != BOOT
            or Path('/proc/self/cgroup').read_text().strip() != '0::' + CG_NAME
            or not os.sched_getaffinity(0) or not os.sched_getaffinity(0) <= set(range(16, 32))):
        raise RuntimeError('original owned Linux cgroup/E-core admission required')
    for name, expected in (('memory.max', str(64 << 30)), ('memory.swap.max', '0'),
                           ('cpuset.cpus.effective', '0,2,4,6,16-31')):
        if (CG / name).read_text().strip() != expected:
            raise RuntimeError('original 64GiB/swap0/cpuset authority changed')
    if len(CLEAN_RECORD_SHA) != 64 or any(c not in '0123456789abcdef' for c in CLEAN_RECORD_SHA):
        raise RuntimeError('source draft awaits actual original-guard clean-image checksum')
    os.umask(0o077)


def common_inputs():
    bound(COLD, COLD_SHA)
    bound(SEED, SEED_SHA)
    bound(CONTROLLER, CONTROLLER_SHA)
    bound(PROVIDERS, PROVIDERS_SHA, maximum=128 << 10)
    bound(CLEAN_RECORD, CLEAN_RECORD_SHA)
    cold = json.loads(COLD.read_bytes())
    if (cold.get('schema') != 'uwvm-freebsd151-sdk-oracle-cross-build-actual-v1'
            or cold.get('cold_object_and_provider_bindings_verified') is not True
            or cold.get('actual_guest_executed') is not False
            or cold.get('ROS_LLVM23_product_qualified') is not False
            or [row.get('label') for row in cold.get('variants', [])] != list(LABELS)):
        raise RuntimeError('exact four original cross-build profiles required')
    clean = json.loads(CLEAN_RECORD.read_bytes())
    image = clean.get('clean_image', {})
    if (clean.get('schema') != 'uwvm-freebsd151-clean-image-actual-v1'
            or clean.get('original_archive', {}).get('sha256') != 'e4ca4db889f8559c9b9dfcacc70405c038476f4b6d41649b152d3809a2ed9e1f'
            or clean.get('original_archive', {}).get('bytes') != 664729340
            or image.get('path') != str(CLEAN) or image.get('bytes') != 2671443968
            or image.get('backing_file_offset') != 0 or image.get('backing_file_bytes') != 0
            or clean.get('actual_guest_executed') is not False
            or clean.get('original_7GB_customized_guest_disk_used') is not False):
        raise RuntimeError('original bounded official clean-image provenance required')
    current = identity(CLEAN)
    if (any(current[key] != image.get(key) for key in ('device', 'inode', 'bytes', 'mtime_ns', 'ctime_ns', 'mode', 'uid', 'links'))
            or current['uid'] != 1000 or current['links'] != 1 or current['mode'] != 0o444
            or not isinstance(image.get('sha256'), str) or len(image['sha256']) != 64
            or sha(CLEAN) != image['sha256'] or identity(CLEAN) != current):
        raise RuntimeError('actual single-link read-only official clean base changed')
    providers = json.loads(PROVIDERS.read_bytes())
    if (providers.get('schema') != 'uwvm-qemu-iso-host-providers-source-static-v1'
            or providers.get('compiler_or_native_tool_executed') is not False
            or providers.get('runtime_loader_execution_qualified') is not False
            or not 1 <= len(providers.get('files', [])) <= 128):
        raise RuntimeError('fixed source-only provider inventory required')
    pins = {}
    for row in [*providers['files'], *providers['firmware']]:
        path = Path(row['path'])
        actual = identity(path)
        if (actual['bytes'] != row['bytes'] or actual['device'] != row['device']
                or actual['inode'] != row['inode'] or actual['uid'] != row['uid']
                or not 1 <= actual['bytes'] <= 2 << 30
                or sha(path) != row['sha256'] or identity(path) != actual):
            raise RuntimeError('actual host provider inventory changed: ' + str(path))
        record = {'identity': actual, 'bytes': actual['bytes'], 'sha256': row['sha256']}
        if str(path) in pins and pins[str(path)] != record:
            raise RuntimeError('provider contexts disagree about one physical input')
        pins[str(path)] = record
    for path in (COLD, CLEAN_RECORD, CLEAN, SEED, CONTROLLER, PROVIDERS):
        actual = identity(path)
        pins[str(path)] = {'identity': actual, 'bytes': actual['bytes'], 'sha256': sha(path)}
    return cold, clean, pins


def private_directory(path):
    before = path.lstat()
    if (not stat.S_ISDIR(before.st_mode) or path.resolve(strict=True) != path
            or before.st_uid != 1000 or stat.S_IMODE(before.st_mode) != 0o700):
        raise RuntimeError('actual UID1000 0700 private directory required')
    return (before.st_dev, before.st_ino, before.st_uid, stat.S_IMODE(before.st_mode))


def import_source(path, name):
    sys.dont_write_bytecode = True
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def init(cold, clean, pins):
    free = os.statvfs(B / 'evidence')
    if free.f_bavail * free.f_frsize < RESERVE + MAX_OVERLAY + MAX_ISO + (72 << 20):
        raise RuntimeError('8GiB reserve plus disposable overlay/ISO/seed/log budget required')
    if RUN.exists() or RUN.is_symlink() or IPC.exists() or IPC.is_symlink():
        raise RuntimeError('one new disposable run and nonce-private QMP directory required')
    RUN.mkdir(mode=0o700); IPC.mkdir(mode=0o700)
    new_file(RUN / 'cold-qualified.json', COLD.read_bytes())
    seed = import_source(SEED, 'uwvm_fixed_freebsd_seed')
    previous = sys.argv
    try:
        sys.argv = [str(SEED), '--qualified-inputs', str(RUN / 'cold-qualified.json'),
                    '--nonce', NONCE, '--output', str(RUN / 'isofs')]
        seed.main()
    finally:
        sys.argv = previous
    new_file(RUN / 'serial-seed-prepared.json', (RUN / 'isofs/serial-seed-prepared.json').read_bytes())
    metadata = {'schema': 'uwvm-freebsd151-serial-vm-prepared-before-v1', 'nonce': NONCE,
                'cold_sha256': COLD_SHA, 'clean_record_sha256': CLEAN_RECORD_SHA,
                'clean_image_sha256': clean['clean_image']['sha256'],
                'source_provider_inventory_sha256': PROVIDERS_SHA,
                'seed_source_sha256': SEED_SHA, 'controller_source_sha256': CONTROLLER_SHA,
                'actual_guest_executed': False, 'immutable_inputs_before': pins}
    new_file(RUN / 'prepared-before.json', (json.dumps(metadata, indent=2, sort_keys=True) + '\n').encode())
    print(json.dumps({'prepared_four_profile_seed': True, 'actual_guest_executed': False,
                      'nonce': NONCE, 'seed_sha256': sha(RUN / 'serial-seed-prepared.json')}))


def bind(cold, clean, pins):
    private_directory(RUN); private_directory(IPC)
    before_raw = (RUN / 'prepared-before.json').read_bytes()
    if not 1 <= len(before_raw) <= 128 << 10:
        raise RuntimeError('bounded actual prepared-before record required')
    before = json.loads(before_raw)
    if (before.get('schema') != 'uwvm-freebsd151-serial-vm-prepared-before-v1'
            or before.get('nonce') != NONCE or before.get('cold_sha256') != COLD_SHA
            or before.get('clean_record_sha256') != CLEAN_RECORD_SHA
            or before.get('immutable_inputs_before') != pins
            or sha(RUN / 'cold-qualified.json') != COLD_SHA):
        raise RuntimeError('source/provider/base changed across separate native preparation stages')
    seed = json.loads((RUN / 'serial-seed-prepared.json').read_bytes())
    if (seed.get('nonce') != NONCE or seed.get('actual_source_inputs_sha256') != COLD_SHA
            or seed.get('required_guest_runtime') != cold['required_guest_runtime']
            or seed.get('required_guest_runtime_sha256') != cold['required_guest_runtime_sha256']):
        raise RuntimeError('runtime-verifying exact seed changed')
    iso = RUN / 'inputs.iso'; overlay = RUN / 'disk.qcow2'
    for path, limit in ((iso, MAX_ISO), (overlay, MAX_OVERLAY)):
        value = identity(path)
        if value['uid'] != 1000 or value['links'] != 1 or not 1 <= value['bytes'] <= limit:
            raise RuntimeError('new single-link finite native output required')
    # Each native tool created its file inside the already private run root.
    # Restrict the overlay before the controller opens it, and make the seed
    # immutable to the guest. Neither action touches the original clean base.
    overlay.chmod(0o600); iso.chmod(0o444)
    controller = import_source(CONTROLLER, 'uwvm_fixed_freebsd_controller')
    backing = controller.actual_overlay_backing(overlay)
    overlay_before = identity(overlay, private=True)
    for path in (RUN / 'cold-qualified.json', RUN / 'serial-seed-prepared.json', iso):
        actual = identity(path)
        pins[str(path)] = {'identity': actual, 'bytes': actual['bytes'], 'sha256': sha(path)}
    manifest = {'schema': 'uwvm-freebsd151-sdk-oracle-serial-vm-bound-v1',
                'run_root': str(RUN), 'ipc_root': str(IPC), 'nonce': NONCE,
                'controller_sha256': CONTROLLER_SHA, 'deadline_seconds': 900,
                'actual_cold_ELF_and_providers_bound': True, 'actual_guest_executed': False,
                'ROS_LLVM23_product_qualified': False, 'immutable_inputs': pins,
                'overlay_before_identity': overlay_before, 'overlay_before_sha256': sha(overlay),
                'overlay_has_only_bound_readonly_base': True, 'overlay_maximum_bytes': MAX_OVERLAY,
                'actual_overlay_backing': backing,
                'prepared_before_sha256': hashlib.sha256(before_raw).hexdigest(),
                'source_provider_inventory_is_not_loader_execution_qualification': True}
    new_file(RUN / 'vm-manifest.json', (json.dumps(manifest, indent=2, sort_keys=True) + '\n').encode())
    print(json.dumps({'exact_four_profile_VM_manifest_sha256': sha(RUN / 'vm-manifest.json'),
                      'actual_guest_executed': False, 'ROS_bundled_LLVM23_product_qualified': False}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('phase', choices=('init', 'bind'))
    args = parser.parse_args()
    admission()
    cold, clean, pins = common_inputs()
    if args.phase == 'init':
        init(cold, clean, pins)
    else:
        bind(cold, clean, pins)


if __name__ == '__main__':
    main()
