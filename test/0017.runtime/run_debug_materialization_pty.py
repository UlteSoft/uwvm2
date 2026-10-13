#!/usr/bin/env python3
"""Real PTY regression: exact per-opcode values across numeric/reference/control cases."""
import argparse
from pathlib import Path
import sys,os,re,json,subprocess,time,hashlib
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root/'test/0018.debugger'));sys.path.insert(0,str(root/'test/0017.runtime'))
from run_wasm_operand_preview_cli import examples,markers,operands
from run_debug_console_tui_color import Driver
p=argparse.ArgumentParser(description=__doc__)
for name in ['uwvm','wasm-tools','out']:p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--case',action='append',help='Select named regression cases; defaults to the complete matrix.')
a=p.parse_args();binary=a.uwvm.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=False)
os.environ['NO_COLOR']='1';os.environ['TERM']='dumb';wt=a.wasm_tools.resolve()
names=['mixed-numeric-bits','gc-reference-kinds','loop-backedge-prefix','call-multivalue-frames','tail-call-frames','exception-payload-prefix','exception-reference-rethrow','nondefaultable-local-initialization','heap-workspace-1000-locals']
if a.case:
 assert set(a.case)<=set(names),a.case
 names=a.case
rows=[]
for case in [x for x in examples() if x['name'] in names]:
 wat=out/(case['name']+'.wat');wasm=wat.with_suffix('.wasm');wat.write_text(case['wat']);subprocess.run([str(wt),'parse',str(wat),'-o',str(wasm)],check=True)
 sites,dump=markers(wt,wasm);c=Driver([str(binary),'-Rdbg','-Rct','0','-Rllvm-cache-path','disable',*['-WFE-'+f for f in ['multi-value','reference-types','function-references','gc','simd','tail-call','exceptions','threads','bulk-memory','multi-memory','memory64','table64','relaxed-simd']],'--run',str(wasm)])
 class Adapter:
  def send(self,text):return c.ask(text)
 adapter=Adapter();observed=[]
 try:
  c.wait(lambda b:b'(uwvm-debug) ' in b,0,120);c.drain()
  for function,offsets in sites.items():
   for offset in offsets:assert b'registered' in c.ask(f'bw 0 {function} {offset}')
  for expected in case['expected']:
   c.ask('continue');reply=c.ask('status');loc=re.search(rb'thread (\d+) module=0 function=(\d+) byte-offset=(\d+)',reply);assert loc,reply
   thread,function,offset=map(int,loc.groups());assert (function,offset)==(expected['function'],sites[expected['function']][expected['ordinal']]),(reply,expected)
   values=[];operands(adapter,thread,0,expected['values'],values)
   if expected['caller'] is not None:operands(adapter,thread,1,expected['caller'],values)
   if expected['locals_page']:
    local=expected['locals_page']
    for index,value in local['values'].items():
     answer=c.ask(f'locals wasm {thread} 0 {index} 1');assert re.findall(rb'^local ([0-9]+) (.*)$',answer,re.M)==[(str(index).encode(),value.encode())],answer
   observed.append({'function':function,'offset':offset,'values':values})
  c.ask('continue');assert b'Program exited normally.' in c.ask('status');c.finish()
  rows.append({'case':case['name'],'passed':True,'observations':observed});print('PASS',case['name'],flush=True)
 finally:(out/(case['name']+'.bin')).write_bytes(c.log);c.close();(out/'result.json').write_text(json.dumps({'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'cases':rows},indent=2))
