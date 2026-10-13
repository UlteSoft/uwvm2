#!/usr/bin/env python3
"""Real macOS ARM64 PTY: owned-PC movement, SI/NI calls, finish and level handoff.
Fixture: fixtures/debug_rich_guest.c built as described in that source.
"""
import argparse,json,re
from pathlib import Path
from run_debug_console_tui_color import Driver
from run_debug_run_cli import require

def main():
 p=argparse.ArgumentParser();p.add_argument('--uwvm',type=Path,required=True);p.add_argument('--wasm',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False)
 c=Driver([str(a.uwvm.resolve()),'-Rdbg','-Rct','0','-Rllvm-cache-path','disable','--run',str(a.wasm.resolve())]);actions=[];checks=[]
 def command(text,allow_error=False):
  reply=c.ask(text).decode('utf8','replace');actions.append({'command':text,'reply':reply});(a.out/'actions.json').write_text(json.dumps(actions,indent=2))
  require(allow_error or 'error:' not in reply,(text,reply));return reply
 def check(ok,name):require(ok,name);checks.append(name);print(name,flush=True)
 def position(reply):
  pc=re.search(r'native-pc=(0x[0-9a-f]+)',reply);stop=re.search(r'stop-id (\d+)',reply);function=re.search(r'thread 1 module=0 function=(\d+)',reply)
  return (int(stop[1]) if stop else None,int(pc[1],16) if pc else None,int(function[1]) if function else None)
 def start_call():
  command('start');reply=command('bw 0 2 40');point=re.search(r'breakpoint (\d+) registered',reply)[1];reply=command('c');check('byte-offset=40' in reply,'real Wasm call breakpoint');command('delete '+point)
 try:
  c.wait(lambda b:b'(uwvm-debug) ' in b,0);c.drain()
  command('help jump');reply=command('starti');before=position(reply);command('jump asm *0x1',True);check(position(command('status'))==before,'outside JIT owner retains exact stop')
  moved=False
  for _ in range(30):
   reply=command('ni');before=position(reply);require(before[1] is not None,'real kernel PC')
   reply=command('jump asm *'+hex(before[1]+4),True)
   if 'Execution moved' in reply:
    check(position(command('status'))[1]==before[1]+4,'PC relocation is read back from the real trap')
    command('jump asm *'+hex(before[1]));check(position(command('status'))[1]==before[1],'reverse relocation preserves native stop')
    c.send('layout asm\n');c.send('tui enable\n');lines=c.model.text().splitlines();y=next(i+1 for i,line in enumerate(lines) if '=> 0x' in line);x=lines[y-1].index('=>')+1
    for event in [f'\x1b[<0;{x};{y}M',f'\x1b[<32;{x};{y+1}M',f'\x1b[<0;{x};{y+1}m']:c.send(event)
    check(re.search(r'=> 0x0*'+f'{before[1]+4:x}'+r'\b',c.model.text()),'real SGR marker drag changes authenticated native PC')
    c.send('jump asm *'+hex(before[1])+'\n');c.send('tui disable\n');check(position(command('status'))[1]==before[1],'mouse and command relocation share the same stop')
    moved=True;break
   check(position(command('status'))==before,'incompatible move is transactional')
  check(moved,'at least one complete frame-neutral native interval moved')
  command('start');before=position(command('status'));reply=command('jump wasm 8',True);check('resumable JIT landing' in reply,'Wasm movement without restore state refuses explicitly');check(position(command('status'))==before,'rejected Wasm movement retains stop')
  start_call()
  for i in range(256):
   reply=command('si')
   if position(reply)[2]==1:break
  else:raise AssertionError('SI did not enter leaf')
  check(position(reply)[1] is not None,'SI enters actual callee at an owned native trap')
  reply=command('fi');check(position(reply)[1] is not None and position(reply)[2]==2,'fi returns from leaf to main at a new native trap')
  command('list wasm');reply=command('nw');check(position(reply)[1] is None and position(reply)[2]==2,'nw hands native stop back to current Wasm activation')
  start_call()
  for i in range(256):
   reply=command('ni');require(position(reply)[2]==2,'NI stays in main')
   if re.search(r'native-wasm provenance=exact .*byte-offset=46\b',reply):break
  else:raise AssertionError('NI did not reach next caller Wasm instruction')
  check(position(reply)[1] is not None,'NI passes call without publishing a callee or host stop')
  reply=command('fi');check(position(reply)[2]==3 and position(reply)[1] is not None,'native finish follows actual activation to _start')
  before=position(command('status'));command('fi',True);check(position(command('status'))==before,'root finish cannot invent a caller')
  command('start');reply=command('bw 0 1 26');point=re.search(r'breakpoint (\d+) registered',reply)[1];command('c');command('delete '+point)
  for i in range(128):
   reply=command('ni')
   if position(reply)[2]==2:break
  else:raise AssertionError('native return did not rebase to the parent')
  check(position(reply)[1] is not None,'NI crosses activation retirement without a stale trap')
  command('c');c.finish();(a.out/'result.json').write_text(json.dumps({'passed':True,'checks':checks,'actions':len(actions)},indent=2))
 finally:(a.out/'transcript.bin').write_bytes(c.log);c.close()
if __name__=='__main__':main()
