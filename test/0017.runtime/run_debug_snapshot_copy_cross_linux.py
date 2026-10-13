#!/usr/bin/env python3
"""Actual private snapshot copier target execution; full VM/ASM parity remains separate.
Invoke only behind the original guarded Linux cgroup supervisor. --unit names
an independently compiled debug_snapshot_copy_ir.cc result directory; its real
LLVM producer emits all four pointer/endian layouts before target lowering.
MIPS uses a freestanding UAPI entry because the restored libc CRT requests an
executable stack; the component fixture itself requires a non-executable stack.
"""
from pathlib import Path
import argparse,hashlib,importlib.util,json,subprocess,time

ap=argparse.ArgumentParser(description=__doc__);ap.add_argument("--repository",choices=["uwvm2","uwvm2-ros"],required=True);ap.add_argument("--unit",type=Path,required=True);ap.add_argument("--root",type=Path,required=True);ap.add_argument("--out",type=Path,required=True);a=ap.parse_args()
R=a.root.resolve(strict=True);S=R/a.repository;O=a.out.resolve();O.mkdir(parents=True,exist_ok=False)
subprocess.run(["bash",str(S/"tools/ci/require_wasm3_test_cgroup.sh")],check=True)
spec=importlib.util.spec_from_file_location("actual_profiles",S/"test/0017.runtime/run_debug_linux_qemu_components.py");p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)
U=a.unit.resolve(strict=True);D=json.loads((U/"results.json").read_text());assert D["passed"] and D["all_inputs_after_unchanged"]
assert Path(D["source_root"]).resolve(strict=True)==S
manifest=json.loads((R/(a.repository+"-current-manifest.json")).read_text());assert manifest["source_id"]==D["source_id"]
exe=U/"test";assert p.sha(exe)==D["binary_sha256"]
deps=Path("/home/macromodel/Documents/uwvm3-implementation/deps");clang=Path("/usr/lib/llvm-22/bin/clang");llc=Path("/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin/llc");lld=Path("/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm/bin/ld.lld")
pins={str(v):p.sha(v) for v in (Path(__file__),S/"test/0017.runtime/run_debug_linux_qemu_components.py",U/"results.json",exe,llc.resolve(),clang.resolve(),lld.resolve())}
pins.update(D["inputs"]);assert all(p.sha(v)==h for v,h in pins.items())
record={"passed":False,"scope":"actual production private copier, LLVM-generated 32/64 pointer layouts and verified target ELFs executed natively/QEMU; finite bytes/flags component DATA","reference_carrier_description":"The host C++ emitter's physical reference description is DATA; this does not qualify a complete target C++ emitter ABI or source/VM leases.","full_target_Cpp_emitter_ABI_qualified":False,"full_cross_arch_product_parity":False,"Wasm_ASM_authority":False,"source_id":D["source_id"],"inputs":pins,"layouts":[],"rows":[],"cgroup":Path("/proc/self/cgroup").read_text()}
def publish():(O/"results.json").write_text(json.dumps(record,indent=2,default=str)+"\n")
def run(row,phase,argv):
 log=row["out"]/(phase+".log");began=time.monotonic()
 with log.open("wb") as f:r=subprocess.run(list(map(str,argv)),stdout=f,stderr=subprocess.STDOUT,timeout=120)
 row["phases"].append({"phase":phase,"argv":list(map(str,argv)),"passed":r.returncode==0,"returncode":r.returncode,"seconds":time.monotonic()-began,"log_sha256":p.sha(log)});publish();assert r.returncode==0,log.read_text()[-3500:]
publish();layouts={}
try:
 for bits,order in ((32,"little"),(32,"big"),(64,"little"),(64,"big")):
  out=O/("layout-"+str(bits)+"-"+order);out.mkdir();ir=out/"actual.ll";bc=out/"actual.bc";row={"bits":bits,"byte_order":order,"phases":[],"out":out};record["layouts"].append(row)
  run(row,"emit",[exe,ir,bc,str(bits),order]);pins[str(ir)]=p.sha(ir);pins[str(bc)]=p.sha(bc);row.update(ir_sha256=p.sha(ir),bitcode_sha256=p.sha(bc));layouts[(bits,order)]=bc
 for profile,triple,emulator,machine,bits,order,flags in p.PROFILES:
  out=O/profile;out.mkdir();obj=out/"actual-copy.o";binary=out/"test.elf"
  row={"profile":profile,"bits":bits,"byte_order":order,"passed":False,"phases":[],"out":out};record["rows"].append(row);bc=layouts[(bits,order)]
  # The restored universal llc lacks PPC/Sparc/MIPS/LoongArch targets.
  # Clang 22 provides all qualified targets; consume the exact generated text
  # (integer bit carriers), with each target ABI's actual profile flags.
  emit_row=next(v for v in record["layouts"] if v["bits"]==bits and v["byte_order"]==order)
  ir=Path(emit_row["out"])/"actual.ll"
  lowerflags=["--target="+triple,*flags,"-Wno-override-module","-fno-pic","-fno-pie"]
  if profile in ("ppc64","ppc64le"):lowerflags+=["-mcmodel=medium"]
  if profile=="sparc64":lowerflags+=["-mcpu=ultrasparc"]
  if profile.startswith("mips"):lowerflags+=["-mno-abicalls"]
  run(row,"lower",[clang,*lowerflags,"-O0","-x","ir","-c",ir,"-o",obj])
  sysroot=deps/"usr"/triple;gcc=deps/"usr/lib/gcc-cross"/triple/"15";linker=deps/"usr/bin"/(triple+"-ld") if profile in ("ppc64","ppc32","sparc64") else lld;qemu=deps/"usr/bin"/("qemu-"+emulator)
  if profile=="x86_64":
   sysroot=Path("/");gcc=Path("/usr/lib/gcc/x86_64-linux-gnu/15")
   if not gcc.exists():gcc=Path("/usr/lib/gcc/x86_64-linux-gnu/14")
  for v in (linker.resolve(),qemu.resolve()):pins[str(v)]=p.sha(v)
  argv=[clang,"--target="+triple,"--sysroot="+str(Path("/") if profile=="x86_64" else deps),"--gcc-install-dir="+str(gcc),"--ld-path="+str(linker),*flags,obj,"-no-pie","-o",binary]
  if profile!="x86_64":argv[1:1]=["-idirafter",str(sysroot/"include"),"-L"+str(sysroot/"lib"),"-L"+str(deps/"usr/lib"/triple)]
  if profile.startswith("mips"):
   # Avoid an unrelated legacy libc CRT executable-stack requirement. This
   # fixture has no libc/IO; execute the exact generated main via Linux exit.
   # Actual kernel UAPI headers supply the syscall number; no hardcoded ABI.
   start=out/"start.S";start_obj=out/"start.o"
   start.write_text('#include <asm/unistd.h>\n.text\n.set noreorder\n.globl _start\n.ent _start\n_start:\n'+("daddiu $29,$29,-32\ndla $28,_gp\ndla $25,main\n" if bits==64 else "addiu $29,$29,-32\nla $28,_gp\nla $25,main\n")+'jalr $25\nnop\nmove $4,$2\nli $2,__NR_exit\nsyscall\nb .\nnop\n.end _start\n.section .note.GNU-stack,"",@progbits\n')
   pins[str(start)]=p.sha(start)
   for header in (sysroot/"include/asm/unistd.h",sysroot/"include/asm/sgidefs.h",sysroot/("include/asm/unistd_n64.h" if bits==64 else "include/asm/unistd_o32.h")):
    pins[str(header)]=p.sha(header)
   run(row,"start",[clang,"--target="+triple,*flags,"-mno-abicalls","-fno-pic","-fno-pie","-fno-stack-protector","-ffreestanding","-nostdinc","-I"+str(sysroot/"include"),"-c",start,"-o",start_obj])
   argv=[clang,"--target="+triple,*flags,"-mno-abicalls","-fno-pic","--ld-path="+str(lld),"-nostdlib","-static","-no-pie","-Wl,-e,_start",start_obj,obj,"-o",binary]
   row["startup"]="freestanding kernel-UAPI exit, no libc CRT qualification"
  # This LLVM-only fixture has no nested-function trampolines. Override legacy
  # MIPS CRT executable-stack notes with a required non-executable stack.
  argv.insert(-2,"-Wl,-z,noexecstack")
  run(row,"link",argv);row["elf"]=p.elf(binary,machine,bits,order)
  assert row["elf"]["GNU_STACK_flags"] and all((v & 1)==0 for v in row["elf"]["GNU_STACK_flags"]), "executable or missing GNU_STACK"
  launcher=[binary] if profile=="x86_64" else [qemu,"-U","LD_LIBRARY_PATH","-L",sysroot,binary]
  run(row,"execute",launcher);row["passed"]=True;publish();print(a.repository,profile,"PASS actual snapshot bytes/flags",flush=True)
 assert all(p.sha(v)==h for v,h in pins.items());record.update(passed=True,all_inputs_after_unchanged=True)
except BaseException as e:record["error"]=repr(e);raise
finally:publish()
