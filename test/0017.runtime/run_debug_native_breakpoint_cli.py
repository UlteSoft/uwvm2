#!/usr/bin/env python3
"""Real confined native breakpoint hits and rejected targets in the original cgroup."""
import argparse, hashlib, json, re
from pathlib import Path
from run_debug_run_cli import Console, require


def main():
    p=argparse.ArgumentParser()
    for key in ('uwvm','wasm','source','out'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--expected-cgroup',required=True);a=p.parse_args()
    require(Path('/proc/self/cgroup').read_text()=='0::'+a.expected_cgroup+'\n','original cgroup')
    cg=Path('/sys/fs/cgroup')/a.expected_cgroup.lstrip('/')
    require((cg/'memory.max').read_text().strip()==str(64<<30) and (cg/'memory.swap.max').read_text().strip()=='0','64GiB/swap0')
    a.out.mkdir(exist_ok=False);rows=[]
    c=Console([str(a.uwvm.resolve()),'-Rdbg','-Rct','0','-Rllvm-call-stack','instruction','-Rllvm-exception-dispatch','native-unwind','-Rllvm-cache-path','disable','--run',str(a.wasm.resolve())])
    def ask(command,marker=None):
        raw=c.command(command,marker.encode() if marker else None)
        out=re.sub(rb'\x1b\[[0-9;?]*[A-Za-z]',b'',raw).replace(b'\r',b'')
        rows.append({'command':command,'reply':out.decode(errors='replace')});return out
    def status():
        out=ask('status');return int(re.search(rb'stop-id ([0-9]+)',out)[1])
    def pc():
        out=ask('info registers $pc');require(b'error:' not in out,repr(out))
        return int(re.search(rb'\bpc[^\n]*?0x([0-9a-fA-F]+)',out)[1],16)
    try:
        c.wait(lambda data:b'(uwvm-debug) ' in data,0)
        for command in ('b 0 1 0','break 0 1 0','break-source 0 x.cc:1','breakpoint set 0 1 0','break-name 0 main'):
            require(b'Undefined command' in ask(command) or b'error:' in rows[-1]['reply'].encode(),command)
        ask('help bi','Set a JIT assembly breakpoint');ask('help breakasm','Set a JIT assembly breakpoint')
        require(b'error:' in ask('bi 0'),'no native activation before execution')
        line=next(i for i,t in enumerate(a.source.read_text().splitlines(),1) if 'UX_LEAF_READY' in t)
        point=ask(f'bs 0 {a.source.resolve()}:{line}','breakpoint ')
        source_id=re.search(rb'breakpoint ([0-9]+)',point)[1].decode()
        ask('r','stopped: breakpoint');ask('delete '+source_id)
        require(b'error:' in ask('bi 0'),'cooperative Wasm stop is not native capture')
        stepped=ask('si','stopped: native instruction step');require(b'error:' not in stepped,repr(stepped))
        current_pc=pc();before=status();disas=ask('disas','native-disassembly-end')
        instructions=re.findall(rb'instruction ([0-9]+) pc=0x([0-9a-fA-F]+) function-offset=([0-9]+) bytes=([0-9a-fA-F ]+)  ([^\n]+)',disas)
        require(len(instructions)>=3,repr(disas))
        require(int(instructions[0][1],16)==current_pc,'actual register PC equals owned disassembly')
        # Target one ordinary sequential instruction ahead, proven again by the controller.
        target=instructions[1];offset=int(target[2]);target_pc=int(target[1],16)
        for command in ('bi -1','bi +1','bi 0x1000','bi 65537','bi 18446744073709551615',f'bi {current_pc}',
                        'bi 0 1 0','bi main','breakasm vm_main','bi 65536'):
            require(b'error:' in ask(command),command);require(status()==before and pc()==current_pc,'rejection preserves exact actual trap')
        # The first native function bytes are the runtime JIT adapter/prologue.
        require(b'error:' in ask('bi 0'),'adapter bytes are not an asm breakpoint')
        multi=next(x for x in instructions if len(x[3].split())>1)
        require(b'error:' in ask('bi '+str(int(multi[2])+1)),'middle of native instruction rejected')
        require(status()==before and pc()==current_pc,'boundary rejection never opens guest gate')
        bp=ask('bi '+str(offset),' asm function-offset=');identifier=re.search(rb'breakpoint ([0-9]+)',bp)[1].decode()
        bp2=ask('breakasm '+str(offset),' asm function-offset=');require(re.search(rb'breakpoint ([0-9]+)',bp2)[1].decode()==identifier,'aliases deduplicate actual bound target')
        listing=ask('info breakpoints');require(b'kind=asm' in listing and b'hits=0' in listing,repr(listing))
        ask('disable '+identifier,'policy updated');require(b'enabled=no' in ask('info breakpoints'),'disabled')
        ask('enable '+identifier,'policy updated');ask('ignore '+identifier+' 0','policy updated')
        require(b'error:' in ask('condition '+identifier+' true'),'native source conditions rejected')
        hit=ask('c','stopped: native breakpoint');require(b'native-breakpoint '+identifier.encode() in hit and b'error:' not in hit,repr(hit))
        require(pc()==target_pc,'real kernel native PC equals target')
        require(status()>before,'fresh real stop identity');require(b'hits=1' in ask('info breakpoints'),'one actual native hit')
        require(b'error:' in ask(f'memory 0 0 {target_pc} 8'),'native addresses cannot select guest memory')
        require(b'error:' in ask('print counter'),'old Wasm source snapshot is not native state')
        ask('delete '+identifier,'breakpoint deleted');ask('info breakpoints','no breakpoints')
        ask('c','Program exited normally.');require(b'error:' in ask('bi '+str(offset)),'exited activation cannot set native breakpoint')
        ask('run','Program exited normally.');ask('info breakpoints','no breakpoints')
        c.finish()
    finally:
        (a.out/'transcript.txt').write_bytes(c.log);(a.out/'actions.json').write_text(json.dumps(rows,indent=2));c.close()
    result={'passed':True,'command_count':len(rows),'real_target_pc':target_pc,'function_offset':offset,'binary_sha256':hashlib.sha256(a.uwvm.read_bytes()).hexdigest(),'scope':'x86_64 real native kernel trap hit; exact activation and publication; forbidden VM/absolute/out-of-owner/mid-instruction targets; both spellings and policies; terminal restored'}
    (a.out/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result),flush=True)
if __name__=='__main__':main()
