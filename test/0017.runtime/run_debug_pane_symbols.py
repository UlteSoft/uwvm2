#!/usr/bin/env python3
"""Production PTY: compact coloured values, symbol operands and explicit config paths."""
import argparse,fcntl,json,os,re,struct,termios
from pathlib import Path
from run_debug_console_tui_color import Driver
from run_debug_rich_tui import RichTerminal
from run_debug_run_cli import require

def fixture(named=True):
 def leb(n):
  out=bytearray()
  while n>=128:out.append((n&127)|128);n>>=7
  out.append(n);return bytes(out)
 def text(s):b=s.encode();return leb(len(b))+b
 def section(i,b):return bytes([i])+leb(len(b))+b
 types=section(1,bytes.fromhex('02 60 00 00 60 01 7f 01 7f'))
 funcs=section(3,bytes.fromhex('02 00 01'))
 memory=section(5,bytes.fromhex('01 00 01'))
 glob=section(6,bytes.fromhex('01 7f 01 41 07 0b'))
 exports=section(7,leb(4)+text('_start')+bytes([0,0])+text('leaf_export')+bytes([0,1])+text('counter_export')+bytes([3,0])+text('memory')+bytes([2,0]))
 main=bytes.fromhex('01 01 7f 41 07 22 00 10 01 1a 23 00 1a 0b');leaf=bytes.fromhex('00 20 00 0b')
 code=section(10,leb(2)+leb(len(main))+main+leb(len(leaf))+leaf)
 names=section(0,text('name')+section(1,leb(2)+leb(0)+text('_start')+leb(1)+text('_Z4leafi'))+section(7,leb(1)+leb(0)+text('_Z7counter')))
 return b'\0asm\x01\0\0\0'+types+funcs+memory+glob+exports+code+(names if named else b'')

def main():
 p=argparse.ArgumentParser();p.add_argument('--uwvm',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--expected-cgroup');a=p.parse_args()
 a.out.mkdir(parents=True,exist_ok=False);checks=[];os.environ.pop('NO_COLOR',None);os.environ['TERM']='xterm-256color'
 if a.expected_cgroup:require(Path('/proc/self/cgroup').read_text().strip()=='0::'+a.expected_cgroup,'cgroup')
 for named in [True,False]:
  wasm=a.out/('named.wasm' if named else 'export.wasm');wasm.write_bytes(fixture(named))
  c=Driver([str(a.uwvm.resolve()),'-Rdbg','-Rct','0','-Rllvm-cache-path','disable','--run',str(wasm.resolve())])
  def check(ok,label):require(ok,(label,c.model.text()));checks.append(label)
  def cmd(text):return c.send(text+'\n')
  def color(raw,code,text=b''):return re.search(rb'\x1b\[(?:0;)?'+str(code).encode()+b'm'+re.escape(text),raw)
  def panel_text():return c.model.text().split('[io]')[0]
  try:
   c.wait(lambda b:b'(uwvm-debug) ' in b,0);c.drain();c.ask('startw',b'stopped: first Wasm instruction');cmd('tui enable')
   fcntl.ioctl(c.fd,termios.TIOCSWINSZ,struct.pack('HHHH',55,144,0,0));c.model=RichTerminal(144,55);c.model.alternate=True;c.model.primary=([[' ']*144 for _ in range(55)],0,0);c.painted=len(c.log);c.drain()
   raw=cmd('layout wasm');screen=panel_text()
   check(('call 1  <leaf(int)>' if named else 'call 1  <leaf_export>') in screen,'demangled function' if named else 'export function fallback')
   check(('global.get 0  <counter>' if named else 'global.get 0  <counter_export>') in screen,'global symbol' if named else 'export global fallback')
   check(color(raw,95,b'call') and color(raw,96,b'1') and color(raw,92,b'<'),'Wasm mnemonic, immediate and symbol colours')
   check('bytes:' not in screen,'decoded operands omit redundant raw bytes')
   raw=cmd('layout asm');check(color(raw,95) and color(raw,94),'ASM opcode/register colours')
   cmd('nw');cmd('nw');raw=cmd('layout locals stack');screen=panel_text()
   check('i32 [0] = 7' in screen,'compact actual local and operand value')
   check(all(noise not in screen for noise in ['wasm-stop','Wasm state thread','epoch=','first=','Note:']),'value panes omit protocol fields')
   check(color(raw,96,b'[0]') and color(raw,94,b'i32') and color(raw,93,b'7'),'index/type/value distinct colours')
   before=a.out/('before'+str(named)+'.layout');after=a.out/('after'+str(named)+'.layout');cmd('layout config save '+str(before.resolve()))
   for action in ['save','load']:
    raw=cmd('layout config '+action+'   ');check(b'<filePath>' in raw,'missing '+action+' path rejected')
   cmd('layout config save '+str(after.resolve()));check(before.read_bytes()==after.read_bytes(),'missing paths preserve layout')
   cmd('help layout config');check('no default' in ' '.join(c.model.text().replace('|',' ').split()),'help documents required paths')
   (a.out/('named.screen.txt' if named else 'export.screen.txt')).write_text(c.model.text())
   cmd('tui disable');c.finish()
  finally:(a.out/('named.bin' if named else 'export.bin')).write_bytes(c.log);c.close()
 (a.out/'result.json').write_text(json.dumps({'passed':True,'checks':checks,'cgroup':a.expected_cgroup},indent=2));print('PASS pane symbols',len(checks))
if __name__=='__main__':main()
