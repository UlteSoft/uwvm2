#!/usr/bin/env python3
"""Real HTTPS simulation, independent deployment/VM gates and startup timing.

Run through run_bounded_v2.py in the designated Linux cgroup. No system trust,
clock, network namespace or global configuration is changed.
"""
from __future__ import annotations
import argparse
import datetime as dt
import hashlib
import http.server
import importlib.util
import json
import os
from pathlib import Path
import ssl
import statistics
import subprocess
import threading
import time


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--fixtures',type=Path,required=True);ap.add_argument('--go-cli',type=Path,required=True)
    ap.add_argument('--vm',type=Path,required=True);ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--exec-prefix-json',default='[]');ap.add_argument('--go-prefix-json',default='[]')
    ap.add_argument('--go-subcommands-json',default='[]')
    ap.add_argument('--benchmark',action='store_true');a=ap.parse_args()
    if os.uname().sysname!='Linux':raise RuntimeError('authorized Linux execution only')
    root=Path(__file__).resolve().parents[2]
    module=importlib.util.spec_from_file_location('wsea_embedded_matrix',Path(__file__).with_name('algorithm_matrix.py'))
    matrix=importlib.util.module_from_spec(module);module.loader.exec_module(matrix)
    matrix.cgroup_snapshot(True)
    os.umask(0o077);a.output.mkdir(mode=0o700,parents=True,exist_ok=False)
    out=a.output.resolve();manifest=json.loads((a.fixtures/'fixtures.json').read_text())
    good=manifest['guest_algorithms']['ES256'];original=Path(good['module']).read_bytes()
    core,cose,certificates,crls=matrix.embedded_parts(original);ca=good['pki']
    state=out/'state';state.mkdir(mode=0o700);(state/'state').write_text('UWVM-WSEA-STATE-1\n0 0 -\n')
    policy=out/'policy';policy.write_text(good['policy'].replace('@STATE_DIR@',str(state)).replace('max_module_bytes=1048576','max_module_bytes=67108864'));policy.chmod(0o600)
    go=[*json.loads(a.go_prefix_json),str(a.go_cli.resolve()),*json.loads(a.go_subcommands_json)];vm=[*json.loads(a.exec_prefix_json),str(a.vm.resolve())]
    report={'scope':'real HTTPS download + embedded Go gate + independently verifying actual VM',
            'source_sha256':matrix.sha(__file__),'fixtures_sha256':matrix.sha(a.fixtures/'fixtures.json'),
            'go_binary_sha256':matrix.sha(a.go_cli),'vm_binary_sha256':matrix.sha(a.vm),
            'records':[],'requests':[],'cgroup_before':matrix.cgroup_snapshot(True)}
    def record(name,argv,*,expected=0,marker=None,codes=(),contains=(),env=None):
        start=time.perf_counter_ns();p=subprocess.run(list(map(str,argv)),capture_output=True,text=True,timeout=45,env=env)
        exits=[expected] if isinstance(expected,int) else list(expected)
        text=p.stdout+p.stderr;passed=p.returncode in exits
        if marker:passed=passed and (marker in p.stdout if 0 in exits else matrix.GUEST_PREFIX not in text)
        if codes:passed=passed and any(code in text for code in codes)
        if contains:passed=passed and all(token in text for token in contains)
        row={'name':name,'argv':list(map(str,argv)),'expected_exit_codes':exits,'returncode':p.returncode,
             'stdout':p.stdout,'stderr':p.stderr,'elapsed_ns':time.perf_counter_ns()-start,'passed':passed}
        report['records'].append(row);print('PASS' if passed else 'FAIL',name,flush=True);return p
    def openssl(*args):
        p=subprocess.run(['openssl',*map(str,args)],capture_output=True,check=True)
        report.setdefault('producer_commands',[]).append({'argv':['openssl',*map(str,args)],'returncode':p.returncode})
        return p.stdout
    # Private transport CA is deliberately distinct from the WSEA release CA.
    tls=out/'tls';tls.mkdir();tlskey=tls/'ca.key';serverkey=tls/'server.key'
    for key in [tlskey,serverkey]:openssl('genpkey','-algorithm','EC','-pkeyopt','ec_paramgen_curve:prime256v1','-out',key)
    openssl('req','-new','-x509','-sha256','-key',tlskey,'-out',tls/'ca.pem','-days','1','-subj','/CN=WSEA private transport test CA',
            '-addext','basicConstraints=critical,CA:TRUE','-addext','keyUsage=critical,keyCertSign')
    openssl('req','-new','-key',serverkey,'-out',tls/'server.csr','-subj','/CN=localhost')
    (tls/'server.ext').write_text('basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=serverAuth\nsubjectAltName=IP:127.0.0.1,DNS:localhost\n')
    openssl('x509','-req','-in',tls/'server.csr','-CA',tls/'ca.pem','-CAkey',tlskey,'-CAcreateserial','-sha256','-days','1',
            '-extfile',tls/'server.ext','-out',tls/'server.pem')
    empty=tls/'empty';empty.mkdir();(tls/'untrusted.pem').write_bytes(b'')
    client_env=dict(os.environ,SSL_CERT_FILE=str(tls/'ca.pem'),SSL_CERT_DIR=str(empty))
    forged=matrix.pack_embedded(core,cose[:-1]+bytes([cose[-1]^1]),certificates,crls)
    changed=bytearray(original);changed[len(core)-1]^=1
    now=int(time.time());payload=matrix.cbor(['uwvm-wsea-v2','matrix.app',1,0,len(core),hashlib.sha256(core).digest(),now-120,now-1,'uwvm2',3])
    protected=matrix.cbor({1:-7});structure=matrix.cbor(['Signature1',protected,b'',payload]);(out/'expired.input').write_bytes(structure)
    openssl('dgst','-sha256','-sign',ca['key'],'-out',out/'expired.sig',out/'expired.input')
    expired_cose=b'\xd2'+matrix.cbor([protected,{},payload,matrix.ec_raw((out/'expired.sig').read_bytes(),32)])
    objects={'/good.wasm':original,'/forged.wasm':forged,'/unsigned.wasm':core,'/changed.wasm':bytes(changed),
             '/expired.wasm':matrix.pack_embedded(core,expired_cose,certificates,crls),'/truncated.wasm':original[:-1],
             '/tail.wasm':original+b'\0'}
    plain_count=[]
    class Plain(http.server.BaseHTTPRequestHandler):
        def do_GET(self):plain_count.append(self.path);self.send_response(200);self.end_headers()
        def log_message(self,*args):pass
    plain=http.server.ThreadingHTTPServer(('127.0.0.1',0),Plain)
    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            report['requests'].append(self.path)
            if self.path=='/downgrade':
                self.send_response(302);self.send_header('Location','http://127.0.0.1:'+str(plain.server_port)+'/leak');self.end_headers();return
            if self.path not in objects:self.send_response(404);self.end_headers();return
            data=objects[self.path];self.send_response(200);self.send_header('Content-Length',str(len(data)));self.end_headers()
            try:self.wfile.write(data)
            except (BrokenPipeError,ConnectionResetError):pass
        def log_message(self,*args):pass
    server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
    context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);context.minimum_version=ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(tls/'server.pem',serverkey);server.socket=context.wrap_socket(server.socket,server_side=True)
    threads=[threading.Thread(target=s.serve_forever,daemon=True) for s in [server,plain]]
    for thread in threads:thread.start()
    base='https://127.0.0.1:'+str(server.server_port)
    try:
        destination=out/'downloaded.wasm'
        record('Go fetch authentic embedded module',[*go,'fetch',destination,'--policy',policy,'--url',base+'/good.wasm'],env=client_env)
        record('VM independently checks downloaded module',[*vm,'--secure','module',destination,policy,'--run',destination],marker=good['guest_marker'])
        for path,codes in [('/forged.wasm',['SIG_003']),('/unsigned.wasm',['SIG_001']),('/changed.wasm',['SIG_004']),
                           ('/expired.wasm',['POL_004']),('/truncated.wasm',['SIG_002']),('/tail.wasm',['SIG_002'])]:
            target=out/('refused-'+path[1:])
            record('Go download refuses '+path,[*go,'fetch',target,'--policy',policy,'--url',base+path],expected=1,codes=codes,env=client_env)
            report['records'].append({'name':'rejected download never published '+path,'passed':not target.exists()})
            # Model a caller bypassing the upper gate: VM must still deny.
            target.write_bytes(objects[path])
            record('VM refuses bypassed upper gate '+path,[*vm,'--secure','module',target,policy,'--run',target],expected=[126,127],codes=codes,marker=good['guest_marker'])
        record('HTTPS downgrade refused',[*go,'fetch',out/'downgrade.wasm','--policy',policy,'--url',base+'/downgrade'],expected=1,env=client_env)
        report['records'].append({'name':'downgrade sends no plaintext request','passed':not plain_count})
        untrusted_env=dict(client_env,SSL_CERT_FILE=str(tls/'untrusted.pem'))
        record('untrusted transport certificate refused',[*go,'fetch',out/'untrusted.wasm','--policy',policy,'--url',base+'/good.wasm'],expected=1,env=untrusted_env)
        before=destination.read_bytes()
        record('verified fetch does not overwrite an existing file',[*go,'fetch',destination,'--policy',policy,'--url',base+'/good.wasm'],expected=1,env=client_env)
        report['records'].append({'name':'existing file preserved','passed':destination.read_bytes()==before})
        destination.write_bytes(bytes(changed))
        record('VM rejects tampering after a successful Go deployment check',[*vm,'--secure','module',destination,policy,'--run',destination],expected=[126,127],codes=['SIG_004'],marker=good['guest_marker'])
        unsigned=out/'ordinary-unsigned.wasm';unsigned.write_bytes(core)
        record('ordinary unsigned execution remains opt-in',[*vm,'--run',unsigned],marker=good['guest_marker'])
        record('secure command help', [*vm, '--secure', 'help'], contains=['named-module'])
        record('secure help category', [*vm, '--help', 'secure'], contains=['--secure module'])
        record('secure status without execution', [*vm, '--secure', 'status'])
        record('general help exposes secure category', [*vm, '--help'], contains=['<secure>'])
        signed=out/'selected-main.wasm';signed.write_bytes(original)
        dependency=out/'ordinary-library.wasm';dependency.write_bytes(b'\0asm\x01\0\0\0')
        record('unselected dependency remains ordinary', [*vm, '--wasm-preload-library', dependency, 'dep',
            '--secure', 'module', signed, policy, '--run', signed], marker=good['guest_marker'])
        for before in [False, True]:
            selection=['--secure','module',dependency,policy]
            preload=['--wasm-preload-library',dependency,'dep']
            record('selected unsigned preload refused independent of option order '+str(before),
                [*vm, *(preload+selection if before else selection+preload), '--run', signed],
                expected=[126,127], codes=['SIG_001'], marker=good['guest_marker'])
        record('duplicate secure selection refused', [*vm, '--secure', 'module', signed, policy,
            '--secure', 'module', signed, policy, '--run', signed], expected=[126,127], marker=good['guest_marker'])
        shared=out/'shared-state';shared.mkdir(mode=0o700);(shared/'state').write_text('UWVM-WSEA-STATE-1\n0 0 -\n')
        shared_policy=out/'shared.policy';text=good['policy'].replace('@STATE_DIR@',str(shared))
        rule=next(line for line in text.splitlines() if line.startswith('module='))
        text+=rule.replace('module=matrix.app|','module=matrix.other|')+'\n'
        shared_policy.write_text(text);shared_policy.chmod(0o600)
        record('multi-rule policy requires explicit identity', [*vm, '--secure', 'module', signed, shared_policy,
            '--run', signed], expected=[126,127], marker=good['guest_marker'])
        record('named module binds correct identity', [*vm, '--secure', 'named-module', signed, shared_policy,
            'matrix.app', '--run', signed], marker=good['guest_marker'])
        record('named module refuses another authorized identity', [*vm, '--secure', 'named-module', signed,
            shared_policy, 'matrix.other', '--run', signed], expected=[126,127], codes=['CERT_006'], marker=good['guest_marker'])
        stamp=lambda seconds:dt.datetime.fromtimestamp(seconds,dt.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
        for name,body in [('signed-library.wasm',b'\0asm\x01\0\0\0'),('other-identity-guest.wasm',core)]:
            target=out/name;target.write_bytes(body)
            record('Go signs separately named module '+name,[*go,'sign',target,'--policy',shared_policy,
                '--key',ca['key'],'--certificate',ca['certificate'],'--crl',ca['crl'],
                '--module-id','matrix.other','--module-version','1','--capabilities','3',
                '--not-before',stamp(now-60),'--not-after',stamp(now+3600)])
        record('main and dependency independently selected with shared policy', [*vm,
            '--wasm-preload-library',out/'signed-library.wasm','dep',
            '--secure','named-module',signed,shared_policy,'matrix.app',
            '--secure','named-module',out/'signed-library.wasm',shared_policy,'matrix.other',
            '--run',signed],marker=good['guest_marker'])
        record('valid B guest cannot replace path bound to A', [*vm,'--secure','named-module',
            out/'other-identity-guest.wasm',shared_policy,'matrix.app','--run',out/'other-identity-guest.wasm'],
            expected=[126,127],codes=['CERT_006'],marker=good['guest_marker'])
        if a.benchmark:
            timings=[]
            for size in [0,65536,1048576,16777216]:
                body=core
                if size:
                    data=b'\x01p'+bytes(size);body+=b'\0'+matrix.uleb(len(data))+data
                target=out/('benchmark-'+str(size)+'.wasm');target.write_bytes(body)
                stamp=lambda seconds:dt.datetime.fromtimestamp(seconds,dt.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
                record('sign benchmark '+str(size),[*go,'sign',target,'--policy',policy,'--key',ca['key'],'--certificate',ca['certificate'],
                       '--crl',ca['crl'],'--module-id','matrix.app','--module-version','1','--capabilities','3',
                       '--not-before',stamp(now-60),'--not-after',stamp(now+3600)])
                modes={}
                for secure in [False,True]:
                    values=[]
                    argv=[*vm,*(['--secure','module',str(target),str(policy)] if secure else []),'--run',str(target)]
                    for iteration in range(17):
                        p=record('startup '+str(size)+' secure='+str(secure)+' iteration='+str(iteration),argv,marker=good['guest_marker'])
                        if iteration>=2:values.append(report['records'][-1]['elapsed_ns']/1e6)
                    modes['secure' if secure else 'ordinary']={'p50_ms':statistics.median(values),'p95_ms':sorted(values)[int(.95*(len(values)-1))],'samples_ms':values}
                timings.append({'payload_bytes':size,'modes':modes,'p50_added_ms':modes['secure']['p50_ms']-modes['ordinary']['p50_ms']})
            report['startup_benchmarks']=timings
    except BaseException as error:
        report['error']=repr(error)
        raise
    finally:
        for service in [server,plain]:service.shutdown();service.server_close()
        for thread in threads:thread.join(timeout=3)
        report['server_threads_closed']=all(not thread.is_alive() for thread in threads)
        report['cgroup_after']=matrix.cgroup_snapshot(True)
        report['status']='PASS' if not report.get('error') and report['server_threads_closed'] and all(row['passed'] for row in report['records']) else 'FAIL'
        (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'status':report['status'],'checks':len(report['records']),'report':str(out/'report.json')}))
    return 0 if report['status']=='PASS' else 1

if __name__=='__main__':raise SystemExit(main())
