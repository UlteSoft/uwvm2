import os,pathlib,sys,resource
CG='/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
BOOT='e000195b-afa2-4a53-93ba-1fb8e28eac03'
cg=pathlib.Path('/sys/fs/cgroup'+CG)
assert os.geteuid()==0
assert pathlib.Path('/proc/sys/kernel/random/boot_id').read_text().strip()==BOOT
assert (cg/'memory.max').read_text().strip()==str(64<<30)
assert (cg/'memory.swap.max').read_text().strip()=='0'
g=[p.parent for p in pathlib.Path('/sys/fs/cgroup').rglob('memory.max') if p.read_text().strip()==str(64<<30)]
assert g==[cg],g
(cg/'cgroup.procs').write_text(str(os.getpid()))
assert pathlib.Path('/proc/self/cgroup').read_text().strip()=='0::'+CG
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
resource.setrlimit(resource.RLIMIT_FSIZE,(128<<20,128<<20))
resource.setrlimit(resource.RLIMIT_MEMLOCK,(64<<20,64<<20))
os.setgroups([]);os.setgid(1000);os.setuid(1000);os.sched_setaffinity(0,{20})
os.environ.update(HOME='/home/macromodel',USER='macromodel',LOGNAME='macromodel',TMPDIR='/tmp/uwvm-gc-perf-20261011-r73/tmp',PATH='/home/macromodel/.local/bin:/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin:/usr/bin:/bin',LD_LIBRARY_PATH='/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib:/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/lib/x86_64-unknown-linux-gnu')
os.execvp(sys.argv[1],sys.argv[1:])
