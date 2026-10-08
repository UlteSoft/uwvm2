#!/usr/bin/env python3
"""Capture real MCJIT objects for dbg-off versus dbg-on inspection in the cgroup.

The test runtime must be built with -DUWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT=1.
Ordinary products deliberately do not expose this object-file capture hook.
"""
import argparse
import hashlib
import json
import os
import subprocess
import time
from pathlib import Path
from run_wasm_operand_preview_cli import OperandConsole


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('source-root','binary','wasm-tools','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--runner-prefix-json',type=Path,help='JSON argv prefix for a real target VM under QEMU user mode.')
    p.add_argument('--prompt-timeout',type=int,default=120)
    p.add_argument('--stop-timeout',type=int,default=20)
    a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False)
    assert 1 <= a.prompt_timeout <= 600 and 1 <= a.stop_timeout <= 120
    OperandConsole.prompt_timeout=a.prompt_timeout
    runner=json.loads(a.runner_prefix_json.read_text()) if a.runner_prefix_json else []
    assert isinstance(runner,list) and all(isinstance(x,str) and x for x in runner)
    subprocess.run(['bash',str(a.source_root/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
    wat=a.out/'numeric.wat';wasm=wat.with_suffix('.wasm')
    wat.write_text('''(module
      (func $leaf (export "leaf") (param i32 i64) (result i64)
        local.get 1 local.get 0 i64.extend_i32_s i64.add)
      (func (export "_start") i32.const 37 i64.const 1000 call $leaf drop))\n''')
    subprocess.run([str(a.wasm_tools),'parse',str(wat),'-o',str(wasm)],check=True)
    subprocess.run([str(a.wasm_tools),'validate','--features','all',str(wasm)],check=True)
    argv=[str(a.binary),'-Rct','0','-Rllvm-cache-path','disable','-Rllvm-call-stack','unwind','--run',str(wasm)]
    rows=[]
    for enabled in (False,True):
        name='on' if enabled else 'off';obj=a.out/(name+'.o')
        os.environ['UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT']=str(obj)
        c=None
        try:
            if enabled:
                c=OperandConsole([*runner,argv[0],'-Rdbg',*argv[1:]],a.out/'on.log');c.prompt()
                assert b'registered' in c.send('break 0 0 6') # actual final end, pre-op stack contains the result
                c.send('continue');limit=time.monotonic()+a.stop_timeout
                while b'stopped:' not in c.send('status'):
                    assert time.monotonic()<limit;time.sleep(.01)
                thread,function,offset=c.location();assert (function,offset)==(0,6)
                reply=c.send(f'operands {thread}');assert b'operand 0 i64 = 1037' in reply,reply
                c.send('continue');limit=time.monotonic()+a.stop_timeout
                while True:
                    reply=c.send('status')
                    if b'guest exited: 0' in reply:break
                    assert time.monotonic()<limit,reply;time.sleep(.01)
            else:
                with (a.out/'off.log').open('wb') as output:
                    subprocess.run([*runner,argv[0],'-Raot',*argv[1:]],stdout=output,stderr=subprocess.STDOUT,check=True,timeout=a.prompt_timeout)
            assert obj.is_file() and obj.read_bytes()[:4]==b'\x7fELF',obj
            rows.append(dict(dbg=enabled,actual_VM=True,object_bytes=obj.stat().st_size,
                object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest(),passed=True))
        finally:
            if c is not None:c.close()
            os.environ.pop('UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT',None)
    (a.out/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
    (a.out/'inputs.json').write_text(json.dumps(dict(binary_sha256=hashlib.sha256(a.binary.read_bytes()).hexdigest(),
        harness_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),runner_prefix=runner,
        runner_sha256=hashlib.sha256(Path(runner[0]).read_bytes()).hexdigest() if runner else None,
        cgroup=Path('/proc/self/cgroup').read_text()),indent=2)+'\n')
    print(json.dumps(rows),flush=True)


if __name__=='__main__':main()
