#!/usr/bin/env python3
"""Restore one shared 64 GiB Linux test cgroup and a bounded persistent arena.

Environment administration only. This script never runs tests, stops another
task, deletes evidence, installs SDKs, or changes an existing memory/CPU limit.
Build/test runners must separately verify membership and supervise owned tasks.
"""
from pathlib import Path
import argparse,json,os,subprocess,sys
BASE=Path("/home/macromodel/Documents/uwvm3-implementation")
ARENA=BASE/"debugger-bounded-20261006"
IMAGE=BASE/"dbg-language-bounded-20261006-r13.ext4"
CPUS="0,2,4,6,16-31"
CAPACITY=16<<30
RESERVE=25<<30

def inspect(name):
    p=subprocess.run(["docker","inspect",name],capture_output=True,text=True,timeout=15)
    if p.returncode:raise RuntimeError(p.stderr.strip())
    return json.loads(p.stdout)[0]

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--container",default="uwvm-debug-tests64g-20261006-r2")
    ap.add_argument("--check-only",action="store_true")
    a=ap.parse_args()
    if sys.platform!="linux":raise RuntimeError("Run on SSH Linux")
    d=inspect(a.container)
    assert d["HostConfig"]["Memory"]==64<<30
    assert d["HostConfig"]["MemorySwap"]==64<<30
    assert d["HostConfig"]["CpusetCpus"]==CPUS
    if not d["State"]["Running"]:
        assert not a.check_only,"Selected keeper is stopped"
        subprocess.run(["docker","start",a.container],check=True,timeout=30)
        d=inspect(a.container)
    pid=d["State"]["Pid"]
    pr=Path("/proc")/str(pid)
    assert pid>0 and (pr/"cmdline").read_bytes()==b"sleep\0infinity\0"
    birth=int((pr/"stat").read_text().rsplit(")",1)[1].split()[19])
    cg=(pr/"cgroup").read_text().strip().split("::")[1]
    assert cg=="/system.slice/docker-"+d["Id"]+".scope"
    config={"pid":pid,"birth":birth,"cg":cg,"arena":str(ARENA),
            "image":str(IMAGE),"check_only":a.check_only}
    administration=r'''
from pathlib import Path
import hashlib,json,os,subprocess,sys
d=json.loads(sys.argv[1]);p=Path("/proc")/str(d["pid"])
assert int((p/"stat").read_text().rsplit(")",1)[1].split()[19])==d["birth"]
assert (p/"cmdline").read_bytes()==b"sleep\0infinity\0"
assert (p/"cgroup").read_text().strip()=="0::"+d["cg"]
c=Path("/sys/fs/cgroup")/d["cg"].lstrip("/")
assert (c/"memory.max").read_text().strip()==str(64<<30)
assert (c/"memory.swap.max").read_text().strip()=="0"
assert (c/"cpuset.cpus.effective").read_text().strip()=="0,2,4,6,16-31"
arena,image=Path(d["arena"]),Path(d["image"])
host=os.statvfs("/home/macromodel/Documents")
assert host.f_bavail*host.f_frsize>=25<<30,"Host disk reserve below 25 GiB"
if not d["check_only"]:
    # Root-only stopped-admission helper needs the original root ownership.
    # Never broaden permissions or delegate any other cgroup control file.
    os.chown(c/"cgroup.procs",0,0)
    arena.parent.mkdir(mode=0o700,exist_ok=True)
    arena.mkdir(mode=0o700,exist_ok=True)
    if not os.path.ismount(arena):
        assert not list(arena.iterdir()),"Refuse to hide a nonempty directory"
        if not image.exists():
            assert host.f_bavail*host.f_frsize>41<<30
            with image.open("xb") as stream:stream.truncate(16<<30)
            image.chmod(0o600)
            subprocess.run(["/usr/sbin/mkfs.ext4","-q","-F","-m","0",
                            "-L","uwvm2_dbg_1006",str(image)],check=True,timeout=30)
        else:
            assert not image.is_symlink() and image.stat().st_uid==0
            assert image.stat().st_size==16<<30
        subprocess.run(["/usr/bin/mount","-t","ext4","-o","loop,nodev,nosuid",
                        str(image),str(arena)],check=True,timeout=30)
    os.chown(arena,1000,1000)
    arena.chmod(0o700)
assert os.path.ismount(arena) and not arena.is_symlink()
assert arena.stat().st_uid==1000
assert image.stat().st_uid==0 and image.stat().st_size==16<<30
mounts=[s.split() for s in Path("/proc/self/mountinfo").read_text().splitlines()
        if s.split()[4]==str(arena)]
assert len(mounts)==1
fields=mounts[0];sep=fields.index("-")
assert fields[sep+1]=="ext4" and {"nodev","nosuid"}<=set(fields[5].split(","))
backing=(Path("/sys/dev/block")/fields[2]/"loop/backing_file").read_text().strip()
assert Path("/"+backing.lstrip("/")).resolve()==image.resolve()
v=os.statvfs(arena)
assert 15<<30<v.f_blocks*v.f_frsize<16<<30
assert v.f_bavail*v.f_frsize>=1<<30 and v.f_favail>=4096
assert (c/"cgroup.procs").stat().st_uid==0
record={"passed":True,"boot":Path("/proc/sys/kernel/random/boot_id").read_text().strip(),
        "init_pid":d["pid"],"init_birth":d["birth"],"cgroup":str(c),
        "memory_max_bytes":64<<30,"swap_max_bytes":0,
        "cpuset":"0,2,4,6,16-31","arena":str(arena),"image":str(image),
        "hard_image_max_bytes":16<<30,"filesystem_id":v.f_fsid,
        "filesystem_total_bytes":v.f_blocks*v.f_frsize,
        "filesystem_available_bytes":v.f_bavail*v.f_frsize,
        "image_allocated_bytes":image.stat().st_blocks*512,
        "host_disk_free_bytes":host.f_bavail*host.f_frsize,
        "host_disk_reserve_bytes":25<<30,"cgroup_procs_uid":0,
        "memory_events":(c/"memory.events").read_text(),"tests_executed":False}
print(json.dumps(record,indent=2))
'''
    argv=["docker","run","--rm","--user","0:0","--privileged","--pid=host",
          "--cgroupns=host","--network","none","--memory","256m",
          "--memory-swap","256m","--cpuset-cpus","16","--entrypoint",
          "nsenter",d["Image"],"-t","1","-m","-r","-w","--","/usr/bin/python3",
          "-c",administration,json.dumps(config)]
    subprocess.run(argv,check=True,timeout=60)
if __name__=="__main__":main()
