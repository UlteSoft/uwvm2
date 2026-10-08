#!/usr/bin/env python3
from pathlib import Path
import subprocess,re,json,hashlib
import argparse
parser=argparse.ArgumentParser(description='Verify actual x86 codegen for shared/unshared size and ordinary mmap access.')
parser.add_argument('--llvm-bin',type=Path,required=True)
parser.add_argument('roots',type=Path,nargs='+',help='directories containing threads_shared_memory_ir.cc output')
args=parser.parse_args();roots=args.roots;llvm=args.llvm_bin
subprocess.run(['bash',str(Path(__file__).resolve().parents[2]/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
for root in roots:
 out=root/'codegen';out.mkdir(exist_ok=True);rows=[];blocks={}
 for shared in [False,True]:
  for unwind in [False,True]:
   stem=('shared' if shared else 'unshared')+'-'+('unwind' if unwind else 'instruction')
   source=root/(stem+'.ll');optimized=out/(stem+'.opt.ll');assembly=out/(stem+'.s')
   cmds=[[str(llvm/'opt'),'-passes=default<O3>','-S',str(source),'-o',str(optimized)],
         [str(llvm/'llc'),'-O3','-mtriple=x86_64-unknown-linux-gnu','-mcpu=raptorlake',str(optimized),'-o',str(assembly)]]
   for cmd in cmds:subprocess.run(cmd,check=True)
   text=optimized.read_text();machine=assembly.read_text()
   for index in [0,2]:
    body=re.search(r'(?ms)^define [^\n]*@uwvm_\w+_func_'+str(index)+r'\([^\n]*\).*?^}',text).group()
    hot=re.search(r'(?ms)^uwvm_\w+_func_'+str(index)+r':.*?^\.Lfunc_end\d+:',machine).group()
    instructions=[line.split('#')[0].strip() for line in hot.splitlines() if re.match(r'^\s+[a-z][a-z0-9]*\s',line)]
    if unwind:
     assert not any(re.match(r'(call|[msl]fence)',x) for x in instructions),(stem,index,instructions)
    if index==0:
     assert re.search(r'load atomic i64,.*?'+('seq_cst' if shared else 'acquire'),body),stem
    else:assert 'memory.length' not in body and 'load atomic' not in body,stem
    blocks[(shared,unwind,index)]=instructions
   rows.append({'input':str(source),'sha256':hashlib.file_digest(source.open('rb'),'sha256').hexdigest(),'commands':cmds,'passed':True})
 for unwind in [False,True]:
  for index in [0,2]:assert blocks[(False,unwind,index)]==blocks[(True,unwind,index)],(unwind,index,blocks)
 (out/'results.json').write_text(json.dumps({'rows':rows,'machine_paths_identical':True},indent=2)+'\n')
 print(root,'PASS x86 shared/unshared size and plain-load instruction equality; native unwind hot paths have no calls/fences')
