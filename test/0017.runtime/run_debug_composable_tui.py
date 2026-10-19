#!/usr/bin/env python3
"""Human-style real PTY regression, in the caller's verified Linux cgroup."""
import argparse,json,os,re,time,fcntl,termios,struct
from pathlib import Path
from run_debug_console_tui_color import Driver,Terminal
from run_debug_run_cli import require,wasm_fixture

def main():
 p=argparse.ArgumentParser();p.add_argument('--uwvm',required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--expected-cgroup');p.add_argument('--wasm',type=Path)
 a=p.parse_args()
 if a.expected_cgroup:require(Path('/proc/self/cgroup').read_text().strip()=='0::'+a.expected_cgroup,'cgroup')
 a.out.mkdir(parents=True,exist_ok=True);wasm=a.wasm or a.out/'tui.wasm'
 if not a.wasm:wasm.write_bytes(wasm_fixture())
 c=Driver([str(Path(a.uwvm).resolve()),'-Rdbg','-Rct','0','-Rllvm-cache-path','disable','--run',str(wasm.resolve())]);actions=[]
 def action(data,contains=None):
  at=len(c.log);c.send(data);text=c.model.text();raw=bytes(c.log[at:]);actions.append({'keys':repr(data),'screen':text});(a.out/'actions.json').write_text(json.dumps(actions,indent=2))
  if contains:require(contains in text,(data,contains,text))
  return raw,text
 try:
  c.wait(lambda b:b'(uwvm-debug) ' in b,0);c.drain()
  raw,_=action('  d\t');require(b'\x1b[?1049h' not in raw,'line completion must not enter alternate screen');action(b'\x15')
  raw,text=action('help \t\t');require(not c.model.alternate and b'\x1b[2J' not in raw,'shell completion appends');action(b'\x15')
  action('bs \t','bs 0');action(b'\x15');action('  bw   \t','bw   0');action(b'\x15')
  action('tui enable\n','layout src');require(c.model.alternate,'TUI enabled')
  action('help\n','help COMMAND')
  raw,text=action('startw\n','layout wasm');require('=> 0' in text and 'global.get' in text,(text,'real Wasm listing'))
  action('layout wasm locals stack memory\n','[memory');action('focus stack\n','[operands *]');action('memory 0 0 32 16\n')
  action('sw\n');action('layout stack\n','[0] · i32 = 7')
  _,text=action('layout wasm\n','i32.const');require('=> 2' in text,('current Wasm instruction after global.get',text))
  action('layout horizontal src asm\n','[asm');action('layout wasm\n')
  for launch in ['startw','starti','startw']:
   raw,text=action(launch+'\n');require(c.model.alternate and b'\x1b[?1049l' not in raw,'launch retains TUI: '+launch)
   require('layout wasm' in text,(launch,'explicit layout preserved',text))
   if launch=='starti':
    _,native=action('layout asm\n');require(native.count('=> 0x')==1,('one actual native PC marker',native));action('layout wasm\n')
  action('bw 0 1 0\n');raw,text=action('run\n');require(c.model.alternate and b'\x1b[?1049l' not in raw,'run retains screen')
  action('tui disable\n');require(not c.model.alternate,'disable restores screen')
  reply=c.command('starti');require(b'error:' not in reply,reply)
  reply=c.command('bi 65536');require(b'error:' in reply,'asm boundary refused')
  for cmd in ['startw','sw','next wasm','startw','finish wasm']:
   reply=c.command(cmd);require(b'Undefined' not in reply and b'Usage:' not in reply,(cmd,reply))
  c.finish()
  (a.out/'result.json').write_text(json.dumps({'passed':True,'actions':len(actions),'cgroup':a.expected_cgroup},indent=2))
 finally:
  (a.out/'transcript.bin').write_bytes(c.log);c.close()
if __name__=='__main__':main()
