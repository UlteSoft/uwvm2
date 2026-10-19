#!/usr/bin/env python3
"""File-only preparation of pinned Bootlin target SDK inputs; executes no SDK tool.

The extracted subset keeps target libc, C++ headers/runtime, GCC target builtins
and upstream metadata. Host compiler executables are deliberately not activated.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import posixpath
import shutil
import tarfile

PINS={
 'aarch64_be': {'name':'aarch64be','triple':'aarch64_be-buildroot-linux-gnu','sha256':'82aef57aec7bc4d0da48b5a045bef2b4d48b2d9c32f4876563009e43829476a7','float_abi':'aapcs64'},
 'armeb': {'name':'armebv7-eabihf','triple':'armeb-buildroot-linux-gnueabihf','sha256':'4d93a7b26c0909ea7574e98246522cf6b77491d43a7ca7aa08ae1c29ecf07b0d','float_abi':'hard'},
}
RELEASE='glibc--stable-2026.08-1'


def digest(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for chunk in iter(lambda:f.read(1048576),b''):h.update(chunk)
 return h.hexdigest()


def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--target',choices=PINS,required=True);ap.add_argument('--archive',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
 pin=PINS[a.target];name=pin['name']+'--'+RELEASE;triple=pin['triple'];out=a.output.absolute()
 if digest(a.archive)!=pin['sha256']:raise ValueError('official archive checksum mismatch')
 out.mkdir(mode=0o700,parents=True,exist_ok=False)
 symlinks=[];hardlinks=[];rows=[]
 with tarfile.open(a.archive, mode="r|xz") as tar:
  for m in tar:
   p=PurePosixPath(m.name)
   if p.is_absolute() or '..' in p.parts or not p.parts or p.parts[0]!=name:raise ValueError('unsafe archive member: '+m.name)
   rel=PurePosixPath(*p.parts[1:])
   parts=rel.parts
   sysroot_input=(len(parts)>=3 and parts[0]==triple and parts[1]=='sysroot' and
                  (parts[2] in ('lib','lib64') or (len(parts)>=4 and parts[2]=='usr' and parts[3] in ('include','lib','lib64'))))
   wanted=sysroot_input or (len(parts)>=2 and parts[0]==triple and parts[1] in ('include','lib','lib64')) or parts[:3]==('lib','gcc',triple) or parts[:2]==('share','buildroot')
   if not wanted:continue
   if m.isdev() or m.isfifo():raise ValueError('unexpected device/FIFO')
   dest=out/Path(rel)
   if m.isdir():dest.mkdir(mode=0o755,parents=True,exist_ok=True);continue
   if m.issym():
    link=m.linkname
    if link.startswith('/'):
     if 'sysroot' not in rel.parts:raise ValueError('absolute link outside target sysroot')
     sysroot=PurePosixPath(triple,'sysroot');target=sysroot/PurePosixPath(link.lstrip('/'))
     link=posixpath.relpath(str(target),str(rel.parent))
    norm=PurePosixPath(posixpath.normpath(str(rel.parent/link)))
    if norm.is_absolute() or '..' in norm.parts:raise ValueError('escaping SDK symlink')
    symlinks.append((dest,link,m.linkname));continue
   if not (m.isfile() or m.islnk()):raise ValueError('unsupported archive member type')
   if m.islnk():
    lp=PurePosixPath(m.linkname)
    if lp.is_absolute() or '..' in lp.parts or lp.parts[0]!=name:raise ValueError('unsafe hardlink')
    hardlinks.append((dest,out/Path(*lp.parts[1:]),rel.as_posix(),m.mode));continue
   dest.parent.mkdir(mode=0o755,parents=True,exist_ok=True)
   with tar.extractfile(m) as src,dest.open('xb') as dst:shutil.copyfileobj(src,dst)
   os.chmod(dest,m.mode&0o777&~0o022)
   rows.append({'path':rel.as_posix(),'sha256':digest(dest)})
  while hardlinks:
   remaining=[];progress=False
   for dest,source,relative,mode in hardlinks:
    if source.is_file() and not source.is_symlink():
     dest.parent.mkdir(mode=0o755,parents=True,exist_ok=True);shutil.copyfile(source,dest);os.chmod(dest,mode&0o777&~0o022)
     rows.append({'path':relative,'sha256':digest(dest)});progress=True
    else:remaining.append((dest,source,relative,mode))
   if not progress:raise ValueError('hardlink target missing from retained SDK subset')
   hardlinks=remaining
  for dest,link,original in symlinks:
   dest.parent.mkdir(mode=0o755,parents=True,exist_ok=True);dest.symlink_to(link)
   rows.append({'path':dest.relative_to(out).as_posix(),'symlink':link,'archive_link':original})
 for dest,_,_ in symlinks:
  if not dest.resolve(strict=False).is_relative_to(out):raise ValueError('SDK symlink chain escapes extraction root')
 receipt={'target':a.target,'provider':'Bootlin GCC15.3 glibc2.44 stable2026.08-1 target subset','triple':triple,'float_abi':pin['float_abi'],
          'archive_url':'https://toolchains.bootlin.com/downloads/releases/toolchains/'+pin['name']+'/tarballs/'+name+'.tar.xz','archive_sha256':pin['sha256'],
          'checksum_url':'https://toolchains.bootlin.com/downloads/releases/toolchains/'+pin['name']+'/tarballs/'+name+'.sha256',
          'source_offer_url':'https://toolchains.bootlin.com/downloads/releases/sources/','license_offer_url':'https://toolchains.bootlin.com/downloads/releases/licenses/',
          'runtime_root':str(out),'sysroot':str(out/triple/'sysroot'),'gcc_install_dir':str(out/'lib/gcc'/triple/'15.3.0'),
          'files':sorted(rows,key=lambda r:r['path']),'host_sdk_execution':False,'archive_extraction':'regular/hardlink contents first, checked symlinks last; absolute sysroot links rewritten to relative target-root equivalents'}
 (out/'provider.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print(out/'provider.json')

if __name__=='__main__':main()
