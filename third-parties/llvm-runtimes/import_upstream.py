#!/usr/bin/env python3
"""Import the pinned LLVM runtime source subset, without building or executing it."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import posixpath
import tarfile
import tempfile

SHA256='ebe9be46fe8756d58c5b198ffad0fa2a766257add81a4dc52179bfacc7888ee6'
COMMIT='6dfe1677ab8dffbc6ec13d53a1e0215d75147689'
ARCHIVE_ROOT='llvm-project-23.1.1.src/'
DEST=Path(__file__).resolve().parent
ROOTS=('cmake/','runtimes/cmake/','llvm/cmake/','libcxx/cmake/Modules/',
       'libcxxabi/cmake/Modules/','libunwind/cmake/Modules/',
       'libcxx/include/','libcxx/src/','libcxx/modules/', 'libcxx/lib/',
       'libcxxabi/include/','libcxxabi/src/','libcxxabi/lib/',
       'libunwind/include/','libunwind/src/',
       'libc/shared/','libc/src/__support/','libc/hdr/','libc/include/')
FILES={'LICENSE.TXT','runtimes/CMakeLists.txt','libcxx/CMakeLists.txt','libcxxabi/CMakeLists.txt',
       'libunwind/CMakeLists.txt','libcxx/cmake/config-ix.cmake','libcxxabi/cmake/config-ix.cmake',
       'libunwind/cmake/config-ix.cmake','libcxx/LICENSE.TXT','libcxxabi/LICENSE.TXT',
       'libunwind/LICENSE.TXT','libc/LICENSE.TXT','libcxx/utils/CMakeLists.txt',
       'libcxx/utils/generate_iwyu_mapping.py','libcxx/utils/libcxx/__init__.py',
       'libcxx/utils/libcxx/header_information.py','libcxx/utils/zos_rename_dll_side_deck.sh',
       'libcxx/utils/merge_archives.py','llvm/utils/merge-json.py',
       'llvm/utils/extract_symbols.py','runtimes/Components.cmake.in',
       'libcxx/vendor/llvm/default_assertion_handler.in'}
BANNED={'.git','.github','test','tests','fuzz','docs','doc','benchmarks','benchmark','examples'}

def require(ok,message):
    if not ok:raise RuntimeError(message)

def digest(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def selected(rel):
    path=PurePosixPath(rel)
    require(not path.is_absolute() and '..' not in path.parts,'Unsafe upstream archive path')
    if any(part in BANNED for part in path.parts):return False
    if path.name in ('.gitignore','.gitattributes') or path.suffix in ('.md','.rst'):return False
    if path.name.lower().startswith(('readme','changelog')) and rel!='libcxx/src/ryu/README.txt':return False
    if rel=='libcxxabi/src/demangle/cp-to-llvm.sh':return False
    if rel.startswith('libcxx/lib/abi/'):return False
    return rel in FILES or any(rel.startswith(root) for root in ROOTS)

def atomic(path,data,mode=0o644):
    path.parent.mkdir(parents=True,exist_ok=True)
    fd,temp=tempfile.mkstemp(dir=path.parent,prefix='.upstream-')
    try:
        with os.fdopen(fd,'wb') as stream:stream.write(data);stream.flush();os.fsync(stream.fileno())
        os.chmod(temp,mode);os.replace(temp,path)
    finally:
        if os.path.exists(temp):os.unlink(temp)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('archive',type=Path);args=parser.parse_args()
    require(digest(args.archive)==SHA256,'Official source archive SHA differs')
    require(not (DEST/'upstream').exists(),'Import never overwrites an existing source subset')
    llvm=DEST.parent/'llvm';original={str(p):digest(p) for p in (llvm/'README.md',llvm/'sources.sha256')}
    records={};data={};links={}
    with tarfile.open(args.archive,'r|xz') as archive:
        for member in archive:
            if member.name==ARCHIVE_ROOT.rstrip('/'):
                require(member.isdir(),'Unexpected root source kind');continue
            require(member.name.startswith(ARCHIVE_ROOT),'Unexpected upstream archive root')
            rel=member.name[len(ARCHIVE_ROOT):]
            if not rel or not selected(rel) or member.isdir():continue
            require(rel not in records,'Duplicate selected source path')
            require(member.isfile() or member.issym() or member.islnk(),'Unsupported selected source kind')
            records[rel]={'archive_path':member.name,'upstream_mode':member.mode,'materialized_link':False}
            if member.isfile():
                stream=archive.extractfile(member);require(stream is not None,'Missing upstream source')
                value=stream.read();require(len(value)==member.size,'Incomplete upstream source');data[rel]=value
            else:
                target=posixpath.normpath(posixpath.join(posixpath.dirname(rel),member.linkname)) if member.issym() else member.linkname.removeprefix(ARCHIVE_ROOT)
                require(selected(target),'Selected symlink target is outside the retained subset')
                links[rel]=target;records[rel].update(materialized_link=True,upstream_link_target=member.linkname)
    while links:
        finished=[rel for rel,target in links.items() if target in data]
        require(finished,'Missing or cyclic selected symlink target')
        for rel in finished:data[rel]=data[links.pop(rel)]
    for rel in ('runtimes/CMakeLists.txt','libcxx/CMakeLists.txt','libcxxabi/CMakeLists.txt','libunwind/CMakeLists.txt',
                'cmake/Modules/LLVMVersion.cmake','llvm/cmake/modules/AddLLVM.cmake','LICENSE.TXT',
                'libcxx/vendor/llvm/default_assertion_handler.in'):
        require(rel in data,'Missing required source '+rel)
    stage=Path(tempfile.mkdtemp(prefix='.llvm-runtimes-upstream-',dir=DEST))
    for rel,value in sorted(data.items()):
        mode=0o755 if records[rel]['upstream_mode']&0o111 else 0o644
        atomic(stage/rel,value,mode)
        records[rel].update(bytes=len(value),sha256=hashlib.sha256(value).hexdigest())
    require(all(digest(p)==v for p,v in original.items()),'Existing ROS LLVM source manifests changed')
    os.chmod(stage,0o755)
    os.replace(stage,DEST/'upstream')
    atomic(DEST/'LICENSE.TXT',data['LICENSE.TXT'])
    manifest={'schema':'uwvm-ros-llvm-runtimes-source-v1','version':'23.1.1','tag':'llvmorg-23.1.1','commit':COMMIT,
      'source_url':'https://github.com/llvm/llvm-project/releases/download/llvmorg-23.1.1/llvm-project-23.1.1.src.tar.xz',
      'archive':{'bytes':args.archive.stat().st_size,'sha256':SHA256},'upstream_files':records,
      'file_count':len(records),'source_bytes':sum(len(v) for v in data.values()),'existing_llvm_before_after_sha256':original,
      'omitted':'Other LLVM projects, all upstream test/docs/fuzz/benchmark/example trees, Git metadata and checkout repositories',
      'native_compiled':False,'native_executed':False}
    atomic(DEST/'upstream-manifest.json',(json.dumps(manifest,indent=2,sort_keys=True)+'\n').encode())
    atomic(DEST/'sources.sha256',''.join(v['sha256']+'  upstream/'+rel+'\n' for rel,v in sorted(records.items())).encode())
    print(json.dumps(dict(files=len(records),bytes=manifest['source_bytes'],archive_sha256=SHA256,native_executed=False)))

if __name__=='__main__':main()
