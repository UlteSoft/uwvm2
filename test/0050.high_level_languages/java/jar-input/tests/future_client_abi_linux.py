#!/usr/bin/env python3
"""SOURCE ONLY: future Linux checks of the exact optional import contract.

These deliberately small Core byte fixtures test inventory refusal, not native
delivery, a full Wasm validator, TeaVM output or a successful ROS application.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import sys


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--producer',type=Path,required=True)
    p.add_argument('--producer-sha256',required=True)
    p.add_argument('--client-abi',type=Path,required=True)
    p.add_argument('--client-abi-sha256',required=True)
    p.add_argument('--helpers',type=Path,required=True)
    p.add_argument('--cpu',type=int,default=8)
    p.add_argument('--out',type=Path,required=True)
    a=p.parse_args()
    expected='/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope'
    if (sys.platform!='linux' or Path('/proc/self/cgroup').read_text().strip()!='0::'+expected
            or os.sched_getaffinity(0)!={a.cpu}):
        raise RuntimeError('authorized existing Linux cgroup and taskset required')
    def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
    for path,wanted in [(a.producer,a.producer_sha256),(a.client_abi,a.client_abi_sha256)]:
        if sha(path)!=wanted:raise RuntimeError('source input pin mismatch')
    a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=False)
    sys.path.insert(0,str(a.helpers.resolve(strict=True)/'python'))
    from linux_control import guard
    before=guard(a.out)
    spec=importlib.util.spec_from_file_location('actual_pinned_jar_producer',a.producer)
    producer=importlib.util.module_from_spec(spec);spec.loader.exec_module(producer)
    catalog=producer.read_json(a.client_abi)
    rows=[]
    def u32(value):
        result=bytearray()
        while True:
            byte=value&127;value>>=7;result.append(byte|(128 if value else 0))
            if not value:return bytes(result)
    def string(value):
        raw=value.encode('utf-8');return u32(len(raw))+raw
    def section(kind,data):return bytes([kind])+u32(len(data))+data
    values={'i32':b'\x7f','i64':b'\x7e'}
    def signature(row):
        return b'\x60'+b''.join(u32(len(row[k]))+b''.join(values[x] for x in row[k]) for k in ['params','results'])
    def fixture(contract,functions=None,type_rows=None,extra_type=False):
        functions=contract['functions'] if functions is None else functions
        type_rows=contract['functions'] if type_rows is None else type_rows
        types=section(1,u32(len(type_rows))+b''.join(signature(x) for x in type_rows))
        imports=section(2,u32(len(functions))+b''.join(string(contract['module'])+string(x['name'])+b'\0'+u32(x.get('index',i)) for i,x in enumerate(functions)))
        return b'\0asm\1\0\0\0'+types+(types if extra_type else b'')+imports
    def check(label,data,contract,rejected=None):
        path=a.out/(label+'.wasm');path.write_bytes(data)
        try:
            result=producer.wasm_imports(path,contract)
        except producer.InputContractError as error:
            if rejected is None or str(error)!=rejected:raise
            rows.append({'case':label,'expected_refusal':str(error),'fixture_sha256':sha(path)})
        else:
            if rejected is not None:raise AssertionError('required refusal absent: '+label)
            rows.append({'case':label,'imports':result.get('optional_service_client_imports'),
                         'fixture_sha256':sha(path)})
    for backend in ['1','2']:
        c=producer.client_abi_contract(catalog,backend)
        check('backend'+backend,fixture(c),c)
        check('backend'+backend+'-abi-only',fixture(c,functions=[c['functions'][0]]),c)
        f=[dict(x) for x in c['functions']];f[1]={**f[1],'name':'unknown'}
        check('backend'+backend+'-unknown',fixture(c,functions=f),c,'unknown optional service-client function')
        f=[dict(c['functions'][0]),{**c['functions'][0],'index':0}]
        check('backend'+backend+'-duplicate',fixture(c,functions=f),c,'duplicate optional service-client import')
        f=[{**c['functions'][0],'index':99}]
        check('backend'+backend+'-bad-index',fixture(c,functions=f),c,'optional service-client type index out of bounds')
        types=[dict(x) for x in c['functions']];types[1]={**types[1],'params':['i32']}
        check('backend'+backend+'-bad-param',fixture(c,type_rows=types),c,'optional service-client import signature mismatch')
        types=[dict(x) for x in c['functions']];types[2]={**types[2],'results':['i64']}
        check('backend'+backend+'-bad-result',fixture(c,type_rows=types),c,'optional service-client import signature mismatch')
        check('backend'+backend+'-duplicate-type-section',fixture(c,extra_type=True),c,'duplicate Core type section')
        recursive=b'\0asm\1\0\0\0'+section(1,b'\x01\x4e')
        check('backend'+backend+'-recursive-refused',recursive,c,'optional client inventory requires a linear Core function type section')
    after=guard(a.out)
    if (before['boot']!=after['boot'] or before['cgroup']!=after['cgroup']
            or before['memory_events']!=after['memory_events']):raise RuntimeError('admission drift')
    for path,wanted in [(a.producer,a.producer_sha256),(a.client_abi,a.client_abi_sha256)]:
        if sha(path)!=wanted:raise RuntimeError('source changed during actual checks')
    report={'passed':True,'scope':'parser contract only','cases':rows,'before':before,'after':after,
            'producer_sha256':a.producer_sha256,'abi_sha256':a.client_abi_sha256,
            'JAR_translation_count':0,'Guest_execution_count':0,'native_delivery_count':0,
            'full_Wasm_validation_count':0,'authority_granted':False}
    (a.out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'passed':True,'cases':len(rows),'scope':report['scope']}))


if __name__=='__main__':main()
