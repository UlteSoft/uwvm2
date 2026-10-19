#!/usr/bin/env python3
"""Build the pinned OpenSSL source out of tree; never search host OpenSSL.

A spec is JSON with platform, arch, cc/ar/ranlib (argv lists), cflags (argv),
ldflags (argv), optional configure_target, and jobs. No target code is executed.
The caller owns resource/cgroup admission. This helper never installs globally.
"""
from __future__ import annotations
import argparse
import contextlib
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import struct
import subprocess
import sys
import time

RELEASE = '3.5.9'
RECIPE = 'uwvm-openssl-source.1'
EXPECTED_MANIFEST_SHA256 = 'ecf889968127e9cac6cda5fe572beb69e2a3e20e276ab7bb4a1e730b1b72b30b'
ARCHES = {
    'x86_64': (62, 64, 'little', 'linux-x86_64'),
    'i686': (3, 32, 'little', 'linux-x86'),
    'aarch64': (183, 64, 'little', 'linux-aarch64'),
    'arm': (40, 32, 'little', 'linux-armv4'),
    'armhf': (40, 32, 'little', 'linux-armv4'),
    'armeb': (40, 32, 'big', 'linux-armv4'),
    'aarch64_be': (183, 64, 'big', 'linux-aarch64'),
    'ppc64': (21, 64, 'big', 'linux-ppc64'),
    'ppc64le': (21, 64, 'little', 'linux-ppc64le'),
    'mips64': (8, 64, 'big', 'linux64-mips64'),
    'mips64el': (8, 64, 'little', 'linux64-mips64'),
    'loongarch64': (258, 64, 'little', 'linux64-loongarch64'),
    'riscv64': (243, 64, 'little', 'linux64-riscv64'),
}
ALIASES = {'x64':'x86_64', 'amd64':'x86_64', 'x86':'i686', 'i386':'i686',
           'arm64':'aarch64', 'arm64be':'aarch64_be', 'aarch64be':'aarch64_be',
           'armv7':'arm', 'armv7a':'arm', 'armv7-a':'arm', 'loong64':'loongarch64',
           'powerpc64':'ppc64', 'powerpc64le':'ppc64le'}


def sha(path: Path) -> str:
    h=hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024), b''): h.update(chunk)
    return h.hexdigest()


def verify_source(root: Path) -> str:
    manifest = root/'sources.sha256'
    if sha(manifest)!=EXPECTED_MANIFEST_SHA256:raise ValueError('OpenSSL release source manifest pin mismatch')
    recorded=set()
    for line in manifest.read_text().splitlines():
        digest, name = line.split('  ', 1)
        rel=Path(name)
        if len(digest)!=64 or rel.is_absolute() or '..' in rel.parts or name in recorded:
            raise ValueError('malformed OpenSSL source manifest')
        recorded.add(name)
        file=root/rel
        if file.is_symlink() or not file.is_file() or sha(file)!=digest:
            raise ValueError('pinned OpenSSL source mismatch: '+name)
    actual={p.relative_to(root).as_posix() for p in (root/'upstream').rglob('*') if p.is_file() or p.is_symlink()}
    if actual != recorded: raise ValueError('unmanifested or missing OpenSSL source')
    version=(root/'upstream/VERSION.dat').read_text()
    for key,val in [('MAJOR','3'),('MINOR','5'),('PATCH','9')]:
        if key+'='+val+'\n' not in version: raise ValueError('OpenSSL release mismatch')
    return sha(manifest)


def command(value, name):
    if not isinstance(value,list) or not value or not all(isinstance(x,str) and '\x00' not in x for x in value):
        raise ValueError(name+' must be a nonempty argv array')
    resolved=shutil.which(value[0])
    if not resolved: raise ValueError('missing '+name+': '+value[0])
    # Preserve argv[0]: llvm-ranlib/llvm-ar may be symlinks to one dispatcher.
    return [str(Path(resolved).absolute()),*value[1:]]


def identify_elf(data: bytes, arch: str):
    machine,bits,endian,_=ARCHES[arch]
    if len(data)<52 or data[:4]!=b'\x7fELF': raise ValueError('compiler did not emit ELF')
    if data[4] != {32:1,64:2}[bits] or data[5] != {'little':1,'big':2}[endian]:
        raise ValueError('compiler ELF class/endian mismatches requested '+arch)
    order='<' if endian=='little' else '>'
    actual=struct.unpack_from(order+'H',data,18)[0]
    if actual!=machine: raise ValueError('compiler ELF machine mismatches requested '+arch)
    flags=struct.unpack_from(order+'I',data,36 if bits==32 else 48)[0]
    if arch in ('arm','armhf','armeb'):
        if (flags>>24)!=5: raise ValueError('ARM target requires EABI5')
        # ET_REL may omit float e_flags. Predefined macro check below binds PCS.
    if arch=='loongarch64' and flags & 7 != 3:
        raise ValueError('LoongArch target requires LP64D floating-point ABI')
    if arch in ('mips64','mips64el') and flags & (0x20|0xf000):
        raise ValueError('MIPS n32 ABI is not the required n64 ABI')
    return {'class':bits,'endian':endian,'machine':actual,'e_flags':hex(flags)}


def audit_archive(path: Path, arch: str):
    # Inspect actual OpenSSL archive members, not just the compiler probe.
    count=0;flag_values=set()
    with path.open('rb') as f:
        if f.read(8)!=b'!<arch>\n':raise ValueError('OpenSSL archive is not a regular ar archive')
        while header:=f.read(60):
            if len(header)!=60 or header[58:]!=b'`\n':raise ValueError('invalid OpenSSL ar member')
            name=header[:16].decode('ascii').strip();size=int(header[48:58]);data=f.read(size)
            if len(data)!=size:raise ValueError('truncated OpenSSL archive member')
            if size%2:f.read(1)
            if name in ('/','//','/SYM64/'):continue
            if name.startswith('#1/'):
                width=int(name[3:]);data=data[width:]
            identity=identify_elf(data,arch);flag_values.add(identity['e_flags']);count+=1
    if count<1:raise ValueError('OpenSSL archive contains no target ELF objects')
    return {'archive_sha256':sha(path),'elf_objects':count,'elf_class':ARCHES[arch][1],
            'machine':ARCHES[arch][0],'byte_order':ARCHES[arch][2],'member_e_flags':sorted(flag_values)}


def probe(spec, build: Path, env):
    cc=spec['cc']; flags=spec['cflags']; arch=spec['arch']
    src=build/'abi-probe.c'; obj=build/'abi-probe.o'
    src.write_text('int uwvm_openssl_target_abi_probe;\n')
    subprocess.run([*cc,*flags,'-c',str(src),'-o',str(obj)],check=True,env=env)
    result={}
    if spec['platform'] in ('linux','unix','bsd','freebsd','netbsd','openbsd','dragonflybsd','sun','cross'):
        result=identify_elf(obj.read_bytes(),arch)
    macros=subprocess.check_output([*cc,*flags,'-dM','-E','-x','c','-'],input=b'',env=env).decode()
    float_abi=spec.get('float_abi','auto')
    if float_abi=='auto':float_abi='hard' if arch in ('armhf','armeb') else 'soft'
    if float_abi not in ('hard','soft'):raise ValueError('float_abi must be auto, hard or soft')
    if arch in ('arm','armhf','armeb') and float_abi=='hard' and '#define __ARM_PCS_VFP 1' not in macros:
        raise ValueError(arch+' requires actual hard-float PCS (__ARM_PCS_VFP)')
    if arch in ('arm','armhf','armeb') and float_abi=='soft' and '#define __ARM_PCS_VFP 1' in macros:
        raise ValueError(arch+' requires the explicitly selected soft-float PCS')
    if arch=='ppc64' and '#define _CALL_ELF 1' not in macros:
        raise ValueError('ppc64 requires big-endian ELFv1 ABI')
    if arch=='aarch64_be' and '#define __AARCH64EB__ 1' not in macros:
        raise ValueError('aarch64_be requires actual big-endian target macros')
    if arch=='armeb' and '#define __ARMEB__ 1' not in macros:
        raise ValueError('armeb requires actual big-endian target macros')
    if arch in ('mips64','mips64el') and not ('#define _MIPS_SIM _ABI64' in macros or '#define _MIPS_SIM 3' in macros):
        raise ValueError('MIPS target compiler macros require actual n64 ABI')
    if arch=='loongarch64' and ('#define __loongarch_grlen 64' not in macros or '#define __loongarch_double_float 1' not in macros):
        raise ValueError('LoongArch target compiler macros require LP64D')
    result['float_abi']=float_abi if arch in ('arm','armhf','armeb') else 'not-applicable'
    result['macros_sha256']=hashlib.sha256(macros.encode()).hexdigest()
    return result


@contextlib.contextmanager
def lock(path: Path):
    if os.name!='posix': raise ValueError('bundled recipe requires a POSIX build host; use explicit openssl-root/provider=package on this host')
    import fcntl
    with path.open('a+b') as f:
        fcntl.flock(f.fileno(),fcntl.LOCK_EX)
        yield


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source-root',type=Path,required=True)
    ap.add_argument('--spec',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args()
    root=a.source_root.resolve(); output=a.output.resolve()
    if output==root or root in output.parents: raise ValueError('build output must be outside the vendored source')
    spec=json.loads(a.spec.read_text())
    arch=str(spec['arch']).lower();spec['arch']=ALIASES.get(arch,arch)
    if spec['arch'] not in ARCHES: raise ValueError('unsupported bundled OpenSSL architecture: '+arch+'; provide an explicit prebuilt root/package')
    for name in ('cc','ar','ranlib'):spec[name]=command(spec[name],name)
    for name in ('cflags','ldflags'):
        if not isinstance(spec.get(name,[]),list) or not all(isinstance(x,str) and '\x00' not in x for x in spec.get(name,[])):
            raise ValueError(name+' must be an argv array')
        spec.setdefault(name,[])
    jobs=int(spec.get('jobs',1))
    if jobs<1 or jobs>1024:raise ValueError('invalid build jobs')
    platform=spec['platform']; target=spec.get('configure_target')
    if not target:
        if platform in ('linux','cross'):target=ARCHES[spec['arch']][3]
        elif platform=='macosx':target={'aarch64':'darwin64-arm64-cc','x86_64':'darwin64-x86_64-cc'}.get(spec['arch'])
        elif platform in ('bsd','freebsd','openbsd','netbsd','dragonflybsd'):
            target={'x86_64':'BSD-x86_64','i686':'BSD-x86','aarch64':'BSD-aarch64'}.get(spec['arch'])
    if not target:raise ValueError('pass an explicit openssl-configure-target for '+platform+'/'+arch)
    if not isinstance(target,str) or not target or any(x in target for x in '\n\r\x00'):
        raise ValueError('invalid Configure target')
    provider=spec.get('provider_receipt')
    if provider:
        provider_path=Path(provider)
        if not provider_path.is_absolute() or sha(provider_path)!=spec.get('provider_receipt_sha256'):
            raise ValueError('target provider receipt identity mismatch')
        metadata=json.loads(provider_path.read_text())
        for entry in metadata.get('runtime_files',[]):
            file=Path(entry['path'])
            if not file.is_absolute() or not file.is_file() or sha(file)!=entry['sha256']:
                raise ValueError('target provider runtime mismatch: '+str(file))
    manifest=verify_source(root)
    identity={'recipe':RECIPE,'release':RELEASE,'source_manifest':manifest,'spec':{k:v for k,v in spec.items() if k!='jobs'},
              'tools':{k:{'path':v[0],'sha256':sha(Path(v[0]))} for k,v in spec.items() if k in ('cc','ar','ranlib')},
              'builder_sha256':sha(Path(__file__).resolve()),'configure_target':target}
    identity_hash=hashlib.sha256(json.dumps(identity,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    output.mkdir(parents=True,exist_ok=True)
    with lock(output/'.lock'):
        receipt_file=output/'receipt.json'
        if receipt_file.exists():
            old=json.loads(receipt_file.read_text())
            if old.get('identity_sha256')==identity_hash and all((output/p).is_file() and sha(output/p)==h for p,h in old.get('artifacts',{}).items()) and len(old.get('artifacts',{}))>=3:
                print(str(receipt_file));return
        # Never reconfigure a Makefile for a different compiler/ABI in place.
        work=output/'build'
        if work.exists():shutil.rmtree(work)
        work.mkdir()
        env=os.environ.copy()
        # Ambient OpenSSL/host compiler search overrides must not change this build.
        for name in ('CC','CXX','CFLAGS','CXXFLAGS','CPPFLAGS','LDFLAGS','CROSS_COMPILE','AR','ARFLAGS','RANLIB','LD','LIBRARY_PATH','CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','OPENSSL_CONF','OPENSSL_MODULES'):
            env.pop(name,None)
        # Upstream sha512-mips.pl probes MIPSEB with $CC -E, without
        # CFLAGS. A generic Clang executable otherwise reports the build host
        # and silently generates little-endian code for a big-endian target.
        # Keep the complete configured C context in CC for that generator.
        effective_cc=[*spec['cc'],*(spec['cflags'] if spec['arch']=='mips64' else [])]
        env.update({'CC':shlex.join(effective_cc),'AR':shlex.join(spec['ar']),'RANLIB':shlex.join(spec['ranlib']),
                    'CFLAGS':shlex.join(spec['cflags']),'LDFLAGS':shlex.join(spec['ldflags'])})
        elf=probe(spec,work,env)
        perl=shutil.which('perl'); make=shutil.which('gmake') or shutil.which('make')
        if not perl or not make:raise ValueError('bundled OpenSSL requires Perl5 and make')
        args=[perl,str(root/'upstream/Configure'),target,'no-shared','no-tests','no-apps','no-module',
              '--prefix='+str(output/'install'),'--openssldir='+str(output/'install/ssl'),'--libdir=lib']
        if spec.get('no_asm'):args.append('no-asm')
        started=time.time()
        with (output/'build.log').open('w') as log:
            log.write('Configure argv: '+json.dumps(args)+'\n');log.flush()
            subprocess.run(args,cwd=work,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
            subprocess.run([make,'-j'+str(jobs),'build_libs'],cwd=work,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
        # Keep generated and upstream headers together, without install/global writes.
        include=output/'include/openssl'
        if include.parent.exists():shutil.rmtree(include.parent)
        include.mkdir(parents=True)
        for directory in (root/'upstream/include/openssl',work/'include/openssl'):
            for p in directory.glob('*.h'):shutil.copy2(p,include/p.name)
        for name in ('libcrypto.a','libssl.a'):shutil.copy2(work/name,output/name)
        archive_abi={}
        if platform in ('linux','cross','unix','bsd','freebsd','openbsd','netbsd','dragonflybsd','sun'):
            archive_abi={name:audit_archive(output/name,spec['arch']) for name in ('libcrypto.a','libssl.a')}
        artifacts={p.relative_to(output).as_posix():sha(p) for p in [output/'libcrypto.a',output/'libssl.a',*sorted(include.glob('*.h'))]}
        receipt={'identity_sha256':identity_hash,'identity':identity,'compiler_abi_probe':elf,'actual_archive_abi':archive_abi,'configure_argv':args,
                 'effective_cc_argv':effective_cc,'make_argv':[make,'-j'+str(jobs),'build_libs'],'start_utc_seconds':started,'end_utc_seconds':time.time(),
                 'artifacts':artifacts,'target_execution':False,'global_install':False}
        if not spec.get('keep_work',False):
            # Retain configure metadata while bounding per-target disk usage.
            for filename in ('configdata.pm','Makefile'):
                if (work/filename).is_file():shutil.copy2(work/filename,output/filename)
            shutil.rmtree(work)
        temp=receipt_file.with_suffix('.json.tmp');temp.write_text(json.dumps(receipt,indent=2)+'\n');temp.replace(receipt_file)
        print(str(receipt_file))


if __name__=='__main__':
    try:main()
    except (ValueError,OSError,subprocess.CalledProcessError) as e:
        print('bundled OpenSSL: '+str(e),file=sys.stderr);sys.exit(1)
