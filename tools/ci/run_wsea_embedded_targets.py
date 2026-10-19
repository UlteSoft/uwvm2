#!/usr/bin/env python3
"""Build and execute each real WSEA product in the existing SSH Linux cgroup.

No global installs, mounts or cgroup changes. Providers must already be privately
prepared from authenticated archives. Every build/test is a bounded owned stage.
"""
from __future__ import annotations
import argparse
import fcntl
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import time


def sha(p):
    h=hashlib.sha256()
    with Path(p).open('rb') as f:
        for b in iter(lambda:f.read(1048576),b''):h.update(b)
    return h.hexdigest()


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--workspace',type=Path,required=True)
    ap.add_argument('--targets',default='x86_64,i686,aarch64,arm,armhf,armeb,aarch64_be,ppc64,mips64,loongarch64')
    ap.add_argument('--repositories',default='uwvm2-ros,uwvm2,uwvm2-stable');ap.add_argument('--phase',choices=['all','crypto'],default='all')
    ap.add_argument('--revision',default='r1')
    ap.add_argument('--cg-limit-mib',type=int,default=32768,help='Early-stop whole-cgroup ceiling below the unchanged hard limit')
    ap.add_argument('--cohort',type=Path,help='Separate immutable code cohort; providers and results stay in workspace')
    a=ap.parse_args();r=a.workspace.resolve(strict=True);cohort=(a.cohort or r).resolve(strict=True);s=cohort/'source';results=r/'results';results.mkdir(exist_ok=True)
    spec=importlib.util.spec_from_file_location('wsea_matrix',s/'tests/security_wsea/algorithm_matrix.py')
    matrix=importlib.util.module_from_spec(spec);spec.loader.exec_module(matrix);matrix.cgroup_snapshot(True)
    profiles=json.loads((s/'tools/dependencies/wsea_linux_profiles.json').read_text())['targets']
    targets=a.targets.split(',');repos=a.repositories.split(',')
    if not set(targets)<=profiles.keys() or not set(repos)<={'uwvm2','uwvm2-stable','uwvm2-ros'}:raise ValueError('unknown target/repository')
    sdk=Path('/home/macromodel/Documents/tool-chain/x86_64-linux-gnu-llvm');cc=sdk/'bin/clang';cxx=sdk/'bin/clang++'
    hostlibs=[sdk/'lib/x86_64-unknown-linux-gnu',sdk/'lib',r/'providers/debian/usr/lib/x86_64-linux-gnu',r/'providers/mips-binutils/usr/lib/x86_64-linux-gnu']
    (r/'tmp').mkdir(mode=0o700,exist_ok=True)
    environment=dict(os.environ,TMPDIR=str(r/'tmp'),LD_LIBRARY_PATH=':'.join(str(p) for p in hostlibs if p.is_dir()))
    bounded=s/'tests/security_wsea/run_bounded_v2.py'
    inventory={'schema':'uwvm-wsea-embedded-products-v2','source_header_sha256':sha(s/'src/uwvm2/utils/security/wsea.h'),
               'driver_sha256':sha(__file__),'code_cohort':str(cohort),'start_utc_seconds':time.time(),'rows':[]}
    if not a.revision.isidentifier():raise ValueError('revision must be a simple identifier')
    index=r/('qualification-products-'+a.revision+'.json')
    def save():
        temporary=index.with_suffix('.json.tmp')
        temporary.write_text(json.dumps(inventory,indent=2)+'\n');temporary.replace(index)

    fixtures=json.loads((r/'fixtures-final/fixtures.json').read_text())
    fixture_hash=sha(r/'fixtures-final/fixtures.json')
    def qualified(kind,target,repo,out,prefix,runtime,profile):
        directory=r/('target-results' if kind=='matrix' else 'target-interop')/target
        filename='report.json' if kind=='matrix' else 'interop-report.json'
        wanted={(x['path'],x['sha256']) for x in runtime['runtime_files']}
        for candidate in sorted(directory.glob(repo+'-*/'+filename)):
            d=json.loads(candidate.read_text())
            guard=results/(kind+'-'+target+'-'+candidate.parent.name+'.json')
            if not guard.is_file():continue
            g=json.loads(guard.read_text())
            if g.get('status')!='PASS' or not g.get('owned_chain_closed'):continue
            if (d.get('status')!='PASS' or d.get('execution')!='qemu' or d.get('target')!=target or
                    d.get('fixtures_sha256')!=fixture_hash or d.get('exec_prefix')!=prefix or
                    d.get('runner_sha256')!=sha(out/'algorithm_matrix_runner') or
                    d.get('vm',{}).get('sha256')!=sha(out/'uwvm') or
                    d.get('execution_oracle_revision')!='r4'):continue
            old_runtime=d.get('runtime_receipt',{})
            if not wanted <= {(x['path'],x['sha256']) for x in old_runtime.get('metadata',{}).get('runtime_files',[])}:continue
            if sha(old_runtime['path'])!=old_runtime['sha256']:continue
            if not d.get('records') or not all(x.get('passed') for x in d['records']):continue
            if kind=='matrix':
                previous=d.get('case_profile','full')
                if profile=='full' and previous!='full':continue
                selected=matrix.selected_cases(fixtures,previous)
                expected=sum(1+bool(x.get('vm')) for x in selected)
                if len(d['records'])!=expected:continue
            elif (len(d['records'])!=482 or d.get('go_cli_sha256')!=sha(r/'native/wsea-cli') or
                  d.get('interop_driver_sha256')!=sha(r/'wasm-cosign/test/wsea/algorithm_interop.py')):continue
            print(json.dumps({'stage':kind+'-'+target+'-'+repo,'state':'REUSE_PASS',
                              'immutable_report':str(candidate)}),flush=True)
            return candidate
        return None
    def stage(name,argv,cwd=s,timeout=900):
        name+='-'+a.revision
        report=results/(name+'.json')
        if report.exists():
            old=json.loads(report.read_text())
            if old.get('status')=='PASS' and old.get('argv')==list(map(str,argv)) and old.get('owned_chain_closed'):
                return
            raise RuntimeError('retained failed/different stage requires a new output revision: '+str(report))
        cmd=['python3',str(bounded),'--disk-root',str(r),'--cg-limit-mib',str(a.cg_limit_mib),'--report',str(report),'--cwd',str(cwd),'--timeout',str(timeout),'--',*map(str,argv)]
        print(json.dumps({'stage':name,'state':'START'}),flush=True)
        if name.startswith(('product-', 'openssl-')):
            with (r/'build-admission.lock').open('a') as lock:
                fcntl.flock(lock,fcntl.LOCK_EX)
                p=subprocess.run(cmd,env=environment)
        else:
            p=subprocess.run(cmd,env=environment)
        print(json.dumps({'stage':name,'state':'PASS' if p.returncode==0 else 'FAIL','report':str(report)}),flush=True)
        if p.returncode:raise RuntimeError('stage failed: '+name)
    qroot=Path('/home/macromodel/.local/opt/qemu-10.2.1')
    qlibs=':'.join(str(qroot/p) for p in ['usr/lib/x86_64-linux-gnu','lib/x86_64-linux-gnu','usr/lib64','lib64'])
    common=Path('/tmp/uwvm-pretty-20261018/providers-ros-r1/root')
    try:
        for target in targets:
            p=profiles[target];triple=p['triple'];flags=list(p['flags']);link=[]
            # The private Clang bootstrap intentionally has a reduced backend
            # set. Use the installed complete Clang for these actual targets.
            cc=Path('/usr/bin/clang-22') if target in ('ppc64','mips64','loongarch64') else sdk/'bin/clang'
            cxx=Path('/usr/bin/clang++-22') if target in ('ppc64','mips64','loongarch64') else sdk/'bin/clang++'
            if target=='x86_64':
                runtime_root=Path('/');sysroot=Path('/');gcc=Path('/usr/lib/gcc/x86_64-linux-gnu/15')
                compile_flags=['--gcc-install-dir='+str(gcc)];libdirs=[Path('/usr/lib/x86_64-linux-gnu'),gcc]
                crypto=Path('/dev/shm/uwvm-wsea-multialg-20261018-r2/openssl/x86_64')
            elif target in ('armeb','aarch64_be'):
                runtime_root=r/'providers'/target;provider=json.loads((runtime_root/'provider.json').read_text())
                sysroot=Path(provider['sysroot']);gcc=Path(provider['gcc_install_dir'])
                target_root=runtime_root/provider['triple'];version=gcc.name
                compile_flags=['--target='+triple,'--sysroot='+str(sysroot),'--gcc-install-dir='+str(gcc),*flags,
                               '-isystem',str(sysroot/'usr/include')]
                cxxdir=target_root/'include/c++'/version
                extra_cxx=['-isystem',str(cxxdir),'-isystem',str(cxxdir/provider['triple']),'-isystem',str(cxxdir/'backward')]
                libdirs=[target_root/'lib',sysroot/'lib',sysroot/'usr/lib',sysroot/'lib64',sysroot/'usr/lib64',gcc]
                crypto=r/'openssl'/target
            else:
                runtime_root=common if target in ('i686','aarch64','arm','armhf') else r/'providers/debian'
                sysroot=runtime_root/'usr'/triple;gcc=runtime_root/'usr/lib/gcc-cross'/triple/'15'
                compile_flags=['--target='+triple,'--sysroot='+str(runtime_root),'--gcc-install-dir='+str(gcc),
                               '-idirafter',str(sysroot/'include'),*flags]
                libdirs=[sysroot/'lib',runtime_root/'usr/lib'/triple,gcc,sysroot/'lib64']
                crypto=r/'openssl'/('mips64-compiler-context' if target=='mips64' else target)
            libdirs=[p for p in libdirs if p.is_dir()]
            for directory in libdirs:link+=['-L'+str(directory)]
            if target!='x86_64':link+=['-B'+str(sysroot/'lib')]
            link+=['-latomic']
            linker=(r/'providers/debian/usr/bin/powerpc64-linux-gnu-ld' if target=='ppc64' else
                    r/'providers/mips-binutils/usr/bin/mips64-linux-gnuabi64-ld' if target=='mips64' else sdk/'bin/ld.lld')
            cxx_flags=[*compile_flags,*( ['-mllvm','-mips-tail-calls'] if target=='mips64' else []),*([] if target not in ('armeb','aarch64_be') else extra_cxx),'--ld-path='+str(linker),'-stdlib=libstdc++']
            if target in ('armeb','aarch64_be'):
                # libstdc++ uses include_next; C headers must follow its wrappers.
                position=cxx_flags.index(str(sysroot/'usr/include'))
                del cxx_flags[position-1:position+1]
                cxx_flags += ['-isystem',str(sysroot/'usr/include')]
                cxx_flags += ['-fconstexpr-steps=8388608']
            provider_files=[]
            for directory in libdirs:
                for file in directory.glob('*.so*'):
                    if not file.is_file():continue
                    if target=='x86_64' and not file.name.startswith(('libc.so','libm.so','libstdc++.so',
                            'libgcc_s.so','libatomic.so','libdl.so','libpthread.so','librt.so','libresolv.so',
                            'libutil.so','libanl.so')):continue
                    raw=file.read_bytes()
                    if raw[:4]!=b'\x7fELF':continue
                    identity=matrix.elf_identity(file)
                    if (identity['machine'],identity['bits'],identity['byte_order'])!=(p['machine'],p['bits'],p['endian']):continue
                    provider_files.append(file.resolve())
            loaders=list((sysroot/'lib').glob('ld*.so*'))+list((sysroot/'lib64').glob('ld*.so*'))
            if target=='x86_64':loaders += [Path('/lib64/ld-linux-x86-64.so.2')]
            for file in loaders:
                if file.is_file():
                    identity = matrix.elf_identity(file)
                    if (identity['machine'], identity['bits'], identity['byte_order']) == (p['machine'], p['bits'], p['endian']):
                        provider_files.append(file.resolve())
            provider_files=sorted(set(provider_files))
            runtime={'target':target,'sysroot':str(sysroot),'runtime_root':str(runtime_root),
                     'runtime_files':[{'path':str(f),'sha256':sha(f)} for f in provider_files]}
            suffix='-'+a.revision if target=='x86_64' else ''
            receipt=r/('runtime-'+target+suffix+'.json');receipt.write_text(json.dumps(runtime,indent=2)+'\n')
            if target!='x86_64':
                specfile=r/('openssl-spec-'+target+'.json')
                openssl_spec={'platform':'linux','arch':target,'cc':[str(cc)],'ar':[str(sdk/'bin/llvm-ar')],
                              'ranlib':[str(sdk/'bin/llvm-ranlib')],'cflags':['-O2','-g0','-fPIC',*compile_flags],
                              'ldflags':link,'jobs':2,'float_abi':p.get('float_abi','auto'),
                              'provider_receipt':str(receipt),'provider_receipt_sha256':sha(receipt)}
                specfile.write_text(json.dumps(openssl_spec,indent=2)+'\n')
                builder=s/'tools/dependencies/build_openssl.py'
                if target=='mips64' and (cohort/'build_openssl_mips_context.py').is_file():
                    builder=cohort/'build_openssl_mips_context.py'
                stage('openssl-'+target,['python3',builder,'--source-root',s/'third-parties/openssl',
                                        '--spec',specfile,'--output',crypto],timeout=1800)
            if a.phase=='crypto':continue
            prefix=[str(qroot/'usr/lib64/ld-linux-x86-64.so.2'),'--library-path',qlibs,str(qroot/'usr/bin'/('qemu-'+p['qemu'])),
                    '-U','LD_LIBRARY_PATH','-E','LD_LIBRARY_PATH='+':'.join(map(str,libdirs)),'-L','/']
            first_product = True
            for repo in repos:
                if repo=='uwvm2-ros' and target in ('ppc64','mips64','loongarch64'):continue
                source=cohort/'repositories'/repo
                if sha(source/'src/uwvm2/utils/security/wsea.h')!=inventory['source_header_sha256']:raise RuntimeError('repository WSEA source differs')
                out=r/'products'/target/(repo+'-'+a.revision);out.parent.mkdir(parents=True,exist_ok=True)
                build_plan={'source':str(source),'target':target,'output':str(out),'openssl_root':str(crypto),
                            'openssl_receipt':str(crypto/'receipt.json'),'cxx':[str(cxx),*cxx_flags],
                            'link_flags':[*link,*( ['-Wl,-z,noexecstack'] if target=='mips64' else [])],
                            'linker_file_sha256':{str(linker):sha(linker)}}
                if target=='mips64':
                    build_plan['linker_provider_receipt']=str(r/'providers/mips-binutils/provider.json')
                    build_plan['linker_provider_sha256']=sha(r/'providers/mips-binutils/provider.json')
                plan=r/('plan-'+target+'-'+repo+'-'+a.revision+'.json');plan.write_text(json.dumps(build_plan,indent=2)+'\n')
                reused=None
                for candidate in sorted(out.parent.glob(repo+'-*')):
                    old_report=candidate/'build-report.json'
                    if not old_report.is_file():continue
                    previous=json.loads(old_report.read_text())
                    if previous.get('status')!='PASS' or previous.get('source_header_sha256')!=inventory['source_header_sha256']:continue
                    if previous.get('compiler_sha256')!=sha(cxx) or previous.get('helper_sha256')!=sha(s/'tools/ci/build_wsea_security_cross.py'):continue
                    if any(previous['plan'].get(k)!=build_plan[k] for k in ['source','target','openssl_root','openssl_receipt','link_flags']):continue
                    old_cxx=previous['plan']['cxx'];new_cxx=build_plan['cxx']
                    if target in ('armeb','aarch64_be') and '-fconstexpr-steps=8388608' not in old_cxx:
                        # A completed evaluation at the lower budget has the same
                        # constexpr result. Keep its original, exact build receipt.
                        new_cxx=[flag for flag in new_cxx if flag!='-fconstexpr-steps=8388608']
                    if old_cxx!=new_cxx:continue
                    if not all(sha(candidate/name)==value for name,value in previous['binary_sha256'].items()):continue
                    if not all(sha(path)==value for entry in previous['stages'] for path,value in entry.get('dependency_sha256',{}).items()):continue
                    reused=candidate;break
                if reused:out=reused
                else:stage('product-'+target+'-'+repo,['python3',s/'tools/ci/build_wsea_security_cross.py','--plan',plan],timeout=1800)
                # The provider receipt remains an immutable crypto-build input;
                # this runtime receipt additionally binds the linked archive.
                execution_receipt=r/('execution-runtime-'+target+suffix+'.json')
                extended=dict(runtime,runtime_files=[*runtime['runtime_files'],{'path':str(crypto/'libcrypto.a'),'sha256':sha(crypto/'libcrypto.a')}])
                execution_receipt.write_text(json.dumps(extended,indent=2)+'\n')
                target_prefix=list(prefix)
                if target!='x86_64':
                    identity=matrix.elf_identity(out/'algorithm_matrix_runner')
                    loader=matrix.sysroot_file(sysroot,identity['interpreter'])
                    target_prefix += [str(loader),'--library-path',':'.join(map(str,libdirs))]
                common_args=['--fixtures',r/'fixtures-final','--runner',out/'algorithm_matrix_runner','--vm',out/'uwvm','--target',target,
                             '--execution','qemu','--exec-prefix-json',json.dumps(target_prefix),'--target-sysroot',sysroot,
                             '--runtime-root',runtime_root,'--runtime-receipt',execution_receipt,'--require-cgroup']
                matrix_out=r/'target-results'/target/(repo+'-'+a.revision);matrix_out.parent.mkdir(parents=True,exist_ok=True)
                profile = 'full' if first_product else 'product'
                matrix_report=qualified('matrix',target,repo,out,target_prefix,extended,profile)
                if matrix_report is None:
                    stage('matrix-'+target+'-'+repo,['python3',s/'tests/security_wsea/algorithm_matrix.py',*common_args,'--case-profile',profile,'--output',matrix_out],timeout=1800)
                    matrix_report=matrix_out/'report.json'
                interop=r/'target-interop'/target/(repo+'-'+a.revision);interop.parent.mkdir(parents=True,exist_ok=True)
                interop_report=qualified('interop',target,repo,out,target_prefix,extended,profile)
                if interop_report is None:
                    stage('interop-'+target+'-'+repo,['python3',r/'wasm-cosign/test/wsea/algorithm_interop.py','--matrix-driver',s/'tests/security_wsea/algorithm_matrix.py',
                          '--go-cli',r/'native/wsea-cli',*common_args,'--output',interop],timeout=1800)
                    interop_report=interop/'interop-report.json'
                warm=r/'target-warm'/target/(repo+'-'+a.revision)
                stage('warm-'+target+'-'+repo,['python3',s/'tests/security_wsea/warm_key_matrix.py',
                    '--fixtures',r/'fixtures-final','--runner',out/'algorithm_matrix_runner',
                    '--exec-prefix-json',json.dumps(target_prefix),'--output',warm],timeout=1800)
                row={'repository':repo,'target':target,'binary_sha256':sha(out/'uwvm'),'verifier_sha256':sha(out/'algorithm_matrix_runner'),
                     'build':str(out/'build-report.json'),'matrix':str(matrix_report),'interop':str(interop_report),'warm':str(warm/'report.json')}
                row['case_profile'] = json.loads(matrix_report.read_text()).get('case_profile','full')
                inventory['rows'].append(row);save();first_product = False
        inventory['status']='PASS'
    except BaseException as error:
        inventory['status']='FAIL';inventory['error']=repr(error);raise
    finally:inventory['end_utc_seconds']=time.time();save()
    return 0

if __name__=='__main__':raise SystemExit(main())
