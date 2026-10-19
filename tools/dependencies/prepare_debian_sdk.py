#!/usr/bin/env python3
"""File-only preparation of already authenticated Debian target-provider debs.

The input index must be generated from a Packages index authenticated against a
pinned Debian InRelease signing key. This program verifies every deb SHA-256,
extracts only data archives into a private fresh root and never installs a
package or executes a maintainer/compiler/linker script.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path,PurePosixPath
import posixpath
import shutil
import tarfile


def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()


def data_archive(path):
 with path.open('rb') as f:
  if f.read(8)!=b'!<arch>\n':raise ValueError('invalid deb ar magic')
  while header:=f.read(60):
   if len(header)!=60 or header[58:]!=b'`\n':raise ValueError('invalid deb ar entry')
   name=header[:16].decode().strip().rstrip('/');size=int(header[48:58]);payload=f.read(size)
   if len(payload)!=size:raise ValueError('truncated deb ar member')
   if size%2:f.read(1)
   if name.startswith('data.tar'):return payload
 raise ValueError('deb data archive missing')


def relative(name):
 p=PurePosixPath(name)
 if p.is_absolute() or '..' in p.parts:raise ValueError('unsafe deb path')
 return Path(*[part for part in p.parts if part!='.'])


def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--index-json',type=Path,required=True);ap.add_argument('--packages-dir',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
 meta=json.loads(a.index_json.read_text());out=a.output.absolute();out.mkdir(mode=0o700,parents=True,exist_ok=False)
 symlinks={};hardlinks=[];packages=[]
 for name,entry in meta['packages'].items():
  deb=a.packages_dir/Path(entry['Filename']).name
  if sha(deb)!=entry['SHA256']:raise ValueError('deb SHA256 mismatch: '+str(deb))
  with tarfile.open(fileobj=io.BytesIO(data_archive(deb)),mode='r:*') as tar:
   for m in tar:
    rel=relative(m.name);dest=out/rel
    if m.isdir():dest.mkdir(mode=0o755,parents=True,exist_ok=True);continue
    if m.issym():
     link=m.linkname
     if link.startswith('/'):link=posixpath.relpath(link.lstrip('/'),rel.parent.as_posix())
     norm=PurePosixPath(posixpath.normpath(str(PurePosixPath(rel.parent.as_posix())/link)))
     if norm.is_absolute() or '..' in norm.parts:raise ValueError('escaping deb symlink')
     symlinks[dest]=(link,m.linkname);continue
    if m.islnk():hardlinks.append((dest,out/relative(m.linkname),m.mode));continue
    if not m.isfile():raise ValueError('unexpected deb special file')
    dest.parent.mkdir(mode=0o755,parents=True,exist_ok=True)
    with tar.extractfile(m) as src,dest.open('wb') as dst:shutil.copyfileobj(src,dst)
    os.chmod(dest,m.mode&0o777&~0o022)
  packages.append(entry)
 while hardlinks:
  pending=[];progress=False
  for dest,source,mode in hardlinks:
   if source.is_file() and not source.is_symlink():
    dest.parent.mkdir(mode=0o755,parents=True,exist_ok=True);shutil.copyfile(source,dest);os.chmod(dest,mode&0o777&~0o022);progress=True
   else:pending.append((dest,source,mode))
  if not progress:raise ValueError('unresolved deb hardlink')
  hardlinks=pending
 for dest,(link,_) in symlinks.items():
  dest.parent.mkdir(mode=0o755,parents=True,exist_ok=True)
  if dest.exists():raise ValueError('deb symlink overwrites existing regular file: '+str(dest)+' -> '+link)
  dest.symlink_to(link)
 for dest in symlinks:
  if not dest.resolve(strict=False).is_relative_to(out):raise ValueError('deb symlink chain escapes provider')
 rows=[]
 for p in sorted(out.rglob('*')):
  if p.is_symlink():rows.append({'path':p.relative_to(out).as_posix(),'symlink':os.readlink(p)})
  elif p.is_file():rows.append({'path':p.relative_to(out).as_posix(),'sha256':sha(p)})
 receipt={'provider':'Debian sid cross target runtime subset; private extraction only','release_signature_primary':meta['release_signature_primary'],'packages_index_sha256':meta['packages_index_sha256'],
          'source_url':'https://deb.debian.org/debian/','packages':packages,'files':rows,'maintainer_scripts_executed':False,'global_install':False,
          'extraction':'regular/hardlink contents first, checked symlinks last; absolute links rewritten to relative provider-root equivalents'}
 (out/'provider.json').write_text(json.dumps(receipt,indent=2)+'\n');print(out/'provider.json')

if __name__=='__main__':main()
