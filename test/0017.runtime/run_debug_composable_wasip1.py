#!/usr/bin/env python3
"""Real PTY, DWARF statements, live renamed descriptors and isolated guest IO."""
import argparse,json,os,re
from pathlib import Path
from run_debug_console_tui_color import Driver
from run_debug_run_cli import require

def main():
 p=argparse.ArgumentParser();p.add_argument('--uwvm',type=Path,required=True);p.add_argument('--wasm',type=Path,required=True)
 p.add_argument('--source',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--expected-cgroup')
 a=p.parse_args();a.source=a.source.resolve()
 if a.expected_cgroup:require(Path('/proc/self/cgroup').read_text().strip()=='0::'+a.expected_cgroup,'original cgroup')
 a.out.mkdir(parents=True,exist_ok=False);root=a.out/'mount';root.mkdir();old=root/'open.txt';old.write_text('test file')
 rows=[];markers={line.split('// ')[1]:i for i,line in enumerate(a.source.read_text().splitlines(),1) if '// ' in line}
 c=Driver([str(a.uwvm.resolve()),'-Rdbg','-Rct','0','-Rllvm-call-stack','instruction','-Rllvm-cache-path','disable',
  '--wasip1-global-mount-dir','/sandbox',str(root.resolve()),'--run',str(a.wasm.resolve())])
 def cmd(text):
  raw=c.send(text+'\n');raw=re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',raw);rows.append({'command':text,'reply':raw.decode(errors='replace'),'screen':c.model.text()})
  (a.out/'actions.json').write_text(json.dumps(rows,indent=2));return raw
 try:
  c.wait(lambda b:b'(uwvm-debug) ' in b,0);c.drain()
  cmd(f'bs 0 {a.source}:{markers["FILE_OPENED"]}');cmd('input TEST_STDIN')
  require(b'stopped: breakpoint' in cmd('run'),'source stop after opening file')
  require(b'error_code = 0' in cmd('print error_code'),'real path_open succeeded')
  fds=cmd('info wasip1 fds 0');require(str(old.resolve()).encode() in fds,'current opened pathname')
  renamed=root/'moved.txt';old.rename(renamed)
  fds=cmd('info wasip1 fds 0');require(str(renamed.resolve()).encode() in fds and str(old.resolve()).encode() not in fds,'rename reflected by live handle')
  cmd('tui enable');require('=> '+str(markers['FILE_OPENED']) in c.model.text(),'source exact breakpoint marker')
  cmd('sw');require('N> ' in c.model.text(),'source intermediate Wasm marker')
  cmd('layout wasm locals stack memory');require('[memory' in c.model.text(),'composed memory')
  cmd('layout wasip1');cmd('focus wasip1');c.send(b'\x1b[6~');c.send(b'\x1b[6~')
  rows.append({'command':'two PgDn','screen':c.model.text()})
  cmd('layout io');cmd('continue')
  screen=c.model.text();require('TUI_STDOUT' in screen and 'TUI_STDERR' in screen and 'TEST_STDIN' in screen,('isolated guest IO',screen))
  require(c.model.alternate,'exit keeps TUI')
  cmd('tui disable');cmd('startw')
  fds=cmd('info wasip1 fds 0');require(str(renamed.resolve()).encode() not in fds,'restart closes old file descriptors')
  cmd('delete 1');cmd('input PLAIN_STDIN');reply=cmd('continue')
  require(b'TUI_STDOUT' in reply and b'TUI_STDERR' in reply and b'PLAIN_STDIN' in reply,'ordinary guest IO mixed with commands')
  require(not c.model.alternate,'plain output stays on primary screen')
  c.finish();(a.out/'result.json').write_text(json.dumps({'passed':True,'actions':len(rows),'live_rename':True,'restart_reloads_environment':True,'source_exact_and_intermediate':True,'guest_stdio_both_modes':True},indent=2))
 finally:
  (a.out/'actions.json').write_text(json.dumps(rows,indent=2));(a.out/'transcript.bin').write_bytes(c.log);c.close()
if __name__=='__main__':main()
