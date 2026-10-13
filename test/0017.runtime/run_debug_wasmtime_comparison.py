#!/usr/bin/env python3
"""Wasmtime native DWARF/GDB and instrumented Wasm/LLDB comparisons on Linux.

Kernel CLOCK_MONOTONIC excludes compilation, debugger startup and setup.
The same .wasm is used in every mode; each backend has its own baseline.
Real PTYs exercise both debugger frontends. Preserve all transcripts.
"""
import argparse,hashlib,json,os,re,socket,statistics,subprocess,time
import select,termios
from pathlib import Path
from run_debug_run_cli import Console


def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def ask(c,command,prompt=rb'\(gdb\) $',timeout=180,marker=None):
    at=len(c.log);os.write(c.fd,command.encode()+b'\n')
    return c.wait(lambda b:re.search(prompt,b) and (marker is None or marker in b),at,timeout)


def finish(c):
    # GDB and LLDB restore terminal settings after a normal quit.
    c.finish()


def finish_batch(c):
    deadline=time.monotonic()+10
    while time.monotonic()<deadline:
        if select.select([c.fd],[],[],.02)[0]:
            try:c.log.extend(os.read(c.fd,65536))
            except OSError:pass
        got,code=os.waitpid(c.pid,os.WNOHANG)
        if got:
            c.reaped=True;assert os.waitstatus_to_exitcode(code)==0,bytes(c.log)[-4000:]
            restored=termios.tcgetattr(c.fd);expected=c.baseline.copy()
            if hasattr(termios,'PENDIN'):restored[3]&=~termios.PENDIN;expected[3]&=~termios.PENDIN
            assert restored==expected,'batch frontend must restore the PTY'
            return
    raise AssertionError('batch frontend exit deadline')


def wasm_run(wt,lldb,wasm,mode,out):
    flags=['run','-C','cache=n','-O','opt-level=0' if mode.endswith('o0') else 'opt-level=2']
    if mode.startswith(('debug','gdb')):flags+=['-D','debug-info']
    if mode=='guest-instrument':flags+=['-D','guest-debug']
    base=[str(wt),*flags,str(wasm)]
    begin=time.perf_counter();startup=None
    if mode.startswith('gdb'):
        c=Console(['/usr/bin/gdb','-q','-nx','--args',*base])
        try:
            c.wait(lambda b:re.search(rb'\(gdb\) $',b),0,30)
            ask(c,'set pagination off');ask(c,'set confirm off')
            startup=time.perf_counter()-begin
            r=ask(c,'run');assert b'exited normally' in r,r[-3000:]
            finish(c);output=bytes(c.log)
        finally:out.write_bytes(c.log);c.close()
    elif mode=='guest-lldb':
        with socket.socket() as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
        # The server is a direct owned child: wait/terminate never touches an unrelated PID.
        with out.with_suffix('.server.bin').open('xb') as f:
            server=subprocess.Popen([str(wt),*flags,'-g',str(port),str(wasm)],stdout=f,stderr=subprocess.STDOUT)
            c=None
            try:
                deadline=time.monotonic()+60
                while b'Debugger listening' not in out.with_suffix('.server.bin').read_bytes():
                    assert server.poll() is None,out.with_suffix('.server.bin').read_bytes()
                    assert time.monotonic()<deadline,'server ready deadline'
                    time.sleep(.02)
                # LLDB's batch driver executes commands synchronously. Its
                # interactive prompt can precede the asynchronous stop event.
                c=Console([str(lldb),'--no-lldbinit','--batch',
                    '-o',f'log enable gdb-remote packets -f {out.with_suffix(".packets")}',
                    '-o',f'process connect --plugin wasm connect://127.0.0.1:{port}',
                    '-o','process status','-o','continue'])
                c.wait(lambda b:b'Process 1 stopped' in b,0,45)
                startup=time.perf_counter()-begin
                r=c.wait(lambda b:b'exited with status = 0' in b,0,180)
                # Normal Wasm completion is reported as a remote process exit.
                assert b'exited with status = 0' in r,r[-3000:]
                finish_batch(c);server.wait(timeout=10)
                assert server.returncode==0
                output=bytes(c.log)+out.with_suffix('.server.bin').read_bytes()
            finally:
                if c is not None:out.write_bytes(c.log);c.close()
                if server.poll() is None:server.terminate()
                try:server.wait(timeout=5)
                except subprocess.TimeoutExpired:server.kill();server.wait()
    else:
        r=subprocess.run(base,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180)
        assert r.returncode==0,r.stdout[-3000:]
        output=r.stdout;out.write_bytes(output)
    elapsed=time.perf_counter()-begin
    ns=re.findall(rb'Kernel ns: (\d+)',output)
    ms=re.findall(rb'Time: ([0-9.]+) ms',output)
    sums=re.findall(rb'Checksum: ([^\r\n]+)',output)
    assert len(sums)==1 and len(ns)+len(ms)==1,output[-3000:]
    return {'kernel_ns':int(ns[0]) if ns else round(float(ms[0])*1e6),
            'checksum':sums[0].decode(),'process_s':elapsed,'ready_s':startup,'argv':base}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['wasmtime','lldb','fixtures','out']:p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--fft',type=Path);p.add_argument('--repeats',type=int,default=7)
    p.add_argument('--mode',action='append');p.add_argument('--only',action='append')
    p.add_argument('--cpu',type=int,default=16,help='CPU already selected by the guarded timing runner.')
    a=p.parse_args();a.out.mkdir(parents=True,exist_ok=False)
    assert os.sched_getaffinity(0)=={a.cpu},f'pin the serial timing run to CPU{a.cpu}'
    os.environ.update(TERM='dumb',NO_COLOR='1',DEBUGINFOD_URLS='')
    files=[(x['name'],a.fixtures/(x['name']+'.wasm'),x['wasm_sha256']) for x in json.loads((a.fixtures/'manifest.json').read_text())]
    if a.fft:files.append(('fft',a.fft,digest(a.fft)))
    if a.only:files=[x for x in files if x[0] in a.only]
    modes=a.mode or ['ordinary','debug-o2','gdb-o2','guest-instrument','guest-lldb','debug-o0','gdb-o0']
    result={'passed':False,'cgroup':Path('/proc/self/cgroup').read_text().strip(),'affinity':sorted(os.sched_getaffinity(0)),
            'wasmtime':str(a.wasmtime),'wasmtime_sha256':digest(a.wasmtime),'lldb_sha256':digest(a.lldb),
            'fixtures':[],'samples':[],'notes':'Unprofiled, serial; CLOCK_MONOTONIC kernel excludes JIT/startup/attach/setup. O0/O2 are the Wasmtime CLI optimization settings, using Cranelift.'}
    for name,wasm,sha in files:
        assert digest(wasm)==sha
        result['fixtures'].append({'name':name,'path':str(wasm),'sha256':sha})
        expected=None
        for rep in range(a.repeats):
            for mode in modes if rep%2==0 else list(reversed(modes)):
                row=wasm_run(a.wasmtime,a.lldb,wasm,mode,a.out/f'{name}-{mode}-{rep}.bin')
                expected=row['checksum'] if expected is None else expected
                assert expected==row['checksum'],(name,expected,row)
                result['samples'].append({'name':name,'mode':mode,'repeat':rep,**row})
                (a.out/'result.json').write_text(json.dumps(result,indent=2))
                print(name,mode,rep,row['kernel_ns']/1e6,'ms',flush=True)
    result['medians_ms']={f'{n}/{m}':statistics.median(x['kernel_ns']/1e6 for x in result['samples'] if (x['name'],x['mode'])==(n,m)) for n,m in {(x['name'],x['mode']) for x in result['samples']}}
    result['passed']=True;(a.out/'result.json').write_text(json.dumps(result,indent=2))


if __name__=='__main__':main()
