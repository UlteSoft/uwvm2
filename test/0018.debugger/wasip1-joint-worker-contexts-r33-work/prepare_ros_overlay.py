from pathlib import Path
import json,hashlib,tarfile,resource,subprocess,shlex,io
L=Path(__file__).parent;B=Path('/Users/liyinan/Documents/MacroModel/src');E='/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17';D=E+'/rounds/wasip1-joint-worker-contexts-20261007-r33'
M=json.loads((L/'inputs.json').read_text());n='uwvm2-ros/src/uwvm2/runtime/lib/uwvm_runtime_checkpoint_observation_calls.h';p=B/n;data=p.read_bytes();old=(B/'uwvm2'/n.split('/',1)[1]).read_bytes();assert data==old.replace(b'get_llvm_jit_generated_wasm_bridge_entry_depth()',b'get_llvm_jit_generated_bridge_scope_depth()')
h=hashlib.sha256(data).hexdigest();assert h!=M[n];M[n]=h;P=L/'ros-repaired-inputs.json';assert not P.exists();P.write_text(json.dumps(M,indent=2)+'\n');A=L/'ros-source-delta.tar.gz';assert not A.exists()
with tarfile.open(A,'w:gz',compresslevel=6) as t:t.add(p,arcname=n,recursive=False)
with tarfile.open(A,'r:gz') as t:
 members=t.getmembers();assert len(members)==1 and members[0].name==n and hashlib.sha256(t.extractfile(members[0]).read()).hexdigest()==h
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest();q=dict(passed=True,base_manifest_sha256=sha(L/'inputs.json'),overlay_manifest_sha256=sha(P),delta_archive_sha256=sha(A),changed_files={n:h},reason='ROS full-only runtime has the existing generated bridge scope depth getter; standard runtime has the generated Wasm entry depth getter',memory_upper_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss+(128<<20),limit_bytes=2<<30)
assert q['memory_upper_bytes']<2<<30;(L/'ros-source-delta-qualified.json').write_text(json.dumps(q,indent=2)+'\n')
(L/'ros_overlay_bootstrap.py').write_text('''from pathlib import Path,PurePosixPath
import json,hashlib,os,tarfile
D=Path(__file__).parent;E=D.parent.parent;Q=json.loads((D/'ros-source-delta-qualified.json').read_text());sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
assert Q['passed'] and sha(D/'inputs.json')==Q['base_manifest_sha256'] and sha(D/'ros-repaired-inputs.json')==Q['overlay_manifest_sha256'] and sha(D/'ros-source-delta.tar.gz')==Q['delta_archive_sha256']
M=json.loads((D/'ros-repaired-inputs.json').read_text());base=json.loads((D/'inputs.json').read_text());assert {n:h for n,h in M.items() if base[n]!=h}==Q['changed_files']
S=D/'ros-repaired-inputs';assert not S.exists();S.mkdir();changed={}
with tarfile.open(D/'ros-source-delta.tar.gz','r|gz') as t:
 for m in t:
  n=PurePosixPath(m.name);assert m.isfile() and not n.is_absolute() and '..' not in n.parts and m.name in Q['changed_files']
  with t.extractfile(m) as f:data=f.read()
  assert hashlib.sha256(data).hexdigest()==M[m.name];p=S/m.name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data);p.chmod(0o444);changed[m.name]=M[m.name]
assert changed==Q['changed_files'];linked=0
for n,h in M.items():
 if not n.startswith('uwvm2-ros/') or n in changed:continue
 p=S/n;p.parent.mkdir(parents=True,exist_ok=True);original=D/'inputs'/n;assert original.is_file() and not original.is_symlink() and sha(original)==h;os.link(original,p);linked+=1
assert all(sha(S/n)==h for n,h in M.items() if n.startswith('uwvm2-ros/'))
old=E/'guard-joint-linux-integrated-r33-uwvm2-ros.json';g=json.loads(old.read_text());assert not g['passed'] and g['actual_root_exit']==1
O=D/'products/linux-integrated/uwvm2-ros';failed=D/'products/linux-compile-failed/uwvm2-ros';failed.parent.mkdir();assert not failed.exists();O.rename(failed)
(D/'ros-first-compile-failed-guard.json').write_bytes(old.read_bytes())
proof=dict(passed=True,base_manifest_sha256=sha(D/'inputs.json'),overlay_manifest_sha256=sha(D/'ros-repaired-inputs.json'),delta_archive_sha256=sha(D/'ros-source-delta.tar.gz'),all_payloads_read_back=True,changed_files=changed,hardlinked_immutable_ros_files=linked,old_inputs_untouched=True,failed_original_guard_sha256=sha(old),failed_products_directory=str(failed),cgroup=Path('/proc/self/cgroup').read_text())
(D/'ros-input-freeze-qualified.json').write_text(json.dumps(proof,indent=2)+'\\n');print('ROS one-file immutable repair overlay qualified',linked,flush=True)
''')
g=(L/'guard-joint-inputs-r33.py').read_text().replace('joint-inputs-r33','joint-ros-overlay-inputs-r33');(L/'guard-joint-ros-overlay-inputs-r33.py').write_text(g)
ssh=['ssh','-i','/Users/liyinan/.ssh/id_ed25519','macromodel@100.123.133.75'];scp=['scp','-i','/Users/liyinan/.ssh/id_ed25519'];subprocess.run([*scp,str(P),str(A),str(L/'ros-source-delta-qualified.json'),str(L/'ros_overlay_bootstrap.py'),'macromodel@100.123.133.75:'+D+'/'],check=True);subprocess.run([*scp,str(L/'guard-joint-ros-overlay-inputs-r33.py'),'macromodel@100.123.133.75:'+E+'/'],check=True)
code='''from pathlib import Path
import os
E=Path('''+repr(E)+''');p=E/'suite.py';s=p.read_text();a="if sys.argv[1]=='joint-source-duplicate-retire-r33':";assert s.count(a)==1
(E/'rounds/wasip1-joint-worker-contexts-20261007-r33/suite-before-ros-overlay.py').write_text(s)
n=s.replace(a,"if sys.argv[1]=='joint-ros-overlay-inputs-r33':\\n import runpy\\n runpy.run_path(str(D/'rounds/wasip1-joint-worker-contexts-20261007-r33/ros_overlay_bootstrap.py'),run_name='__main__')\\nel"+a);compile(n,str(p),'exec');tmp=E/'suite-ros-overlay-r33.tmp';assert not tmp.exists();tmp.write_text(n);tmp.chmod(p.stat().st_mode);assert p.read_text()==s;os.replace(tmp,p)
''';subprocess.run([*ssh,'python3 -c '+shlex.quote(code)],check=True);print('ROS repaired manifest',q['overlay_manifest_sha256'],flush=True)
