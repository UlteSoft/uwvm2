"""Bounded source-contract audit only. No compiler, Wasm, ROS peer or profiler."""
from pathlib import Path
import datetime,hashlib,json,os,re,resource,sys,traceback
V=Path('/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1')
OUT=V/'root-language-full-requirements-audit-20261019-a1'
ROOT=OUT/'source'
G='/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
def h(b):return hashlib.sha256(b).hexdigest()
def bracket():
 c=Path('/sys/fs/cgroup'+G)
 assert Path('/proc/self/cgroup').read_text().strip()=='0::'+G
 assert(c.stat().st_dev,c.stat().st_ino)==(29,19075)
 assert(c/'memory.max').read_text().strip()==str(64<<30)and(c/'memory.swap.max').read_text().strip()=='0'
 assert Path('/proc/sys/kernel/random/boot_id').read_text().strip()=='51c27aa1-d709-4a06-ba16-2ac3003fbcbd'
 return(c/'memory.events').read_text()
def main():
 os.sched_setaffinity(0,{15});resource.setrlimit(resource.RLIMIT_AS,(512<<20,512<<20));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 before=bracket();raw=(OUT/'input-manifest.json').read_bytes();manifest=json.loads(raw)
 for r in manifest['members']:
  b=(ROOT/r['path']).read_bytes();assert len(b)==r['bytes']and h(b)==r['sha256']
 sys.path.insert(0,str(ROOT/'sdk/tools'));import check_bindings as cb
 schema=json.loads((ROOT/'sdk/abi/abi-v1.json').read_bytes())
 rows=[];authority={}
 try:authority=cb.check()
 except Exception as e:authority={'status':'SOURCE_CONTRACT_FAILED','error':repr(e),'scope':'Current actual Host/catalog provenance or existing binding/dispatch check failed. Separate per-declaration rows below do not erase this failure.'}
 def widths(s,pattern,mapping):return cb.widths(s,pattern,mapping)
 def put(mapping,name,value):
  if name in mapping:raise ValueError('duplicate '+name)
  mapping[name]=value
 def extra(kind,v):
  module='uwvm_ros'+str(v)+'_v1';rows={}
  if kind=='Go':
   p=ROOT/f'sdk/go/raw_ros{v}.go'
   for mod,name,symbol,args in re.findall(r'//go:wasmimport\s+(\w+)\s+(\w+)\s*\nfunc\s+(\w+)\(([^)]*)\)\s+uint32',p.read_text()):
    assert mod==module and symbol==''.join(n.title()for n in name.split('_'));put(rows,name,(widths(args,r'\w+\s+(uint32|uint64)',{'uint32':'i32','uint64':'i64'}),['i32']))
  elif kind=='Zig':
   p=ROOT/f'sdk/zig/ros{v}.zig'
   for mod,name,args in re.findall(r'pub extern "(\w+)" fn (\w+)\(([^)]*)\) u32;',p.read_text()):
    assert mod==module;put(rows,name,(widths(args,r'(?:@"[^"]+"|\w+)\s*:\s*(u32|u64)',{'u32':'i32','u64':'i64'}),['i32']))
  elif kind=='Java/Kotlin':
   p=ROOT/f'sdk/java/src/main/java/org/uwvm/roswasm/Ros{v}Raw.java'
   for mod,name,symbol,args in re.findall(r'@Import\(module="(\w+)",\s*name="(\w+)"\)\s+@Unmanaged\s+public static native int (\w+)\(([^)]*)\);',p.read_text()):
    assert mod==module and name==symbol;put(rows,name,(widths(args,r'(int|long)\s+\w+',{'int':'i32','long':'i64'}),['i32']))
  elif kind=='MoonBit':
   p=ROOT/f'sdk/moonbit/ros{v}/raw.mbt'
   for symbol,args,mod,name in re.findall(r'pub fn raw_(\w+)\(([^)]*)\)\s*->\s*Int\s*=\s*"(\w+)"\s*"(\w+)"',p.read_text()):
    assert mod==module and name==symbol;put(rows,name,(widths(args,r'\w+\s*:\s*(Int|UInt64)',{'Int':'i32','UInt64':'i64'}),['i32']))
  elif kind=='CSharp/FSharp/VB':
   p=ROOT/'sdk/csharp/RosWasm/SelectedRaw.cs'
   for version,name,symbol,args in re.findall(r'\[DllImport\("roswasm_bridge",EntryPoint="roswasm_ros([12])_(\w+)",ExactSpelling=true,CallingConvention=CallingConvention.Cdecl\)\]\s+internal static extern uint (\w+)\(([^)]*)\);',p.read_text()):
    if int(version)!=v:continue
    assert name==symbol;put(rows,name,(widths(args,r'(uint|ulong)\s+\w+',{'uint':'i32','ulong':'i64'}),['i32']))
  elif kind=='CSharp C forwarder':
   p=ROOT/'sdk/csharp/roswasm_bridge.c'
   for version,name,args,callee_version,callee,values in re.findall(r'uint32_t roswasm_ros([12])_(\w+)\(([^)]*)\)\s*\{\s*return uwvm_ros([12])_(\w+)\(([^)]*)\);\s*\}',p.read_text()):
    if int(version)!=v:continue
    assert version==callee_version and name==callee
    names=[]if args.strip()=='void'else[a.split()[-1]for a in args.split(',')if a.strip()]
    assert names==[a.strip()for a in values.split(',')if a.strip()]
    put(rows,name,(widths(args,r'(uint32_t|uint64_t)\s+\w+',{'uint32_t':'i32','uint64_t':'i64'}),['i32']))
  return p,rows
 for v in[1,2]:
  expected={f['name']:([p['type']for p in f['params']],f['results'])for f in schema['modules'][f'uwvm_ros{v}_v1']['functions']}
  parsers=[('C/C++',lambda v:cb.parse_c(v)),('Rust',lambda v:cb.parse_rust(v)),('TinyGo',lambda v:cb.parse_tinygo(v)),('AssemblyScript',lambda v:cb.parse_assemblyscript(v)),('JavaScript metadata',lambda v:cb.parse_javascript(v))]
  parsers +=[(kind,lambda v,kind=kind:extra(kind,v))for kind in['Go','Zig','Java/Kotlin','MoonBit','CSharp/FSharp/VB','CSharp C forwarder']]
  for kind,parser in parsers:
   try:
    path,actual=parser(v)
    missing=sorted(expected.keys()-actual.keys());extra_names=sorted(actual.keys()-expected.keys())
    wrong=sorted(n for n in expected.keys()&actual.keys()if expected[n]!=actual[n])
    passed=not(missing or extra_names or wrong)
    rows.append({'binding':kind,'backend':v,'path':str(path.relative_to(ROOT)),'declarations':len(actual),'passed':passed,'missing':missing,'extra':extra_names,'wrong_widths':wrong})
   except Exception as e:rows.append({'binding':kind,'backend':v,'passed':False,'error':repr(e)})
 text=(ROOT/'sdk/python/roswasm/_native.c').read_text()
 names=set(re.findall(r'(?:else\s+)?if\s*\(strcmp\(name,\s*"(\w+)"\)\s*==\s*0\)\s*\{',text))
 expected_names={f['name']for m in schema['modules'].values()for f in m['functions']}
 python={'scope':'Source dispatch-name inventory plus selected checked C headers only. Not CPython C API, frozen target link, runtime behavior or separate parameter-width proof.','names':len(names),'missing':sorted(expected_names-names),'extra':sorted(names-expected_names),'passed':names==expected_names,'selects_checked_ros1_header':'#include "ros1/abi.h"'in text,'selects_checked_ros2_header':'#include "ros2/abi.h"'in text}
 dispatch={}
 try:dispatch={'status':'PASS','tables':cb.check_c_dispatch(schema)}
 except Exception as e:dispatch={'status':'FAIL','error':repr(e)}
 for r in manifest['members']:
  b=(ROOT/r['path']).read_bytes();assert len(b)==r['bytes']and h(b)==r['sha256']
 assert bracket()==before
 result={'status':'SOURCE_CONTRACT_AUDIT_FINISHED','captured_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'Strict declared import sets, scalar widths and C forwarding/dispatch source only. No language compilation, Wasm/ROS execution, compiler retention, permission or SDK parity qualification.','input_manifest_sha256':h(raw),'existing_authoritative_checker':authority,'bindings':rows,'python_dispatch':python,'C_dispatch':dispatch,'all_binding_declarations_passed':all(r['passed']for r in rows),'all15_requirements_complete':False,'memory_events':before,'cgroup':G,'memory.max':64<<30,'swap_max':0,'no_child_processes_launched':True}
 (OUT/'source-contract-result.json').write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps(result))
if __name__=='__main__':main()
