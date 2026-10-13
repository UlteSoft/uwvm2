#!/usr/bin/env python3
"""A native finish timeout/Ctrl+C must retain an actual cooperative pause."""
import argparse,json,re,time
from pathlib import Path
from run_debug_console_tui_color import Driver
from run_debug_run_cli import require

def fixture(finite_return=False):
 def leb(n):
  out=bytearray()
  while n>=128:out.append((n&127)|128);n>>=7
  out.append(n);return bytes(out)
 def section(n,b):return bytes([n])+leb(len(b))+b
 def name(s):b=s.encode();return leb(len(b))+b
 start=bytes.fromhex('00 10 01 0b');loop=bytes.fromhex('00 03 40 23 00 41 01 6a 24 00 0c 00 0b 0b')
 # Linux's physical finish requires a real unwindable return continuation.
 # A 2**32-iteration conditional loop keeps a native return in the generated
 # function while remaining far longer than the timeout/cancellation window.
 if finite_return:loop=bytes.fromhex('00 03 40 23 00 41 01 6a 24 00 23 00 45 45 0d 00 0b 0b')
 return (b'\0asm\x01\0\0\0'+section(1,bytes.fromhex('01 60 00 00'))+section(3,bytes.fromhex('02 00 00'))
  +section(6,bytes.fromhex('01 7f 01 41 00 0b'))+section(7,leb(1)+name('_start')+bytes([0,0]))
  +section(10,leb(2)+leb(len(start))+start+leb(len(loop))+loop))
def main():
 p=argparse.ArgumentParser();p.add_argument('--uwvm',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--finite-return',action='store_true');a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False);w=a.out/'loop.wasm';w.write_bytes(fixture(a.finite_return))
 c=Driver([str(a.uwvm.resolve()),'-Rdbg','-Rct','0','-Rllvm-cache-path','disable','--run',str(w.resolve())]);actions=[]
 def command(text):
  r=c.ask(text).decode('utf8','replace');actions.append([text,r]);return r
 def stable_pause(label):
  time.sleep(.1);first=command('status');time.sleep(.1);second=command('status')
  require('stopped:' in first and 'stopped:' in second,(label,first,second));require(re.search(r'stop-id (\d+)',first)[1]==re.search(r'stop-id (\d+)',second)[1],label+' does not auto-resume')
 try:
  c.wait(lambda b:b'(uwvm-debug) ' in b,0);c.drain();command('startw');reply=command('bw 0 1 0');point=re.search(r'breakpoint (\d+) registered',reply)[1];command('c');command('delete '+point)
  require('native-pc=' in command('si'),'actual native stop before finish');reply=command('fi');require('error:' not in reply,reply);stable_pause('timeout')
  require('native-pc=' in command('si'),'re-enter native after timeout');c.send('fi\n');c.send(b'\x03');c.drain();stable_pause('Ctrl+C')
  c.finish();(a.out/'result.json').write_text(json.dumps({'passed':True,'cases':['timeout retains pause','Ctrl+C retains pause'],'actions':actions},indent=2))
 finally:(a.out/'transcript.bin').write_bytes(c.log);c.close()
if __name__=='__main__':main()
