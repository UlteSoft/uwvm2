from pathlib import Path
import sys,os,shutil,json
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
names=['repo-sync-c20','repo-sync-c20-v2','repo-sync-c21','uwvm2-candidate-c17','uwvm2-candidate-c18','uwvm2-candidate-c19'];cmds=[]
for p in Path('/proc').iterdir():
 try:cmds.append((p/'cmdline').read_bytes())
 except OSError:pass
before=int((N.C/'memory.current').read_text());rows=[]
for name in names:
 p=R/'source'/name;assert p.is_dir() and not p.is_symlink();assert not any((str(p)+'/').encode() in c for c in cmds),name
 d=N.BASE/'cold-source-c21'/name;assert not d.exists();d.parent.mkdir(exist_ok=True);files={str(f.relative_to(p)):N.sha(f) for f in p.rglob('*') if f.is_file()};size=sum(f.stat().st_size for f in p.rglob('*') if f.is_file());shutil.copytree(p,d,symlinks=True)
 for f,h in files.items():assert N.sha(d/f)==h
 retired=p.with_name(p.name+'.owned-retired-c21');assert not retired.exists();p.rename(retired);p.symlink_to(d,target_is_directory=True)
 for f,h in files.items():assert N.sha(p/f)==h
 shutil.rmtree(retired)
 for f in d.rglob('*'):
  if not f.is_file() or f.is_symlink():continue
  fd=os.open(f,os.O_RDONLY)
  try:os.posix_fadvise(fd,0,0,os.POSIX_FADV_DONTNEED)
  finally:os.close(fd)
 rows.append(dict(source=str(p),persisted=str(d),files=files,bytes=size));print('OWN_COLD_SOURCE_MIGRATED',name,len(files),size,flush=True)
N.save(R/'source/owned-source-migration-c21-a1.json',dict(rows=rows,pre_memory=before,post_memory=int((N.C/'memory.current').read_text()),scope='Only six owned completed/superseded immutable source snapshots, no live compile commands reference them. Complete file hashes verified in persistent copy before removing tmpfs originals; original paths retained by symlink. Current production snapshots and C20/C21 benchmark sources untouched; no shared cache reclaim.'))
