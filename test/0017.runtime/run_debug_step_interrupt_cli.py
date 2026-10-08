#!/usr/bin/env python3
"""Actual Linux cgroup PTY and PIDFD SIGINT cancellation of Wasm over/out.
Requires a genuine qualified production main/runtime/host build record.
No model stop, callee register or native stack is requested.
"""
from pathlib import Path
import argparse,errno,hashlib,json,os,pty,re,select,signal,termios,time

def digest(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def check(v,why):
    if not v:raise AssertionError(why)

class Console:
    def __init__(self,argv):
        reader,writer=os.pipe();self.pid,self.fd=pty.fork()
        if self.pid==0:
            os.close(writer)
            if os.read(reader,1)!=b'R':os._exit(99)
            os.close(reader);os.execv(argv[0],argv)
        os.close(reader);self.pidfd=os.pidfd_open(self.pid)
        self.baseline=termios.tcgetattr(self.fd);self.log=bytearray();self.reaped=False
        self.deadline=time.monotonic()+30
        os.write(writer,b'R');os.close(writer)
    def write(self,s):os.write(self.fd,s)
    def read(self,timeout):
        if not select.select([self.fd],[],[],timeout)[0]:return
        try:data=os.read(self.fd,65536)
        except OSError as e:
            if e.errno==errno.EIO:raise AssertionError('CLI exited before expected reply') from None
            raise
        check(data,'CLI output EOF');self.log.extend(data);check(len(self.log)<=1<<20,'bounded transcript')
    def wait(self,start,predicate):
        while time.monotonic()<self.deadline:
            suffix=bytes(self.log[start:])
            if predicate(suffix):return suffix
            self.read(.02)
        raise AssertionError('actual CLI deadline: '+repr(bytes(self.log[start:])[-3000:]))
    def prompt(self,start):return self.wait(start,lambda s:b'(uwvm-debug) ' in s)
    def response(self,start,markers):
        def complete(s):
            for marker in (*markers,b'error:'):
                at=s.find(marker)
                if at>=0 and b'(uwvm-debug) ' in s[at+len(marker):]:return True
            return False
        return self.wait(start,complete)
    def send(self,s):
        start=len(self.log);self.write(s.encode()+b'\n')
        markers=(b'breakpoint ',) if s.startswith('break ') else (b'running',b'stopped: breakpoint') if s=='continue' else (b'stop-id ',)
        reply=self.response(start,markers)
        check(b'error:' not in reply,'real command refused: '+repr(reply[-1800:]));return reply
    def interrupt_step(self,command,external):
        start=len(self.log);self.write(command.encode()+b'\n')
        self.wait(start,lambda s:command.encode() in s)
        until=time.monotonic()+.12
        while time.monotonic()<until:self.read(.01)
        check(not any(x in self.log[start:] for x in (b'stop-id ',b'stopped:',b'error:')),'step must genuinely wait inside infinite callee')
        sent=time.monotonic()
        if external:signal.pidfd_send_signal(self.pidfd,signal.SIGINT)
        else:self.write(b'\x03')
        reply=self.response(start,(b'stopped: pause',));elapsed=time.monotonic()-sent
        check(b'stopped: pause' in reply and b'thread ' in reply and b'error:' not in reply and
              b'timed out' not in reply,'interrupt must complete at an authentic Wasm pause: '+repr(reply[-2000:]))
        check(elapsed<1.5,'interrupt delayed by observation timeout')
        return reply,elapsed
    def finish(self):
        self.write(b'quit\n')
        while time.monotonic()<self.deadline:
            pid,status=os.waitpid(self.pid,os.WNOHANG)
            if pid:
                self.reaped=True;check(os.waitstatus_to_exitcode(status)==0,'clean actual product exit')
                check(termios.tcgetattr(self.fd)==self.baseline,'actual terminal modes restored');return
            if select.select([self.fd],[],[],.02)[0]:
                try:self.log.extend(os.read(self.fd,65536))
                except OSError as e:
                    if e.errno!=errno.EIO:raise
        raise AssertionError('actual product exit deadline')
    def close(self):
        if not self.reaped:
            try:signal.pidfd_send_signal(self.pidfd,signal.SIGKILL)
            except ProcessLookupError:pass
            os.waitpid(self.pid,0)
        os.close(self.pidfd);os.close(self.fd)

def stop_id(s):
    m=re.findall(rb'stop-id ([0-9]+)',s);check(m,'authentic stop ID required');v=int(m[-1]);check(v>0,'nonzero stop ID');return v

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('binary','build-record','wasm','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--binary-sha256',required=True);p.add_argument('--cgroup',required=True)
    a=p.parse_args();actual=Path('/proc/self/cgroup').read_text().strip()
    check(actual=='0::'+a.cgroup,'original cgroup required')
    cg=Path('/sys/fs/cgroup')/a.cgroup.lstrip('/')
    check((cg/'memory.max').read_text().strip()==str(64<<30) and (cg/'memory.swap.max').read_text().strip()=='0','64GiB/swap0 required')
    binary=a.binary.resolve(strict=True);record=json.loads(a.build_record.read_text())
    check(record.get('schema') in ('matched-canceled-step-full-product-closure-v1',
          'matched-debug-native-cfi-full-product-closure-v1') and record.get('phase')=='after',
          'actual matched production closure record required')
    check(record['binaries'][str(binary)]==a.binary_sha256==digest(binary),'actual binary and qualified build identity')
    pins={str(binary):digest(binary),str(a.build_record):digest(a.build_record),str(a.wasm):digest(a.wasm),str(Path(__file__)):digest(__file__)}
    a.out.mkdir(parents=True,exist_ok=False);rows=[]
    for policy in ('instruction','unwind'):
        argv=[str(binary),'-Rdbg','-Rct','0','-Rllvm-call-stack',policy,'-Rllvm-cache-path','disable','--run',str(a.wasm)]
        con=Console(argv);row={'policy':policy,'argv':argv,'passed':False}
        try:
            con.prompt(0);con.send('break 0 1 0');con.send('continue');initial=con.send('wait')
            check(b'stopped: breakpoint' in initial,'actual caller call-site breakpoint')
            m=re.search(rb'thread ([0-9]+) module=0 function=1 ',initial);check(m,'actual caller participant')
            participant=int(m[1]);before=stop_id(initial)
            for kind,external in (('over',False),('out',True)):
                paused,elapsed=con.interrupt_step('step wasm '+str(participant)+' '+kind,external)
                current=stop_id(paused);check(current>before,'canceled step has fresh genuine stop')
                check(b'function=0 ' in paused and b'native-pc=' not in paused,'callee cooperative Wasm context only')
                status=con.send('status');check(stop_id(status)==current,'canceled request retires and session accepts next request')
                stepped=con.send('step wasm '+str(participant)+' into')
                before=stop_id(stepped);check(before>current and b'stopped: selected participant step' in stepped,
                                            'next real Wasm step works after cancellation')
                row[kind]={'source':'PIDFD SIGINT' if external else 'PTY Ctrl+C','interrupt_seconds':elapsed,'canceled_stop':current,'next_step_stop':before}
            con.finish();row['passed']=True
        finally:
            (a.out/(policy+'.log')).write_bytes(con.log);con.close()
        rows.append(row);(a.out/'results.json').write_text(json.dumps({'rows':rows,'pins':pins},indent=2)+'\n')
        print('PASS actual Wasm over/out PTY Ctrl+C and PIDFD SIGINT',policy,flush=True)
    check(all(digest(p)==h for p,h in pins.items()),'actual build/binary/source/Wasm inputs unchanged')
if __name__=='__main__':main()
