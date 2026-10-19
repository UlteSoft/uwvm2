#!/usr/bin/env python3
"""Real PTY mouse, divider, unlimited mapped source and cross-level regression."""
import argparse,fcntl,json,re,struct,termios
from pathlib import Path
from run_debug_console_tui_color import Driver,Terminal
from run_debug_run_cli import require

def main():
 p=argparse.ArgumentParser();p.add_argument('--uwvm',type=Path,required=True);p.add_argument('--wasm',type=Path,required=True)
 p.add_argument('--source',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--expected-cgroup')
 a=p.parse_args()
 if a.expected_cgroup:require(Path('/proc/self/cgroup').read_text().strip()=='0::'+a.expected_cgroup,'cgroup')
 a.out.mkdir(parents=True,exist_ok=False);mount=a.out/'mount';mount.mkdir();(mount/'open.txt').write_text('mouse test')
 large=a.out/'large.c';large.write_text(''.join('// line %05d '%i+'x'*1000+'\n' for i in range(1,5001)))
 c=Driver([str(a.uwvm.resolve()),'-Rdbg','-Rct','0','-Rllvm-cache-path','disable','--wasip1-global-mount-dir','/sandbox',str(mount.resolve()),'--run',str(a.wasm.resolve())]);rows=[]
 def send(keys):
  raw=c.send(keys);rows.append({'keys':repr(keys),'screen':c.model.text(),'reply':re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',raw).decode(errors='replace')});(a.out/'actions.json').write_text(json.dumps(rows,indent=2));return raw,c.model.text()
 def cmd(s):return send(s+'\n')
 def mouse(button,x,y,release=False):return send(f'\x1b[<{button};{x};{y}'+('m' if release else 'M'))
 try:
  c.wait(lambda b:b'(uwvm-debug) ' in b,0);c.drain()
  fcntl.ioctl(c.fd,termios.TIOCSWINSZ,struct.pack('HHHH',45,120,0,0));c.model=Terminal(120,45);c.painted=len(c.log)
  markers={line.split('// ')[1]:i for i,line in enumerate(a.source.read_text().splitlines(),1) if '// ' in line}
  cmd(f'bs 0 {a.source.resolve()}:{markers["FILE_OPENED"]}');raw,_=cmd('run');require(b'stopped: breakpoint' in raw,'real opened descriptor stop')
  raw,_=cmd('info wasip1 fd 0 4');require(b'name=path_open' in raw and b'thread=1' in raw and b'created-fd=4' in raw,raw)
  raw,text=cmd('tui enable');require(b'\x1b[?1006h' in raw,'SGR mouse enabled')
  cmd('layout horizontal src wasm');cmd('set source '+str(large.resolve()));require('source text unavailable' not in c.model.text(),'source > 1 MiB mapped')
  first=re.search(r'line (\d+)',c.model.text());require(first is not None,'large source visible');first=int(first[1])
  send('sta');_,text=mouse(65,5,5);require(f'line {first+3:05}' in text and '(uwvm-debug) sta' in text,('wheel down preserves input',text))
  _,text=mouse(64,5,5);require(f'line {first:05}' in text,'wheel up');send(b'\x15')
  cmd('scroll src down 4096');require('line 04' in c.model.text(),'typed source scroll across mmap')
  cmd('scroll src up 4096');require(f'line {first:05}' in c.model.text(),'typed scroll returns')
  mouse(4,5,3);mouse(36,25,3);mouse(4,25,3,True)
  _,text=cmd('window show');require('window src x=1 y=3 width=80' in text,('title resize',text))
  mouse(4,5,3);mouse(36,100,3);mouse(4,100,3,True)
  _,text=cmd('window show');require('width=100' in text and 'width=20' in text,('minimum widths',text))
  _,text=cmd('window resize src 60');require('window wasm x=61' in text,'typed equivalent')
  cmd('layout wasip1');_,text=cmd('info wasip1 fd all');fdrow=next((n+1 for n,line in enumerate(text.splitlines()) if re.search(r'\bfd=4\b',line)),None)
  require(fdrow is not None,('descriptor row',text));mouse(0,8,fdrow);_,text=mouse(0,8,fdrow,True);require('name=path_open' in text,('descriptor click history',text))
  cmd('info wasip1 fd all');raw,_=cmd('mouse off');require(b'\x1b[?1006l' in raw,'disable mouse tracking')
  cmd('mouse on');raw,_=cmd('tui disable');require(b'\x1b[?1006l' in raw and not c.model.alternate,'leave restores terminal')
  cmd('list src');cmd('list wasm');cmd('list asm');cmd('help window');send('help mou\t');require('help mouse' in c.model.text(),'help completion');send(b'\x15')
  c.finish();(a.out/'result.json').write_text(json.dumps({'passed':True,'actions':len(rows),'source_bytes':large.stat().st_size,'cgroup':a.expected_cgroup},indent=2))
 finally:(a.out/'transcript.bin').write_bytes(c.log);c.close()
if __name__=='__main__':main()
