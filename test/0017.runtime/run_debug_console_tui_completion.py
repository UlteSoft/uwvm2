#!/usr/bin/env python3
"""Real LLVM-full console PTY tests. Run only in the original keeper cgroup."""
import argparse, errno, fcntl, hashlib, importlib.util, json, os, re, select
import signal, struct, subprocess, termios, time
from pathlib import Path


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--uwvm', type=Path, required=True)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--cgroup', required=True)
    parser.add_argument('--clang', type=Path, required=True)
    parser.add_argument('--wasm-tools', type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--native-unavailable', action='store_true',
        help='Assert refusal when actual build receipts prove owner-table V2 disabled; no positive native qualification')
    parser.add_argument('--build-receipts', type=Path)
    args = parser.parse_args()
    assert Path('/proc/self/cgroup').read_text().strip() == '0::' + args.cgroup
    root = args.source_root.resolve(strict=True)
    output = args.out.resolve(); output.mkdir(exist_ok=False)
    driver_source = root / 'test/0017.runtime/run_debug_keyboard_product.py'
    # An independent observation barrier allows the supervisor to bind the PTY
    # child before exec. Product source and execution paths are unchanged.
    driver = output / 'pty_observer.py'
    driver.write_text(driver_source.read_text().replace('            os.execv(command[0], command)',
        '            time.sleep(.15)\n            os.execv(command[0], command)'))
    spec = importlib.util.spec_from_file_location('pty_observer', driver)
    mod = importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
    source = root / 'test/0017.runtime/fixtures/debug_console_completion_cpp.cc'
    wasm = output / 'completion.wasm'
    compiler = [str(args.clang), '--target=wasm32', '-nostdlib', '-std=c++20', '-O1', '-g', '-fno-exceptions',
        '-fno-rtti', '-Wl,--no-entry', '-Wl,--export=_start', str(source), '-o', str(wasm)]
    subprocess.run(compiler, check=True, timeout=30)
    subprocess.run([str(args.wasm_tools), 'validate', '--features', 'all', str(wasm)], check=True, timeout=10)
    if args.native_unavailable:
        assert args.build_receipts is not None,'actual all-TU build receipts required for disabled-owner expectation'
        repo='uwvm2-ros' if args.ros else 'uwvm2'
        receipts=json.loads(args.build_receipts.read_text())
        for part in ['main','runtime','host-api']:
            matches=[r for r in receipts if r['label']==repo+'-'+part+'-compile']
            assert len(matches)==1 and matches[0]['passed']
            flags=[v for v in matches[0]['argv'] if 'UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2' in v]
            assert not flags or flags==['-DUWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2=0'],flags
        proof=json.loads((args.uwvm.parent/(repo+'-build-proof.json')).read_text())
        assert proof['passed'] and proof['all_product_TUs_fresh'] and proof['binary_sha256']==sha(args.uwvm)
    rows = []
    def save():
        (output / 'results.json').write_text(json.dumps({'passed':all(r['passed'] for r in rows),
            'binary_sha256':sha(args.uwvm), 'source_sha256':sha(source), 'wasm_sha256':sha(wasm),
            'compiler':compiler, 'rows':rows}, indent=2)+'\n')
    prefix = [str(args.uwvm), '-Rdbg'] + (['-Raot'] if args.ros else ['-Rcc','jit','-Rcm','full']) + \
        ['-Rct','0','-Rllvm-cache-path','disable']
    for policy in ['instruction','unwind']:
        con = mod.Console(prefix+['-Rllvm-call-stack',policy,'--run',str(wasm)], 100)
        fd = os.pidfd_open(con.pid)
        row = {'case':policy,'passed':False,'observations':[]}
        try:
            fcntl.ioctl(con.fd,termios.TIOCSWINSZ,struct.pack('HHHH',42,180,0,0))
            con.wait(lambda b:b'UWVM LLVM full debugger.' in b and b'(uwvm-debug) ' in b)
            initial = con.send('status',b'prepared; no Wasm instruction executed')
            initial_stop = mod.stop_id(initial)
            def observed(label):
                row['observations'].append({'case':label,'passed':True})
            start = len(con.log); con.write(b'b 0 completion_t\t\n')
            breakpoint = con.wait(lambda b:b'registered at executable Wasm expression byte offset' in b,start)
            assert b'completion_target' in breakpoint and b'error:' not in breakpoint
            bp = int(re.search(rb'breakpoint (\d+) registered',breakpoint).group(1)); observed('real DWARF function completion and breakpoint')
            start = len(con.log); con.write(b'continue\n')
            stopped = con.wait(lambda b:b'stopped: breakpoint' in b and b'stop-id ' in b,start)
            assert mod.stop_id(stopped) != initial_stop
            thread = int(re.search(rb'thread (\d+) module=0',stopped).group(1))
            con.send(f'delete {bp}',b'(uwvm-debug) ')
            ready_line = next(i for i,line in enumerate(source.read_text().splitlines(),1) if 'COMPLETION_READY' in line)
            br = con.send(f'break-source 0 {source}:{ready_line}',b'byte-offset=')
            bp = int(re.search(rb'breakpoint (\d+) source=',br).group(1))
            start = len(con.log); con.write(b'continue\n')
            stopped = con.wait(lambda b:b'stopped: breakpoint' in b and b'stop-id ' in b,start)
            current = mod.stop_id(stopped)
            con.send(f'delete {bp}',b'(uwvm-debug) ')
            # Tab alone must never run a command or alter the current stop.
            start=len(con.log); con.write(b'print par\t')
            completed=con.wait(lambda b:b'(uwvm-debug) print parameter' in b,start)
            assert b'source-value stop=' not in completed and b'source parameter ' not in completed
            con.write(b'\n'); actual=con.wait(lambda b:b'type=' in b,start)
            assert b'parameter' in actual; observed('current variable completion remains input-only')
            start=len(con.log);con.write(b'ptype packet.fi\t\n')
            actual=con.wait(lambda b:b'source-type end' in b,start)
            assert b'name=packet.first' in actual and b'error:' not in actual; observed('aggregate member completion')
            start=len(con.log);con.write(b'ptype pointer->se\t\n')
            actual=con.wait(lambda b:b'source-type end' in b,start)
            assert b'name=pointer->second' in actual and b'error:' not in actual; observed('pointer member completion')
            start=len(con.log);con.write(b'frame variable par\t\n')
            actual=con.wait(lambda b:b'type=' in b,start);assert b'parameter' in actual;observed('LLDB variable completion')
            start=len(con.log);con.write(b'print par + 1'+b'\x02'*4+b'\t\n')
            actual=con.wait(lambda b:b'type=' in b,start);assert b'parameter + 1' in actual;observed('completion inside expression preserves suffix')
            # Literal paths, including spaces, use the actual directory at Tab time.
            chosen=output/'source name.cc';chosen.write_bytes(source.read_bytes())
            start=len(con.log);con.write(('tui source '+str(output)+'/source n').encode()+b'\t\n')
            con.wait(lambda b:b'source name.cc' in b,start);observed('real path completion with spaces')
            start=len(con.log);con.write(b'tui enable\n')
            screen=con.wait(lambda b:b'\x1b[?1049h' in b and b'[src *]' in b and b'=> ' in b,start)
            assert b'source name.cc' not in screen or b'error:' not in screen
            assert f'stop={current}'.encode() in screen;observed('source TUI actual highlighted source and stop')
            start=len(con.log);con.write(b'info wasip1 \t')
            con.wait(lambda b:b'info wasip1 args' in b and b'info wasip1 preopens' in b,start)
            con.write(b'\x15');observed('static Tab candidates remain visible inside TUI')
            start=len(con.log);con.write(b'print par\t')
            completed=con.wait(lambda b:b'(uwvm-debug) print parameter' in b,start)
            con.write(b'\x15');observed('dynamic variable Tab redraws active TUI input')
            start=len(con.log);con.write(b'layout split\n')
            screen=con.wait(lambda b:b'[source *]' in b and b'[asm *]' in b and (b'native-disassembly stop=' in b or b'error: native disassembly unavailable:' in b),start)
            observed('split preserves cooperative native-view rejection')
            start=len(con.log);con.write(b'focus cmd\n\x1b[5~\x1b[6~focus code\n')
            con.wait(lambda b:b'[commands *]' in b,start);observed('focus and page scrolling')
            fcntl.ioctl(con.fd,termios.TIOCSWINSZ,struct.pack('HHHH',28,100,0,0));start=len(con.log);con.write(b'\x0c')
            con.wait(lambda b:b'\x1b[28;' in b,start);observed('resize and CtrlL redraw')
            start=len(con.log);con.write(b'layout wasm\n')
            con.wait(lambda b:b'[wasm *]' in b and b'[locals]' in b,start);observed('Wasm values pane')
            start=len(con.log);con.write(b'layout wasip1\n')
            con.wait(lambda b:b'[wasip1 *]' in b,start);observed('WASIp1 pane')
            start=len(con.log);con.write(b'\x18a')
            con.wait(lambda b:b'\x1b[?1049l' in b and b'(uwvm-debug) ' in b,start);observed('CtrlX A preserves live console')
            def plain_reply(command):
                start=len(con.log);con.write(command.encode()+b'\n')
                return con.wait(lambda b:command.encode() in b and b'\n(uwvm-debug) ' in b and
                    (b'stop-id ' in b or b'error:' in b or b'unavailable' in b),start)
            state=plain_reply('status');assert mod.stop_id(state)==current
            if args.native_unavailable:
                rejected=plain_reply(f'step asm {thread}')
                assert b'error:' in rejected and b'native instruction 0x' not in rejected
                retained=plain_reply('status');assert mod.stop_id(retained)==current
                observed('disabled owner authorization refuses asm step without changing stop')
                start=len(con.log);con.write(b'layout asm\n')
                unavailable=con.wait(lambda b:b'[asm *]' in b and b'native disassembly unavailable:' in b,start)
                assert b'native-disassembly stop=' not in unavailable
                observed('asm TUI shows unavailable without exposing VM bytes')
                start=len(con.log);con.write(b'layout regs\n')
                unavailable=con.wait(lambda b:b'[registers *]' in b and b'native registers unavailable: requires an authenticated Wasm JIT trap' in b,start)
                assert b'native-registers stop=' not in unavailable
                observed('register TUI refuses an unauthenticated native frame')
                start=len(con.log);con.write(b'\x18a');con.wait(lambda b:b'\x1b[?1049l' in b and b'(uwvm-debug) ' in b,start)
                retained=plain_reply('status');assert mod.stop_id(retained)==current
                row['native_positive_coverage']=False
                row['native_panel_qualification']='unavailable: actual all-TU build argv disables native owner-table V2'
            else:
                # Some genuine safe points lack an admitted native instruction.
                # Recover exclusively through bounded real Wasm stepping, with TF clear.
                native_deadline=time.monotonic()+45
                for attempt in range(128):
                    assert time.monotonic()<native_deadline,'finite native admission search'
                    stepped=plain_reply(f'step asm {thread}')
                    if b'native instruction 0x' in stepped:
                        assert b'native-pc=0x' in stepped and mod.stop_id(stepped)!=current
                        current=mod.stop_id(stepped);break
                    assert b'error:' in stepped,stepped
                    retained=plain_reply('status');assert mod.stop_id(retained)==current
                    recovery=plain_reply(f'step wasm {thread}')
                    assert b'stopped: selected participant step' in recovery,recovery
                    assert mod.stop_id(recovery)!=current;current=mod.stop_id(recovery)
                else:
                    raise AssertionError('no genuine native trap within 128 actual Wasm steps')
                row['native_admission_attempts']=attempt+1
                reg=con.send('info registers',b'native-registers end')
                assert b'frame=0 module=0' in reg;observed('genuine native trap registers')
                bounded=con.send(f'disassemble-range {thread} {current} 16 0 0 1',b'native-disassembly-end')
                lo=int(re.search(rb'owner-begin=0x([0-9a-f]+)',bounded).group(1),16)
                hi=int(re.search(rb'owner-end=0x([0-9a-f]+)',bounded).group(1),16)
                pcs=[int(p,16) for p in re.findall(rb' instruction \d+ pc=0x([0-9a-f]+)',bounded)]
                assert pcs and all(lo<=pc<hi for pc in pcs);observed('every asm instruction remains inside the Wasm JIT owner')
                start=len(con.log);con.write(b'layout regs\n')
                con.wait(lambda b:b'[registers *]' in b and b'native-registers stop=' in b,start);observed('TUI authentic native register pane')
                # Source completion must not borrow the old cooperative frame after native stepping.
                start=len(con.log);con.write(b'\x18a');con.wait(lambda b:b'\x1b[?1049l' in b and b'(uwvm-debug) ' in b,start)
                start=len(con.log);con.write(b'print par\t\n')
                rejected=con.wait(lambda b:b'native trap has no current cooperative local snapshot' in b and
                    b'\n(uwvm-debug) ' in b,start)
                assert b'print parameter' not in rejected;observed('native stop rejects stale cooperative completion after submitted Tab')
                row['native_positive_coverage']=True
            start=len(con.log);con.write(b'tui enable\n');con.wait(lambda b: b'\x1b[?1049h' in b,start)
            con.write(b'\x04');row['exit']=con.exit(0)
            assert termios.tcgetattr(con.fd)==con.baseline
            assert bytes(con.log).count(b'\x1b[?1049h')==bytes(con.log).count(b'\x1b[?1049l')
            assert b'\x1b[?2004l' in con.log;observed('CtrlD restores alternate screen, paste and terminal modes')
            row['passed']=True
        except BaseException as error:
            row['error']=repr(error)
        finally:
            (output/(policy+'.pty')).write_bytes(con.log)
            row['transcript_sha256']=sha(output/(policy+'.pty'))
            if not con.reaped:
                try:signal.pidfd_send_signal(fd,signal.SIGKILL)
                except ProcessLookupError:pass
                os.waitpid(con.pid,0);con.reaped=True
            os.close(fd);os.close(con.fd);rows.append(row);save()
        if not row['passed']:break
    print(json.dumps({'passed':all(r['passed'] for r in rows),'cases':len(rows),'observations':sum(len(r['observations']) for r in rows)}),flush=True)
    raise SystemExit(0 if all(r['passed'] for r in rows) else 1)

if __name__=='__main__':main()
