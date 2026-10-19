#!/usr/bin/env python3
"""Future actual JAR -> Wasm qualification, SOURCE ONLY until Linux release.

Creates real JARs with the real JDK, calls the producer, then runs the same
producer Wasm in Wasmtime and three UWVM products. No JVM application oracle,
mock provider or successful replacement native is used.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import sys


def sha(path):
    with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()


def expected_explicit(arguments,environment):
    def describe(name,value):
        if value is None:return name+'=NULL\n'
        b=value.encode('utf-16-le')
        return name+'=len:'+str(len(b)//2)+',utf16:'+','.join(str(int.from_bytes(b[i:i+2],'little')) for i in range(0,len(b),2))+'\n'
    return 'dependency-jar: ready\nargc='+str(len(arguments))+'\n'+''.join(describe(f'arg[{i}]',x) for i,x in enumerate(arguments))+''.join(describe('env['+k+']',environment.get(k)) for k in ['UWVM_JAR_EMPTY','UWVM_JAR_UTF8','UWVM_JAR_EQUALS','UWVM_JAR_MISSING'])


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['producer','helpers','m2','toolchain-receipt','patch-root','maven','java-home','wasmtime','uwvm2','uwvm2-stable','uwvm2-ros','out']:
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--toolchain-receipt-sha256',required=True)
    p.add_argument('--cpu',type=int,default=8)
    a=p.parse_args()
    if sys.platform!='linux' or os.sched_getaffinity(0)!={a.cpu}:raise RuntimeError('authorized Linux taskset required')
    a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=False)
    helpers=a.helpers.resolve(strict=True);sys.path.insert(0,str(helpers/'python'))
    from linux_control import guard,invoke
    before=guard(a.out)
    record={'passed':False,'kind':'actual future qualification, never source-only PASS','before':before,'cases':[],
            'input_hashes':{},'commands':[],'JVM_application_executed':False,'ROS_authority_granted':False,'threads_claim':False}
    def save():(a.out/'report.json').write_text(json.dumps(record,indent=2)+'\n')
    def stop(sig,frame):
        for s in (signal.SIGINT,signal.SIGTERM,signal.SIGHUP):signal.signal(s,signal.SIG_IGN)
        raise InterruptedError(str(sig))
    for s in (signal.SIGINT,signal.SIGTERM,signal.SIGHUP):signal.signal(s,stop)
    env={'LC_ALL':'C.UTF-8','JAVA_HOME':str(a.java_home),'PATH':str(a.java_home/'bin')+':/usr/bin:/bin'}
    def run(argv,name,expected_exit=0):
        folder=a.out/'commands'/name;row={'argv':list(map(str,argv)),'folder':str(folder)};record['commands'].append(row);save()
        try:return invoke(argv,folder,cwd=a.out,env=env,timeout=900,expected_exit=expected_exit)
        finally:
            if (folder/'receipt.json').is_file():row['receipt_sha256']=sha(folder/'receipt.json')
            save()
    try:
        source=Path(__file__).resolve().parent/'src'
        for path in [a.producer,Path(__file__),*source.rglob('*.java'),helpers/'python/linux_control.py',helpers/'owned_process.py',a.toolchain_receipt]:record['input_hashes'][str(path.resolve(strict=True))]=sha(path)
        jdk=a.java_home/'bin';classes={}
        for name in ['dep','manifest','explicit','native']:
            classes[name]=a.out/('classes-'+name);classes[name].mkdir()
        def javac(name,src,extra=()):
            run([jdk/'javac','-g','-proc:none','-encoding','UTF-8','-source','17','-target','17',*extra,'-d',classes[name],source/src],'javac-'+name)
        javac('dep','dep/Message.java');javac('manifest','demo/ManifestMain.java')
        javac('explicit','demo/ExplicitMain.java',['-cp',str(classes['dep'])]);javac('native','demo/NativeFailure.java')
        jars={}
        for name in classes:
            jars[name]=a.out/(name+'.jar')
            mainflag=['--main-class','demo.ManifestMain'] if name=='manifest' else []
            run([jdk/'jar','--create','--file',jars[name],*mainflag,'-C',classes[name],'.'],'jar-'+name)
        common=[sys.executable,a.producer,'--helpers',helpers,'--m2',a.m2,'--toolchain-receipt',a.toolchain_receipt,
                '--toolchain-receipt-sha256',a.toolchain_receipt_sha256,'--patch-root',a.patch_root,'--maven',a.maven,
                '--java-home',a.java_home,'--cpu',a.cpu]
        def translate(label,jar,extra=(),negative=None):
            out=a.out/('translation-'+label)
            run([*common,'--jar',jar,*extra,'--out',out],'producer-'+label,1 if negative else 0)
            receipt=json.loads((out/'receipt.json').read_text())
            item={'case':label,'producer_receipt':str(out/'receipt.json'),'producer_receipt_sha256':sha(out/'receipt.json'),
                  'negative':bool(negative),'expected_failure_kind':negative,'producer_passed':receipt['passed']}
            record['cases'].append(item);save()
            if negative:
                assert not receipt['passed'] and receipt['error']
                if negative=='missing-main':
                    assert receipt['error']['type']=='InputContractError' and receipt['error']['message']=='no Main-Class: pass --main-class'
                    assert not receipt['commands']
                else:
                    command=next(x for x in receipt['commands'] if Path(x['folder']).name=='teavm')
                    actual=command['actual'];assert actual['exit']!=0 and not actual['passed']
                    errors=(Path(command['folder'])/'stdout.log').read_text(errors='replace')+(Path(command['folder'])/'stderr.log').read_text(errors='replace')
                    needle='unavailableNative' if negative=='unresolved-native' else 'dep.Message'
                    assert needle in errors, 'failure must be real TeaVM diagnostic for the intended missing dependency'
                item['expected_failure_observed']=True;save();return None
            assert receipt['passed'];return Path(receipt['artifact']['path'])
        normal=translate('manifest',jars['manifest'])  # Genuine JAR-only input; no local application source map.
        explicit=translate('explicit-dependency',jars['explicit'],['--main-class','demo.ExplicitMain','--dependency-jar',jars['dep'],'--source-root',source])
        translate('no-main',jars['explicit'],['--dependency-jar',jars['dep']],negative='missing-main')
        translate('native',jars['native'],['--main-class','demo.NativeFailure'],negative='unresolved-native')
        translate('missing-dependency',jars['explicit'],['--main-class','demo.ExplicitMain'],negative='unresolved-dependency')
        arguments=['','雪=🙂','a=b=c','='];values={'UWVM_JAR_EMPTY':'','UWVM_JAR_UTF8':'雪=🙂','UWVM_JAR_EQUALS':'a=b=c'}
        features=['gc','exceptions','tail-call','function-references','reference-types','simd','relaxed-simd','extended-const','multi-memory','memory64','table64']
        for label,wasm,args,environment,wanted in [('manifest',normal,[],{},'manifest-jar: 42\n'),('explicit-unicode',explicit,arguments,values,expected_explicit(arguments,values))]:
            wasm_sha=sha(wasm)
            flags=[x for k,v in environment.items() for x in ['--env',k+'='+v]]
            commands={'wasmtime':[a.wasmtime,'run','-C','cache=n,parallel-compilation=n',*flags,wasm,*args]}
            for repo in ['uwvm2','uwvm2-stable','uwvm2-ros']:
                mode=['-Rcm','full','-Rcc','jit'] if repo=='uwvm2' else ['-Raot']
                extra=['--wasip1-global-noinherit-system-environment',*[x for k,v in environment.items() for x in ['--wasip1-global-add-or-replace-environment',k,v]]]
                commands[repo]=[getattr(a,repo.replace('-','_')),*mode,'-Rct','0','-Rllvm-cache-path','disable','-Rclog','err',*['-WFE-'+x for x in features],*extra,'--run',wasm,*args]
            for engine,command in commands.items():
                name='guest-'+label+'-'+engine;row=run(command,name);folder=a.out/'commands'/name
                stdout=(folder/'stdout.log').read_bytes();stderr=(folder/'stderr.log').read_bytes()
                # Guest stderr is empty in these real fixtures. If a product
                # emits known CLI diagnostics, preserve and investigate, rather
                # than broadly strip or make a fake success table.
                assert row['exit']==0 and stdout==wanted.encode() and stderr==b''
                assert row.get('retired') and row.get('ownership',{}).get('closed')
                assert all(not m['pidfd_retained'] for m in row['ownership']['members'])
                assert sha(wasm)==wasm_sha
                record['cases'].append({'case':label,'engine':engine,'wasm_sha256':wasm_sha,'exact_stdout':True,
                                        'empty_stderr':True,'retired':row.get('retired'),'ownership':row.get('ownership')});save()
        record['passed']=True
    except BaseException as error:record['error']=repr(error);record['passed']=False
    finally:
        for s in (signal.SIGINT,signal.SIGTERM,signal.SIGHUP):signal.signal(s,signal.SIG_IGN)
        try:
            record['input_hashes_after']={k:sha(k) for k in record['input_hashes']};assert record['input_hashes']==record['input_hashes_after']
            record['after']=guard(a.out);assert before['boot']==record['after']['boot'] and before['memory_events']==record['after']['memory_events']
        except BaseException as error:record['finalization_error']=repr(error);record['passed']=False
        save()
    return 0 if record['passed'] else 1


if __name__=='__main__':raise SystemExit(main())
