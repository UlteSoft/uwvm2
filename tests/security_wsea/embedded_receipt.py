#!/usr/bin/env python3
"""Actual production receipt invalidation across policy/state/time boundaries."""
import argparse
import datetime as dt
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import time


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ['fixtures','runner','verifier','go-cli','output']:ap.add_argument('--'+name,type=Path,required=True)
    a=ap.parse_args();os.umask(0o077);a.output.mkdir(mode=0o700,parents=True,exist_ok=False)
    spec=importlib.util.spec_from_file_location('matrix',Path(__file__).with_name('algorithm_matrix.py'))
    matrix=importlib.util.module_from_spec(spec);spec.loader.exec_module(matrix);matrix.cgroup_snapshot(True)
    fixtures=json.loads((a.fixtures/'fixtures.json').read_text());good=fixtures['guest_algorithms']['ES256'];rows=[]
    def context(name):
        directory=a.output/name;directory.mkdir();state=directory/'state';state.mkdir(mode=0o700)
        (state/'state').write_text('UWVM-WSEA-STATE-1\n0 0 -\n')
        policy=directory/'policy';policy.write_text(good['policy'].replace('@STATE_DIR@',str(state)));policy.chmod(0o600)
        return directory,policy,state
    def check(name,passed,**detail):rows.append(dict(name=name,passed=passed,**detail))
    def start(policy,module):
        p=subprocess.Popen([str(a.runner),str(policy),str(module)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        ready=p.stdout.readline().strip();check('immutable receipt content/metadata/pointer integrity',ready=='READY receipt-integrity-pass',output=ready)
        if not ready.startswith('READY'):raise RuntimeError('receipt fixture did not authorize')
        return p
    def match(p):p.stdin.write('match\n');p.stdin.flush();return p.stdout.readline().strip()
    def stop(p):
        p.stdin.write('quit\n');p.stdin.flush();p.communicate(timeout=10);check('receipt process normal closure',p.returncode==0)
    def ca_copy(directory):
        old=Path(good['pki']['directory']);new=directory/'ca'
        shutil.copytree(old,new)
        config=new/'ca.cnf';config.write_text(config.read_text().replace(str(old),str(new)))
        return new
    def openssl(*argv):
        p=subprocess.run(['openssl',*map(str,argv)],capture_output=True,text=True,timeout=20)
        check('authenticated PKI producer',p.returncode==0,argv=['openssl',*map(str,argv)],stderr=p.stderr)
        if p.returncode:raise RuntimeError('PKI producer failed')
    def sign(path,policy,version,after,certificate=None,crl=None):
        ca=good['pki'];stamp=lambda t:dt.datetime.fromtimestamp(t,dt.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
        core=matrix.embedded_parts(Path(good['module']).read_bytes())[0];path.write_bytes(core)
        argv=[str(a.go_cli),'sign',str(path),'--policy',str(policy),'--key',ca['key'],'--certificate',str(certificate or ca['certificate']),'--crl',str(crl or ca['crl']),
              '--module-id','matrix.app','--module-version',str(version),'--capabilities','3','--not-before',stamp(time.time()-60),'--not-after',stamp(after)]
        p=subprocess.run(argv,capture_output=True,text=True,timeout=30);check('real signature producer',p.returncode==0,argv=argv,stderr=p.stderr)
        if p.returncode:raise RuntimeError('signing failed')
    try:
        directory,policy,state=context('policy');saved=policy.read_text();p=start(policy,good['module'])
        try:
            check('initial receipt matches',match(p)=='MATCH')
            policy.write_text(saved.replace('policy_version=1','policy_version=2'))
            check('changed policy invalidates receipt',match(p)=='DENY')
            cold=subprocess.run([str(a.verifier),str(policy),good['module']],capture_output=True,text=True,timeout=20)
            check('new policy independently authorized',cold.returncode==0 and cold.stdout.strip()=='ALLOW')
            policy.write_text(saved)
            check('persistent policy floor prevents restoring old receipt',match(p)=='DENY')
        finally:stop(p)
        directory,policy,state=context('version');p=start(policy,good['module'])
        try:
            newer=directory/'newer.wasm';sign(newer,policy,2,time.time()+3600)
            cold=subprocess.run([str(a.verifier),str(policy),str(newer)],capture_output=True,text=True,timeout=20)
            check('new release advances protected version',cold.returncode==0 and cold.stdout.strip()=='ALLOW')
            check('old receipt observes new module floor',match(p)=='DENY')
        finally:stop(p)
        directory,policy,state=context('revocation');p=start(policy,good['module'])
        try:
            ca=ca_copy(directory)
            openssl('ca','-batch','-config',ca/'ca.cnf','-revoke',good['pki']['certificate'])
            openssl('ca','-batch','-config',ca/'ca.cnf','-gencrl','-out',ca/'revoked.pem')
            core,cose,certs,_=matrix.embedded_parts(Path(good['module']).read_bytes())
            revoked=directory/'revoked.wasm'
            revoked.write_bytes(matrix.pack_embedded(core,cose,certs,(ca/'revoked.pem').read_bytes()))
            cold=subprocess.run([str(a.verifier),str(policy),str(revoked)],capture_output=True,text=True,timeout=20)
            check('authenticated revocation learns durable marker',cold.returncode==2 and 'CERT_004' in cold.stdout)
            check('protected revocation memory invalidates cached PKI receipt',match(p)=='DENY')
        finally:stop(p)
        directory,policy,state=context('expiry');short=directory/'short.wasm';expiry=int(time.time())+3;sign(short,policy,1,expiry);p=start(policy,short)
        try:
            check('receipt valid before signed expiry',match(p)=='MATCH')
            while time.time()<expiry+0.05:time.sleep(min(.1,max(.01,expiry+0.05-time.time())))
            check('cached PKI receipt cannot outlive manifest expiry',match(p)=='DENY')
        finally:stop(p)
        for kind in ['certificate','crl-next','crl-age']:
            directory,policy,state=context(kind);ca=ca_copy(directory);expiry=int(time.time())+4
            stamp=lambda value:dt.datetime.fromtimestamp(value,dt.timezone.utc).strftime('%Y%m%d%H%M%SZ')
            certificate=None;crl=None
            if kind=='certificate':
                certificate=ca/'expiring.pem'
                openssl('ca','-batch','-notext','-config',ca/'ca.cnf','-in',ca/'signer.csr','-out',certificate,
                        '-startdate',stamp(time.time()-60),'-enddate',stamp(expiry))
            else:
                crl=ca/'fresh.pem'
                if kind=='crl-age':
                    policy.write_text(policy.read_text().replace('max_crl_age_seconds=86400','max_crl_age_seconds=3'))
                openssl('ca','-batch','-config',ca/'ca.cnf','-gencrl','-out',crl,
                        '-crl_lastupdate',stamp(expiry-4),'-crl_nextupdate',stamp(expiry if kind=='crl-next' else time.time()+3600))
            signed=directory/'signed.wasm';sign(signed,policy,1,time.time()+3600,certificate,crl);p=start(policy,signed)
            try:
                check(kind+' receipt initially valid',match(p)=='MATCH')
                while time.time()<expiry+.05:time.sleep(min(.1,max(.01,expiry+.05-time.time())))
                check(kind+' bound expires cached PKI receipt',match(p)=='DENY')
            finally:stop(p)
    except BaseException as error:
        rows.append({'name':'receipt harness normal completion','passed':False,'error':repr(error)})
        raise
    finally:
        report={'status':'PASS' if rows and all(r['passed'] for r in rows) else 'FAIL','records':rows,
                'source_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'cgroup':matrix.cgroup_snapshot(True)}
        (a.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'status':report['status'],'checks':len(rows)}));return 0 if report['status']=='PASS' else 1

if __name__=='__main__':raise SystemExit(main())
