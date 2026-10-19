#!/usr/bin/env python3
"""Build the real ordinary WSEA CLI and production verifier from a frozen cohort.

Invoke only through the authorized SSH cgroup bounded launcher. A JSON plan
provides exact source, target, cxx argv (including real SDK/ABI flags), explicit
OpenSSL root and receipt, link_flags, and output. No toolchain is downloaded or
installed. No target executable is run by this build helper.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import subprocess
import sys
import time


def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()


def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--plan',type=Path,required=True)
 ap.add_argument('--disable-wsea',action='store_true',help='Build the real ordinary VM without OpenSSL; secure requests must fail closed')
 ap.add_argument('--thread-local',action='store_true',help='Enable the production UWVM_USE_THREAD_LOCAL runtime configuration')
 a=ap.parse_args()
 if os.uname().sysname!='Linux':raise RuntimeError('build only in authorized SSH Linux cgroup')
 resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 plan=json.loads(a.plan.read_text());source=Path(plan['source']).resolve(strict=True);out=Path(plan['output']).absolute()
 crypto=None if a.disable_wsea else Path(plan['openssl_root']).resolve(strict=True)
 if Path('/proc/self/cgroup').read_text().strip()!='0::/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope':raise RuntimeError('original cgroup is required')
 if not set(os.sched_getaffinity(0))<={16,17}:raise RuntimeError('approved two-core affinity is required')
 out.mkdir(mode=0o700,parents=True,exist_ok=False)
 cxx=plan['cxx'];link_flags=plan.get('link_flags',[])
 if not isinstance(cxx,list) or not cxx:raise ValueError('plan cxx must be an explicit argv')
 openssl_receipt=None if a.disable_wsea else Path(plan['openssl_receipt']).resolve(strict=True)
 if openssl_receipt is not None:
  provider=json.loads(openssl_receipt.read_text())
  if not provider.get('actual_archive_abi'):raise ValueError('OpenSSL must have actual archive ELF audit evidence')
 security_paths=['src/uwvm2/utils/security/wsea.h','src/uwvm2/uwvm/runtime/initializer/init.h','src/uwvm2/uwvm/runtime/initializer/init.cppm','src/uwvm2/uwvm/wasm/loader/wasm_file.h','src/uwvm2/uwvm/wasm/type/file.h','src/uwvm2/uwvm/cmdline/parser.h']
 security_sources={name:sha(source/name) for name in security_paths}
 record={'status':'FAIL','plan':plan,'plan_sha256':sha(a.plan),'source_header_sha256':sha(source/'src/uwvm2/utils/security/wsea.h'),
         'openssl_receipt_sha256':sha(openssl_receipt) if openssl_receipt is not None else None,
         'wsea_enabled':not a.disable_wsea,'compiler_sha256':sha(cxx[0]),'helper_sha256':sha(__file__),
         'thread_local_enabled':a.thread_local,
         'security_source_sha256':security_sources,
         'scope':('real ordinary interpreter CLI without OpenSSL/WSEA' if a.disable_wsea else 'real ordinary interpreter CLI + actual production verifier')+'; WASI enabled, LLVM/JIT disabled; thread-local runtime '+('enabled' if a.thread_local else 'disabled'),
         'start_utc_seconds':time.time(),'stages':[]}
 def save(): (out/'build-report.json').write_text(json.dumps(record,indent=2)+'\n')
 def run(name,argv):
  start=time.time()
  with (out/(name+'.log')).open('wb') as log:p=subprocess.run(argv,stdout=log,stderr=subprocess.STDOUT,timeout=900)
  row={'name':name,'argv':argv,'start_utc_seconds':start,'end_utc_seconds':time.time(),'returncode':p.returncode,'log_sha256':sha(out/(name+'.log'))}
  record['stages'].append(row);save()
  if p.returncode:raise RuntimeError('build stage failed: '+name)
 includes=[]
 for entry in ['src','third-parties/fast_io/include','third-parties/bizwen/include','third-parties/boost_unordered/include']:includes+=['-I'+str(source/entry)]
 if crypto is not None:includes+=['-I'+str(crypto/'include')]
 definitions=['-DUWVM=2','-DUWVM_USE_UWVM_INT','-DUWVM_DISABLE_JIT','-DUWVM_IMPORT_WASI',
              '-DUWVM_VERSION_X=2','-DUWVM_VERSION_Y=0','-DUWVM_VERSION_Z=3','-DUWVM_VERSION_S=0']
 if not a.disable_wsea:definitions+=['-DUWVM_WSEA_OPENSSL=1']
 if a.thread_local:definitions+=['-DUWVM_USE_THREAD_LOCAL=1']
 common=['-std=c++26','-O1','-g0','-ferror-limit=1','-Wno-undefined-inline','-Wno-braced-scalar-init',*includes,*definitions]
 objects=[]
 units=[('runtime','src/uwvm2/runtime/lib/uwvm_runtime.default.cpp'),('main','src/uwvm2/uwvm/main.default.cpp'),('host','src/uwvm2/uwvm/host_api.default.cpp')]
 try:
  for name,relative in units:
   obj=out/(name+'.o');dep=out/(name+'.d')
   run(name,[*cxx,*common,'-MD','-MF',str(dep),'-c',str(source/relative),'-o',str(obj)])
   record['stages'][-1]['object_sha256']=sha(obj)
   dependencies=dep.read_text().replace('\\\n',' ').split(':',1)[1].split()
   record['stages'][-1]['dependency_sha256']={str(Path(p).resolve(strict=True)):sha(p) for p in dependencies}
   save();objects.append(str(obj))
  libraries=[] if crypto is None else [str(crypto/'libssl.a'),str(crypto/'libcrypto.a')]
  run('link',[*cxx,*objects,*libraries,*link_flags,'-pthread','-ldl','-lm','-o',str(out/'uwvm')])
  if not a.disable_wsea:
   verifier_dep=out/'verifier.d'
   run('verifier',[*cxx,'-std=c++20','-O0','-g0','-MD','-MF',str(verifier_dep),'-DUWVM_WSEA_OPENSSL=1','-I'+str(source/'src'),'-I'+str(crypto/'include'),
                    str(source/'tests/security_wsea/algorithm_matrix_runner.cpp'),*libraries,*link_flags,'-pthread','-ldl','-lm','-o',str(out/'algorithm_matrix_runner')])
   dependencies=verifier_dep.read_text().replace('\\\n',' ').split(':',1)[1].split()
   record['stages'][-1]['dependency_sha256']={str(Path(p).resolve(strict=True)):sha(p) for p in dependencies}
  record['binary_sha256']={name:sha(out/name) for name in (('uwvm',) if a.disable_wsea else ('uwvm','algorithm_matrix_runner'))}
  if sha(source/'src/uwvm2/utils/security/wsea.h')!=record['source_header_sha256']:raise RuntimeError('source changed during build')
  if any(sha(source/name)!=digest for name,digest in security_sources.items()):raise RuntimeError('security gate source changed during build')
  record['status']='PASS'
 except Exception as e:record['error']=str(e);raise
 finally:record['end_utc_seconds']=time.time();save()
 print(json.dumps({'status':record['status'],'output':str(out),'binary_sha256':record.get('binary_sha256')}))

if __name__=='__main__':main()
