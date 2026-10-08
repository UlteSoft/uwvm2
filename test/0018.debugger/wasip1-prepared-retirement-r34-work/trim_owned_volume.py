from pathlib import Path
import fcntl
import hashlib
import json
import os
import resource
import select
import subprocess
import time
import stat
import atexit

E = Path('/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17')
D = E / 'rounds/wasip1-prepared-retirement-20261008-r34'
CG = Path('/sys/fs/cgroup/system.slice/docker-d3b12e6d323a0f28c2408989fff41a1ca1a991e19945919bc870337dd0afae03.scope')
expected = '0::/' + str(CG).removeprefix('/sys/fs/cgroup/')
policy = json.loads((E / 'storage-policy.json').read_text())
assert Path('/proc/sys/kernel/random/boot_id').read_text().strip() == 'c8d3550f-3a19-40d7-8507-a76c2045ace5'
assert (CG / 'memory.max').read_text().strip() == '68719476736' and (CG / 'memory.swap.max').read_text().strip() == '0'
assert Path('/proc/11166/stat').read_text().split(')')[-1].split()[19] == '22661'
assert Path('/sys/class/block/loop31/loop/backing_file').read_text().strip() == policy['volume_image_path']
image = Path(policy['volume_image_path'])
assert image.stat().st_uid == 1000 and image.stat().st_size == policy['volume_capacity_bytes'] == 8 << 30
assert os.major(E.stat().st_dev) == 7 and os.minor(E.stat().st_dev) == 31
lease = os.open(E / 'suite.lock', os.O_RDWR)
fcntl.flock(lease, fcntl.LOCK_EX)
os.sched_setaffinity(0, {16})
helper = ['docker', 'run', '--rm', '--cap-drop=ALL', '--cap-add=DAC_OVERRIDE', '--network=none',
    '--read-only', '--security-opt=no-new-privileges', '--cgroupns=host', '--pid=host',
    '--memory=33554432', '--memory-swap=33554432', '-v', str(CG / 'cgroup.procs') + ':/admit:rw',
    'debian:sid', 'sh', '-c', 'echo "$1" > /admit', 'sh', str(os.getpid())]
subprocess.run(helper, check=True, timeout=30)
assert Path('/proc/self/cgroup').read_text().strip() == expected
sha = lambda p: hashlib.file_digest(Path(p).open('rb'), 'sha256').hexdigest()
tool = Path('/usr/sbin/fstrim')
tool_hash = sha(tool)
identity = (image.stat().st_dev, image.stat().st_ino, image.stat().st_size, image.stat().st_uid)
excluded = {D / 'trim-own-ext4-qualified.json', D / 'trim-own-ext4-native.log', D / 'trim-own-helper-cid.json'}


def inventory():
    rows = {}
    hashes = {}
    for parent, dirs, names in os.walk(E, followlinks=False):
        links = [n for n in dirs if (Path(parent) / n).is_symlink()]
        dirs[:] = [n for n in dirs if n not in links]
        for name in names + links:
            p = Path(parent) / name
            if p in excluded:
                continue
            s = p.lstat()
            key = (s.st_dev, s.st_ino)
            if stat.S_ISREG(s.st_mode):
                if key not in hashes:
                    hashes[key] = sha(p)
                content = hashes[key]
            elif stat.S_ISLNK(s.st_mode):
                content = os.readlink(p)
            else:
                content = None  # Preserve inactive socket/FIFO metadata without opening it.
            rows[str(p.relative_to(E))] = (s.st_size, s.st_mode, s.st_uid, content)
    assert resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024 < 2 << 30
    return rows


before = inventory()
before_blocks = image.stat().st_blocks * 512
before_free = os.statvfs(E.parent).f_bavail * os.statvfs(E.parent).f_frsize
script = ('ulimit -v 65536; echo $$ > /admit; taskset -pc 16 $$ >/dev/null; '
    'test "$(cat /proc/self/cgroup)" = "' + expected + '" || exit 80; '
    'test "$(stat -c %d /own)" = "' + str(E.stat().st_dev) + '" || exit 81; '
    'echo TRIM_ROOT_PID=$$; kill -STOP $$; exec /trim --verbose --length 8589934592 /own')
create = ['docker', 'create', '--cap-drop=ALL', '--cap-add=DAC_OVERRIDE', '--cap-add=SYS_ADMIN',
    '--network=none', '--read-only', '--security-opt=no-new-privileges', '--cgroupns=host', '--pid=host',
    '--memory=33554432', '--memory-swap=33554432', '-v', str(E) + ':/own:rw',
    '-v', str(CG / 'cgroup.procs') + ':/admit:rw', '-v', str(tool) + ':/trim:ro',
    'debian:sid', 'sh', '-c', script]
cid = subprocess.check_output(create, text=True).strip()
assert len(cid) == 64 and all(c in '0123456789abcdef' for c in cid)
owned_cid = cid
def cleanup():
    if owned_cid is not None:
        subprocess.run(['docker', 'rm', '-f', owned_cid], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=30)
atexit.register(cleanup)
(D / 'trim-own-helper-cid.json').write_text(json.dumps(dict(cid=cid, source=sha(Path(__file__))), indent=2) + '\n')
proc = subprocess.Popen(['docker', 'start', '-a', cid], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
assert select.select([proc.stdout], [], [], 30)[0], 'owned trim helper startup timeout'
line = proc.stdout.readline()
assert line.startswith('TRIM_ROOT_PID=')
pid = int(line.split('=')[1])
state = json.loads(subprocess.check_output(['docker', 'inspect', cid], text=True))[0]
assert state['State']['Pid'] == pid and state['State']['Running']
p = Path('/proc') / str(pid)
birth = p.joinpath('stat').read_text().split(')')[-1].split()[19]
assert p.stat().st_uid == 0 and p.joinpath('cgroup').read_text().strip() == expected
assert os.sched_getaffinity(pid) == {16}
fd = os.pidfd_open(pid)
subprocess.run(['docker', 'kill', '--signal=CONT', cid], check=True, stdout=subprocess.DEVNULL)
out, _ = proc.communicate(timeout=60)
(D / 'trim-own-ext4-native.log').write_text(line + out)
poll = select.poll()
poll.register(fd, select.POLLIN)
assert proc.returncode == 0 and poll.poll(1000), 'real trim helper retirement required'
os.close(fd)
assert sha(tool) == tool_hash
assert (image.stat().st_dev, image.stat().st_ino, image.stat().st_size, image.stat().st_uid) == identity
after = inventory()
assert before == after, 'all existing live bytes, sizes, modes and owners must remain identical'
after_blocks = image.stat().st_blocks * 512
after_free = os.statvfs(E.parent).f_bavail * os.statvfs(E.parent).f_frsize
assert after_blocks <= before_blocks
subprocess.run(['docker', 'rm', cid], check=True, stdout=subprocess.DEVNULL)
owned_cid = None
q = dict(passed=True, native_execution_claimed=False, exact_own_volume=str(E), backing_image=str(image),
    volume_device='7:31', limit_bytes=8 << 30, original_cgroup=expected, boot_id='c8d3550f-3a19-40d7-8507-a76c2045ace5',
    anchor_pid=11166, anchor_birth=22661, helper_cid=cid, helper_pid=pid, helper_birth=birth,
    helper_actual_pidfd_retired=True, helper_address_space_limit_bytes=64 << 20,
    tool_sha256=tool_hash, controller_sha256=sha(Path(__file__)), all_existing_file_bytes_sizes_modes_owners_unchanged=True,
    existing_files=len(before), inventory_sha256=hashlib.sha256(json.dumps(before, sort_keys=True).encode()).hexdigest(),
    before_image_allocated_bytes=before_blocks, after_image_allocated_bytes=after_blocks,
    before_host_available_bytes=before_free, after_host_available_bytes=after_free,
    native_test_disk_and_memory_limits_unchanged=True)
(D / 'trim-own-ext4-qualified.json').write_text(json.dumps(q, indent=2) + '\n')
print(json.dumps(q), flush=True)
