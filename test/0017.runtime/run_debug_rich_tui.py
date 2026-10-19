#!/usr/bin/env python3
"""Real PTY regression for selection, nested panes, memory, hover and idle resize."""
import argparse,base64,fcntl,json,re,struct,termios,time
from pathlib import Path
from run_debug_console_tui_color import Driver,Terminal
from run_debug_run_cli import require

class RichTerminal(Terminal):
 def __init__(self,*a):super().__init__(*a);self.osc_state=0
 def feed(self,data):
  visible=bytearray()
  for b in data:
   if self.osc_state==0:
    if b==27:self.osc_state=1
    else:visible.append(b)
   elif self.osc_state==1:
    if b==93:self.osc_state=2
    else:visible.extend((27,b));self.osc_state=0
   elif self.osc_state==2:
    if b==7:self.osc_state=0
    elif b==27:self.osc_state=3
   elif self.osc_state==3:self.osc_state=0 if b==92 else 2
  super().feed(bytes(visible))

def main():
 p=argparse.ArgumentParser();p.add_argument('--uwvm',type=Path,required=True);p.add_argument('--wasm',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--expected-cgroup')
 a=p.parse_args()
 if a.expected_cgroup: require(Path('/proc/self/cgroup').read_text().strip()=='0::'+a.expected_cgroup,'bounded cgroup')
 a.out.mkdir(parents=True,exist_ok=False);rows=[]
 c=Driver([str(a.uwvm.resolve()),'-Rdbg','-Rct','0','-Rllvm-cache-path','disable','--run',str(a.wasm.resolve())])
 def record(name,raw=b''):
  rows.append({'name':name,'screen':c.model.text(),'raw':raw.decode('utf8','backslashreplace')});(a.out/'actions.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2));return raw,c.model.text()
 def send(keys):return record(repr(keys),c.send(keys))
 def cmd(s):return send(s+'\n')
 def mouse(b,x,y,release=False):return send(f'\x1b[<{b};{x};{y}'+('m' if release else 'M'))
 def resize(w,h):
  at=len(c.log);fcntl.ioctl(c.fd,termios.TIOCSWINSZ,struct.pack('HHHH',h,w,0,0));c.model=RichTerminal(w,h);c.model.alternate=True;c.model.primary=([[' ']*w for _ in range(h)],0,0);c.painted=at
  c.drain();raw=bytes(c.log[at:]);record(f'resize {w}x{h} without key',raw);require(b'\x1b[2J' in raw,'idle resize cleared');require('(uwvm-debug)' in c.model.text(),'idle resize prompt')
 try:
  c.wait(lambda b:b'(uwvm-debug) ' in b,0);c.drain();cmd('start');cmd('tui enable');resize(120,45)
  cmd('not-a-valid-command');cmd('memory nope');send('draft')
  send(b'\x1b[A');require(c.model.line()=='(uwvm-debug) memory nope','failed command retained')
  send(b'\x1b[A');require(c.model.line()=='(uwvm-debug) not-a-valid-command','unknown command retained')
  send(b'\x1b[B\x1b[B');require(c.model.line()=='(uwvm-debug) draft','down restores draft');send(b'\x15')
  cmd('layout horizontal src wasm asm');config_before=a.out/'before-start.layout';cmd('layout config save '+str(config_before));cmd('start')
  config_after=a.out/'after-start.layout';cmd('layout config save '+str(config_after));require(config_before.read_bytes()==config_after.read_bytes(),'start preserves complete layout')
  send(b'\x1b[A');require(c.model.line()=='(uwvm-debug) layout config save '+str(config_after),'history after restart');send(b'\x15')
  raw,_=send('x');require(len(raw)<256 and b'\x1b[2J' not in raw and b'[src' not in raw,'input update is incremental');send(b'\x15')
  cmd('layout src');cmd('n');cmd('n');raw,text=cmd('inspect src a');require('volatile int a = 7' in text and 'source-value stop' not in text,'compact source inspection')
  lines=c.model.text().splitlines();line_index=next(i for i,l in enumerate(lines) if 'volatile int a=7' in l)
  x=lines[line_index].index('int ')+1;raw,text=mouse(35,x,line_index+1);require('[inspect' not in text,'keyword has no empty tooltip')
  x=lines[line_index].index('a=7')+1;raw,text=mouse(35,x,line_index+1);require('[inspect' in text and 'volatile int a = 7' in text,'actual value hover')
  mouse(35,120,2);require('[inspect' not in c.model.text(),'leaving dismisses hover')
  cmd('layout memory');cmd('memory 0 0 65536 16')
  require('0x0000000000010000' in c.model.text() and '|AB..' in c.model.text(),'address and ASCII')
  mouse(0,23,5);mouse(32,33,5);raw,text=mouse(0,33,5,True)
  require('i8 = 65' in text and 'u32 = 4278207041' in text,('exact selected bytes interpreted',text))
  clips=re.findall(rb'\x1b\]52;c;([^\x07]*)\x07',raw);require(clips and b'41 42 00 ff' in base64.b64decode(clips[-1]),'memory text copied as data')
  cmd('selection clear');cmd('memory format u8');_,text=cmd('memory select 65539 1');require('u8 = 255' in text,'typed memory interpretation')
  cmd('layout tree v70(h50(src,memory),io)');cmd('window show');require('window memory x=61 y=3' in c.model.text(),'top left/right split')
  config=a.out/'layout with spaces.conf';cmd('layout config save '+str(config));require(config.exists(),'layout saved')
  before=config.read_bytes();cmd('layout asm');cmd('layout config load '+str(config));cmd('window show');require('window memory x=61 y=3' in c.model.text(),'layout restored')
  bad=a.out/'bad.conf';bad.write_text('uwvm-layout 1\nh50(src,src)\n0\n');raw,_=cmd('layout config load '+str(bad));require(b'current layout retained' in raw,'invalid load rejected');cmd('layout config save '+str(config));require(config.read_bytes()==before,'invalid load preserves complete tree')
  cmd('layout invalid-pane');cmd('layout config save '+str(config));require(config.read_bytes()==before,'invalid pane leaves nested layout intact')
  cmd('layout add wasm below memory');cmd('layout add asm right wasm');cmd('window show');require('window wasm' in c.model.text() and 'window asm' in c.model.text(),'add independent panes');cmd('layout remove wasm');cmd('layout config load '+str(config))
  cmd('layout move memory below src');cmd('window show');require('window memory x=1' in c.model.text(),'nested move collapse')
  cmd('layout src');raw,text=cmd('list src');source=next((line for line in text.splitlines() if 'int ' in line),None)
  # Select command history across two rows, excluding pane borders and SGR.
  cmd('help wasip1 io');lines=c.model.text().splitlines();y=next((i+1 for i,l in enumerate(lines) if 'IO records show' in l),None)
  require(y is not None,('IO help present',c.model.text()));x=lines[y-1].index('IO records')+1
  mouse(0,x,y);mouse(32,x+9,y);raw,_=mouse(0,x+9,y,True);clips=re.findall(rb'\x1b\]52;c;([^\x07]*)\x07',raw);require(clips and base64.b64decode(clips[-1])==b'IO records','body selects instead of dragging')
  raw,_=send(b'\x03');require(b'\x1b]52;c;' in raw,'CtrlC copies active selection')
  send(b'\x16');require(c.model.line()=='(uwvm-debug) IO records','CtrlV pastes selected text');send(b'\x15')
  cmd('selection clear');cmd('layout tree h50(src,memory)');_,text=cmd('window show');beforelines=[l for l in text.splitlines() if 'window src x=' in l or 'window memory x=' in l]
  # A non-label border drag must not resize/dock panes.
  mouse(0,45,3);mouse(32,75,10);mouse(0,75,10,True);raw,_=cmd('window show');require('window src x=1 y=3 width=60' in c.model.text(),'border cannot start window drag')
  mouse(0,5,3);mouse(32,119,4);require('Move src right of memory' in c.model.text(),'horizontal docking preview near top edge');mouse(0,119,4,True);raw,_=cmd('window show');require('window src x=61' in c.model.text(),'title drag docks to right')
  send('  sta');resize(200,55);require('(uwvm-debug)   sta' in c.model.text(),'resize preserves command draft');resize(80,24);resize(120,45);send(b'\x15')
  cmd('layout wasm');cmd('ni');_,text=cmd('list wasm');require('N>' in text and 'Wasm position unavailable at this native' not in text,'native stop retains qualified Wasm display')
  cmd('start');cmd('layout io');cmd('continue');raw,text=cmd('io show');require(re.search(r'\[[0-9]{4}-[^\]]*(?:[+-][0-9]{2}:[0-9]{2}|Z)\] \[stdout\]',text),'local ISO 8601 IO records')
  export=a.out/'io.txt';cmd('io save '+str(export));require('[stdout]' in export.read_text() and 'UTC' not in export.read_text() and 'LOCAL' not in export.read_text(),'export output')
  cmd('io clear');empty=a.out/'empty.txt';cmd('io save '+str(empty));require(empty.read_bytes()==b'','IO clear')
  for topic in ['layout config','selection','inspect','wasip1 io','memory']:
   raw,_=cmd('help '+topic);require(b'Unknown' not in raw and b'unknown command' not in raw,'help '+topic)
  cmd('tui disable');require(not c.model.alternate,'terminal restored');c.finish()
  (a.out/'result.json').write_text(json.dumps({'passed':True,'actions':len(rows),'cgroup':a.expected_cgroup},indent=2))
 finally:(a.out/'transcript.bin').write_bytes(c.log);c.close()
if __name__=='__main__':main()
