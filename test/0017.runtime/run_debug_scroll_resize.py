#!/usr/bin/env python3
"""Real PTY paging, bounded wheel bursts and title-only column resizing."""
import argparse,fcntl,json,os,re,select,struct,termios,time
from pathlib import Path
from run_debug_console_tui_color import Driver
from run_debug_rich_tui import RichTerminal
from run_debug_run_cli import require

def main():
 p=argparse.ArgumentParser();p.add_argument('--uwvm',type=Path,required=True);p.add_argument('--wasm',type=Path,required=True)
 p.add_argument('--out',type=Path,required=True);p.add_argument('--unmapped-wasm',type=Path);p.add_argument('--expected-cgroup')
 a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False)
 if a.expected_cgroup:require(Path('/proc/self/cgroup').read_text().strip()=='0::'+a.expected_cgroup,'cgroup')
 c=Driver([str(a.uwvm.resolve()),'-Rdbg','-Rct','0','-Rllvm-cache-path','disable','--run',str(a.wasm.resolve())]);checks=[];actions=[]
 def check(ok,why):require(ok,(why,c.model.text()));checks.append(why)
 def send(s):
  raw=c.send(s);actions.append({'input':repr(s),'screen':c.model.text()});return raw
 def cmd(s):return send(s+'\n')
 def mouse(button,x,y,release=False):return send(f'\x1b[<{button};{x};{y}'+('m' if release else 'M'))
 def resize(w,h):
  fcntl.ioctl(c.fd,termios.TIOCSWINSZ,struct.pack('HHHH',h,w,0,0));c.model=RichTerminal(w,h);c.model.alternate=True;c.model.primary=([[' ']*w for _ in range(h)],0,0);c.painted=len(c.log);c.drain()
 def code():return c.model.text().split('[io]')[0]
 def addresses():return re.findall(r'(?:=>|N>|  )\s+(0x[0-9a-f]+)  ',code())
 def flood(event,count):
  payload=event.encode()*count;at=0;started=time.monotonic();flags=fcntl.fcntl(c.fd,fcntl.F_GETFL);fcntl.fcntl(c.fd,fcntl.F_SETFL,flags|os.O_NONBLOCK)
  try:
   while at<len(payload):
    require(time.monotonic()-started<8,'wheel input drain deadline')
    readers,writers,_=select.select([c.fd],[c.fd],[],.02)
    if readers:
     try:c.log.extend(os.read(c.fd,65536))
     except BlockingIOError:pass
    if writers:
     try:at+=os.write(c.fd,payload[at:at+4096])
     except BlockingIOError:pass
  finally:fcntl.fcntl(c.fd,fcntl.F_SETFL,flags)
  c.drain();cmd('status');elapsed=time.monotonic()-started
  check(elapsed<8 and '(uwvm-debug)' in c.model.text(),f'{count} wheel events remain responsive ({elapsed:.3f}s)')
 try:
  c.wait(lambda b:b'(uwvm-debug) ' in b,0);c.drain();cmd('start');cmd('tui enable');resize(144,85)
  cmd('layout asm');check(len(addresses())>32,'ASM fills tall viewport across multiple 32-row pages');initial=addresses()[0]
  mouse(65,8,8);check(addresses()[0]!=initial,'ASM wheel fetches next actual page')
  mouse(64,8,8);check(addresses()[0]==initial,'ASM wheel returns to reference instruction')
  # Instrumented x86 functions can contain more than 4096 instructions.
  # A command's step bound is not a bound on the entire JIT function.
  for _ in range(64):
   cmd('scroll asm down 4096')
   if len(addresses())==1:break
  check(len(addresses())==1,'ASM clamps to final instruction at top');last=addresses()
  flood('\x1b[<65;8;8M',3000);check(addresses()==last,'ASM bottom cannot scroll into empty space')
  for _ in range(64):
   before=addresses();cmd('scroll asm up 4096')
   if addresses()==before:break
  check(bool(addresses()),'ASM clamps to function entry');first=addresses()[0]
  flood('\x1b[<64;8;8M',3000);check(addresses()[0]==first,'ASM top remains inside same JIT function')
  cmd('layout src');source=a.out/'scroll.c';source.write_text('one\ntwo\nthree\n')
  cmd('set source '+str(source.resolve()));cmd('scroll src down 4096')
  check('3 three' in code() and '4 ' not in code(),'source EOF excludes artificial trailing blank line')
  flood('\x1b[<65;8;8M',3000);check('3 three' in code(),'source bottom remains last real line')
  large=a.out/'large.c';large.write_text(''.join(f'// line {i:05d} '+('x'*1000)+'\n' for i in range(1,5001)))
  cmd('set source '+str(large.resolve()));flood('\x1b[<65;8;8M',3000)
  check('line 05000' in code() and 'line 05001' not in code(),'large mapped source burst reaches exact EOF')
  resize(144,55);cmd('layout horizontal src wasm asm');cmd('window show')
  check(all(f'window {pane} x={x} y=3 width=48' in c.model.text() for pane,x in [('src',1),('wasm',49),('asm',97)]),'three equal columns')
  mouse(0,5,3);mouse(32,17,3);mouse(0,17,3,True);cmd('window show')
  check('window src x=1 y=3 width=60' in c.model.text(),'plain title-row drag resizes width without Shift')
  mouse(0,5,3);mouse(32,1,3);mouse(0,1,3,True);cmd('window resize src 48')
  cmd('layout config save '+str(a.out/'columns.layout'));cmd('window resize asm 40');cmd('window resize asm 48');cmd('window resize src 48');cmd('window show')
  check(all(f'window {pane} x={x} y=3 width=48' in c.model.text() for pane,x in [('src',1),('wasm',49),('asm',97)]),'nested exact widths permit three thirds')
  cmd('layout config save '+str(a.out/'exact.layout'));before=(a.out/'exact.layout').read_bytes();cmd('start');cmd('layout config save '+str(a.out/'restart.layout'))
  check(before==(a.out/'restart.layout').read_bytes(),'start preserves exact rational column sizes')
  cmd('tui disable');c.finish()
 finally:(a.out/'transcript.bin').write_bytes(c.log);(a.out/'actions.json').write_text(json.dumps(actions,indent=2));c.close()
 if a.unmapped_wasm:
  c=Driver([str(a.uwvm.resolve()),'-Rdbg','-Rct','0','-Rllvm-cache-path','disable','--run',str(a.unmapped_wasm.resolve())])
  try:
   c.wait(lambda b:b'(uwvm-debug) ' in b,0);c.drain()
   for instruction in ['n','s']:
    c.ask('startw', b'stopped: first Wasm instruction');raw=c.ask(instruction, b'source ')
    check(b'source stepping unavailable' not in raw and re.search(rb'\bsource .+:\d+:',raw),'source step seeks mapped statement from startup: '+instruction)
   c.finish()
  finally:(a.out/'unmapped-transcript.bin').write_bytes(c.log);c.close()
 (a.out/'result.json').write_text(json.dumps({'passed':True,'checks':checks,'actions':len(actions),'cgroup':a.expected_cgroup},indent=2))
 print('PASS scroll/resize',len(checks))
if __name__=='__main__':main()
