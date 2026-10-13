from pathlib import Path
import sys,json,shutil,tarfile,base64,re,hashlib,os
R=Path('/tmp/uwvm-gc-perf-20261011-r74');P73=Path('/tmp/uwvm-gc-perf-20261011-r73')
sys.path.insert(0,str(R/'control'));import native as N
def file_check():
 assert Path('/proc/sys/kernel/random/boot_id').read_text().strip()==N.BOOT and Path('/proc/self/cgroup').read_text().strip()=='0::'+N.CG
 assert N.unique_groups()==[str(N.C)] and (N.C/'memory.max').read_text().strip()==str(64<<30) and (N.C/'memory.swap.max').read_text().strip()=='0'
 m=int((N.C/'memory.current').read_text());assert m<48<<30 and N.host_available()>8<<30 and shutil.disk_usage(N.BASE).free>25<<30
 return dict(boot=N.BOOT,cgroup=N.CG,memory=m,host_available=N.host_available(),unique64=N.unique_groups(),affinity=sorted(os.sched_getaffinity(0)),memory_events=(N.C/'memory.events').read_text(),scope='File-only export controller. No native tests/builds at this stage; original32/40GiB admission in native.py unchanged,48GiB withdrawal bound retained.')
file_check();OUT=N.BASE/'artifacts/C23-NATIVE-BUNDLE-a1';OUT.mkdir(exist_ok=True)
assert not (OUT/'MANIFEST.json').exists(), 'accepted bundle immutable'
def read(p):return json.loads(Path(p).read_text())
def put(rel,j):
 p=OUT/rel;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(j,ensure_ascii=False,indent=2)+'\n')
def copy(p,rel):
 p=Path(p);q=OUT/rel;q.parent.mkdir(parents=True,exist_ok=True)
 if not q.exists():shutil.copy2(p,q)
 assert N.sha(p)==N.sha(q),(p,q)
 fd=os.open(q,os.O_RDONLY);os.fsync(fd);os.posix_fadvise(fd,0,0,os.POSIX_FADV_DONTNEED);os.close(fd)
 file_check()
def tree(P,rel):
 for p in Path(P).rglob('*'):
  if p.is_file():copy(p,Path(rel)/p.relative_to(P))
repos=['uwvm2','uwvm2-stable','uwvm2-ros']
names=['inline-auth-correctness-c23-a3.json','inline-auth-new-fixtures-c23-a3.json','static-layout-new-fixtures-c23-a3.json','inline-auth-private-extra-c23-a3.json','foreign-typed-store-private-c23-a3.json','inline-auth-live-collection-c23-a3.json','repo-sync-c23-a1-all-three-qualified.json','repo-sync-c23-a1-foreign-isolated-features-qualification.json','foreign-typed-store-three-repos-c23-a1.json','production-live-gc-c23-a1.json','debug-inline-auth-three-repos-c23-a1.json','cache-reload-c23-a2.json','inline-auth-gdb-c23-a3.json','inline-auth-production-gdb-c23-a2.json','inline-auth-clock-c23-a3.json','gc-pressure-inline-auth-clock-c23-a3.json','inline-auth-fixed-c23-a4-final.json','gc-pressure-inline-auth-fixed-c23-a3.json','inline-auth-write-hardware-c23-a3.json','inline-auth-hardware-c23-a3.json','gc-pressure-inline-auth-hardware-c23-a3.json','three-production-inline-auth-clock-c23-a1.json']+['repo-sync-c23-a1-'+r+'-qualification.json' for r in repos]
agg={n:read(R/'evidence'/n) for n in names}
for n in names:copy(R/'evidence'/n,Path('evidence')/n)
counts={'private_main':379,'prior32_old_new_wasmtime':96,'new80_old_new_wasmtime':240,'private_foreign_features':36,'private_new_foreign_stores':12,'private_actual_gc_pairs':8,'production_main':1473,'production_foreign_features':108,'production_new_foreign_stores':36,'production_actual_gc':12,'signed_cache_miss_hit':24}
actual=[len(agg['inline-auth-correctness-c23-a3.json']['rows'])]+[agg[n]['cells'] for n in ['inline-auth-new-fixtures-c23-a3.json','static-layout-new-fixtures-c23-a3.json','inline-auth-private-extra-c23-a3.json','foreign-typed-store-private-c23-a3.json','inline-auth-live-collection-c23-a3.json']]+[sum(agg['repo-sync-c23-a1-'+r+'-qualification.json']['cases'] for r in repos)]+[agg[n]['cells'] for n in ['repo-sync-c23-a1-foreign-isolated-features-qualification.json','foreign-typed-store-three-repos-c23-a1.json','production-live-gc-c23-a1.json','cache-reload-c23-a2.json']]
assert actual==list(counts.values()) and sum(actual)==2424,actual
for r in repos:assert agg['repo-sync-c23-a1-'+r+'-qualification.json']['passed']==491
for j in agg.values():
 for row in j.get('rows',[]):
  if 'pass_' in row:assert row['pass_']
  if 'pass' in row:assert row['pass']
receipt_names=set();receipt_shas=set();fixture_paths=set();fixture_hashes=set()
def refs(x):
 if isinstance(x,dict):
  for k,v in x.items():
   if k in ['receipt','assembly_receipt','label','profile'] and isinstance(v,str) and (R/'evidence'/(v+'.json')).exists():receipt_names.add(v)
   elif k=='receipt_sha' and isinstance(v,str):receipt_shas.add(v)
   elif k.lower()=='wasm_sha' and isinstance(v,str):fixture_hashes.add(v)
   else:refs(v)
 elif isinstance(x,list):
  for v in x:refs(v)
 elif isinstance(x,str) and x.startswith('/tmp/uwvm-gc-perf-') and x.endswith(('.wasm','.wat')) and Path(x).is_file():fixture_paths.add(Path(x))
for j in agg.values():refs(j)
scope=read(R/'source/production-wasi-clock-gc-policy-scope-c23-a1.json');refs(scope);copy(R/'source/production-wasi-clock-gc-policy-scope-c23-a1.json','resources/production-wasi-clock-gc-policy-scope-c23-a1.json')
D=R/'source/typed-array-inline-auth-codegen-c23-a3';hardware=[];raw=[]
for n in ['hardware-assembly-proof-write-c23-a3.json','hardware-assembly-proof-c23-a3.json','hardware-assembly-proof-gc-pressure-c23-a3.json']:
 j=read(D/n);assert len(j['rows'])==6;refs(j)
 for row in j['rows']:
  assert row['zero_loss'] and row['atom_target_samples']==row['software_target_samples']==0;P=R/'profiles'/row['profile'];q=read(P/'validation-a3/qualified.json');assert q['hardware_runtime_qualified'] and q['native_ROI_ns']>=500000000 and q['target_samples']==row['core_target_samples'] and q['cgroup']==N.CG and q['boot']==N.BOOT
  pf=P/'vtune/data.0/system-wide.perf';dst=N.BASE/'recovery'/pf.relative_to(R);assert N.sha(pf)==q['raw_sha']==N.sha(dst)
  raw.append(dict(profile=P.name,tmp=str(pf),persistent=str(dst),sha256=q['raw_sha'],bytes=pf.stat().st_size))
  hardware.append(row)
  for f in P.rglob('*'):
   if f.is_file() and f.relative_to(P).parts[0]!='vtune':
    copy(f,Path('profiles')/P.name/f.relative_to(P))
    if f.suffix=='.json':refs(read(f))
  for i,a in enumerate(row['asm']):
   for ending in ['-obj-','-asm-']:receipt_names.add(row['profile']+ending+str(i))
for p in D.rglob('*'):
 if p.is_file():
  copy(p,Path('codegen')/p.relative_to(D))
  if p.suffix=='.json':refs(read(p))
assert len(hardware)==18
put('bindings/vtune-raw-disk-manifest.json',raw)
for n in ['inline-auth-fixed-c23-a4-final.json','gc-pressure-inline-auth-fixed-c23-a3.json']:
 for row in agg[n]['rows']:
  P=Path(row['dir']);tree(P,Path('profiles')/P.name)
debug_cells=0
for P in [R/'evidence/debug-static-layout-private-c23-a3']+[R/'evidence'/('debug-inline-auth-three-repos-c23-a1-'+r) for r in repos]:
 j=read(P/'result.json');assert all(x['pass_'] for x in j['results']);debug_cells+=j['cells'];receipt_names.add(P.name);refs(j);tree(P,Path('debug')/P.name)
assert debug_cells==48
for name in ['hardware-specs-inline-auth-write-c23-a3','hardware-specs-inline-auth-c23-a3','hardware-specs-gc-pressure-inline-auth-c23-a3','inline-auth-gdb-c23-a3','inline-auth-production-gdb-c23-a2','static-layout-preview-c23-a3']:
 tree(R/'source'/name,Path('specs')/name)
 for p in (R/'source'/name).rglob('*.json'):refs(read(p))
scoped=read(R/'source/proposed-current-scoped-c23-a3.json');production={}
for repo in repos:
 S=R/'source/repo-sync-c23-current'/repo;B=R/'build/repo-sync-c23-a1'/repo;b=read(B/'build-binding.json');assert N.sha(S/'current-source-binding.json')==b['input_sha']==b['verified_source_id'] and len(b['optimization_flags'])==6 and N.sha(B/'uwvm')==b['ELF_sha']
 production[repo]=dict(ELF_sha=b['ELF_sha'],input_sha=b['input_sha'],profile_flags=b['optimization_flags'],scoped_source=scoped[repo]);copy(S/'current-source-binding.json',Path('bindings')/repo/'source.json');copy(B/'build-binding.json',Path('bindings')/repo/'build.json');copy(B/'owned-intermediate-cleanup.json',Path('resources')/repo/'owned-intermediate-cleanup.json')
 for f,h in scoped[repo].items():assert N.sha(S/f)==h;copy(S/f,Path('bindings')/repo/'scoped-source'/f)
for tag,src,build,sourcefile,buildfile,plan in [
 ('private-c22','uwvm2-candidate-c22','deferred-typed-array-access-inline','source-binding-c22.json','build-binding-c22.json','build-plan-typed-array-access-inline-c22.json'),
 ('private-c23','uwvm2-candidate-c23-a3','deferred-typed-array-access-inline-static32','source-binding-c23-a3.json','build-binding-c23-a3.json','build-plan-typed-array-access-inline-static32-c23-a3.json')]:
 copy(R/'source'/src/sourcefile,Path('bindings')/tag/'source.json');copy(R/'build'/build/buildfile,Path('bindings')/tag/'build.json');copy(R/'source'/plan,Path('bindings')/tag/'plan.json')
for p in (R/'evidence').glob('*.json'):
 if p.name.endswith(('.heartbeat.json','.preexec.json','.started.json')):continue
 h=N.sha(p)
 if h in receipt_shas:receipt_names.add(p.stem)
 elif (p.name.startswith(('repo-sync-c23-','build-static-array-layout-c23-')) and any(t in p.stem for t in ['compile','link'])):
  j=read(p)
  if 'argv' in j and j['returncode']==0 and j['guard'] is None and j['guard_error'] is None:receipt_names.add(p.stem)
receipt_names.update(['typed-array-access-compile-c23-a3-'+str(i) for i in range(3)]+['typed-array-access-link-c23-a3'])
receipts=[];logs=[];found=set()
for name in sorted(receipt_names):
 p=R/'evidence'/(name+'.json');j=read(p);assert 'argv' in j and j['guard'] is None and j['guard_error'] is None and j['pre']['cgroup']==N.CG and j['pre']['boot']==N.BOOT,(name,j);h=N.sha(p);found.add(h);receipts.append(dict(file=str(p),sha256=h,receipt=j));refs(j['argv'])
 for suffix in ['stdout','stderr']:
  f=p.with_suffix('.'+suffix);assert N.sha(f)==j[suffix+'_sha'];logs.append(dict(file=str(f),sha256=N.sha(f),base64=base64.b64encode(f.read_bytes()).decode()))
assert receipt_shas<=found,receipt_shas-found
for n,rows in [('accepted-native-receipts.jsonl',receipts),('accepted-native-logs.jsonl',logs)]:
 (OUT/'evidence'/n).write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in rows))
canonical=[P73/'source/cases-restored-a1.json']+[R/'source'/s for s in ['array-edge-mutations-native-a1.json','gc-scalar-graph-native-a1.json','gc-raw-nan-native-a1.json','repeated-array-types-native-a1.json','observable-features-native-a1.json','gc-phase-feedback-native-c1.json','uniform-reference-mutations-native-c3.json','local-catch-reference-fixtures-native-c6.json']]
for p in canonical:refs(read(p));copy(p,Path('fixtures/canonical-manifests')/p.name)
for name in ['static-layout-fixtures-c23-a2','inline-array-auth-fixtures-c22-a1','typed-array-store-c21-a2','typed-array-boundaries-c20','array-length-boundaries-c20','typed-array-store-bench-c21','typed-store-gc-pressure-c21','typed-store-live-collection-c21-a2','typed-store-integrated-clock-c21','typed-array-store-debug-c21','foreign-typed-store-c21','repo-sync-c20-fixtures-export-v2']:
 tree(R/'source'/name,Path('fixtures')/name)
 for p in (R/'source'/name).rglob('*.json'):refs(read(p))
 for p in (R/'source'/name).rglob('*'):
  if p.is_file() and p.suffix in ['.wat','.wasm']:fixture_paths.add(p)
fixtures=[]
for p in sorted(fixture_paths):
 assert p.parts[1]=='tmp' and p.parts[2].startswith('uwvm-gc-perf-'),p
 root=Path('/tmp')/p.parts[2]
 rel=Path('fixtures/remote-source')/root.name/p.relative_to(root);copy(p,rel);fixtures.append(dict(remote=str(p),bundle=str(rel),sha256=N.sha(p)))
 other=p.with_suffix('.wat')
 if p.suffix=='.wasm' and other.is_file():copy(other,Path('fixtures/remote-source')/root.name/other.relative_to(root))
covered={x['sha256'] for x in fixtures if x['remote'].endswith('.wasm')};assert fixture_hashes<=covered,fixture_hashes-covered
put('fixture-manifest.json',fixtures)
for p in (R/'control').glob('*.py'):
 if 'c23' in p.name or p.name in ['native.py','fixed_counter_worker_c20_v2.py','disjoint_pmu_c20_fixed_v2.py','disjoint_pmu_read_lock_c20_v1.py','validate_profile_a3.py','debug_typed_array_store_c21_a1.py','debug_typed_array_store_three_repos_c21_a1.py']:copy(p,Path('controls')/p.name)
for name in ['enter.sh','entry.py']:copy(N.BASE.parent/'uwvm-gc-perf-20261011-r73/control'/name,Path('controls/entry')/name)
for p in (R/'source').glob('c23-owned-*.json'):copy(p,Path('resources')/p.name)
for name in ['c23-final-resource-audit-a1.json','c23-own-unaccepted-export-stage-disk-migration-a1.json','static-layout-c23-shared-cache-reclaim-proposal-a1.json','proposed-current-scoped-c23-a3.json','resume-static32-tu0-c23-a3.json','inline-auth-native-symbols-c23-a3.json','local-wasmtime-design-source-c23-a1.json']:
 copy(R/'source'/name,Path('resources' if 'wasmtime' not in name else 'bindings/wasmtime-design')/name)
copy(R/'build/deferred-typed-array-access-inline-static32/owned-intermediate-cleanup-c23-a3.json','resources/private-owned-intermediate-cleanup.json')
WT=R/'source/wasmtime-codegen-analysis-a1'
for name in ['source49-binding-a2.json','binding-a2.json','source-witnesses49-a1.json']:copy(WT/name,Path('bindings/wasmtime49')/name)
copy(R/'source/wasmtime-dynamic-library-binding-c20.json','bindings/wasmtime49/dynamic-library.json')
p=WT/'bytecodealliance-wasmtime-3c8a3e7/crates/cranelift/src/func_environ/gc.rs';ls=p.read_text().splitlines();put('bindings/wasmtime49/typed-array-source-excerpts.json',dict(file=str(p),sha=N.sha(p),commit='3c8a3e79aa3188a1a12b3aebbaa097abccceac92',excerpts=[dict(first_line=a,text='\n'.join(ls[a-1:b])) for a,b in [(901,928),(945,1048),(1076,1102)]]))
archives=[]
for p in [R/'source/uwvm2-candidate-c23-a3-bound-source.tar.gz',R/'source/repo-proposed-current-source-c23-a3.tar.gz']:
 dst=N.BASE/'recovery'/p.relative_to(R);assert N.sha(p)==N.sha(dst);archives.append(dict(tmp=str(p),persistent=str(dst),sha=N.sha(p),bytes=p.stat().st_size))
rejected=[]
for name in ['inline-auth-clock-c23-a1.json','gc-pressure-inline-auth-clock-c23-a1.json','inline-auth-fixed-c23-a5.json']:
 p=R/'evidence'/name;copy(p,Path('rejected/all-width-prototype')/name);rejected.append(dict(file=str(p),sha=N.sha(p),reason='Superseded all-width static layout. i8/i16 read-write regressed despite lower instructions; final specializes only32bit.'))
tree(R/'source/static-layout-preview-c23-a2','rejected/all-width-prototype/actual-RX')
for p in (R/'evidence').glob('*.json'):
 if p.name.endswith(('.heartbeat.json','.preexec.json','.started.json')):continue
 if p.name.startswith(('repo-sync-c23-','build-static-array-layout-c23-')) and any(t in p.stem for t in ['compile','link']):
  j=read(p)
  if 'argv' in j and (j['guard'] is not None or j['returncode']!=0):
   copy(p,Path('rejected/resource-or-build-attempts')/p.name)
   for suffix in ['stdout','stderr']:
    f=p.with_suffix('.'+suffix)
    if f.exists():copy(f,Path('rejected/resource-or-build-attempts')/f.name)
   rejected.append(dict(file=str(p),sha=N.sha(p),reason=j['guard'] or 'rejected build attempt'))
old='typed-array-access-inline';new='typed-array-access-inline-static32'
elves={old:read(OUT/'bindings/private-c22/build.json')['ELF_sha'],new:read(OUT/'bindings/private-c23/build.json')['ELF_sha']}
for n in ['inline-auth-clock-c23-a3.json','gc-pressure-inline-auth-clock-c23-a3.json']:
 for x in agg[n]['results']:
  if x['variant'] in elves:assert x['ELF_sha']==elves[x['variant']]
report=dict(revision='C23-static32',claim='Opt-in authenticated local packed32bit numeric array LLVM IR specialization; Wasmtime GC parity remains incomplete.',environment=file_check(),memory_max=int((N.C/'memory.max').read_text()),memory_swap_max=int((N.C/'memory.swap.max').read_text()),production=production,correctness_execution_cells=counts,correctness_total=sum(actual),private_correctness_total=sum(actual[:6]),production_correctness_total=sum(actual[6:]),debug_restarts=48,gdb_actual_path_cells=7,extra_policy_audit_cells=3,private_clock_cells=agg['inline-auth-clock-c23-a3.json']['cells']+agg['gc-pressure-inline-auth-clock-c23-a3.json']['cells'],production_access_clock_cells=96,fixed_counter_cells=52,qualified_hardware_profiles=18,accepted_receipts_exported=len(receipts),wasm_fixture_hashes_bound=len(covered),access_clock=agg['inline-auth-clock-c23-a3.json']['summary'],access_fixed=agg['inline-auth-fixed-c23-a4-final.json']['summary'],gc_pressure_clock=agg['gc-pressure-inline-auth-clock-c23-a3.json']['summary'],gc_pressure_fixed=agg['gc-pressure-inline-auth-fixed-c23-a3.json']['summary'],production_access_clock=agg['three-production-inline-auth-clock-c23-a1.json']['summary'],hardware=[{k:v for k,v in x.items() if k!='hotspots'} for x in hardware],mca=read(D/'static32-mca-a2/result.json'),archives=archives,rejected_attempts=rejected,new_shared_reclaim_executed=False,coverage_limits=['2424 are execution cells, not2424 independent Wasm modules. Expected traps and2invalid validation tests included in491 perrepo.','20 representative isolated features; not full formal Wasm3 conformance or complete perfeature performance matrix.','No concurrent-GC/checkpoint restoration/QEMU/crossarch performance qualification this revision.','Experimental Linux x86_64 LLVM/int128 profile; all3TUs must use same6macros. Default and noninline profiles unchanged.','Mutable locks, bounded membership authentication, legacy/subtype/foreign representations retained.','Reference read still14.78x Wasmtime clock here; no equivalence with DRC cyclic-collection capability.','Actual production WASI timing fixtures gate5(imports) disable automaticGC. Separate actualGC fixtures prove positive collections and reclamation.','Private33flag frozen experiment and production6flag current-source clocks are different protocols; do not combine them as before/after ratios.','VTune profiles have different calibrated reps. Hardware fixed counters compare equal workloads/reps without scaling. MCA selected paths omit coherence/cache stalls.','Some inherited scope strings/job print labels mentionC21; actual validated binary SHA, variant, recipe and summary fields bind this matrix toC22 versus C23-static32. Build binding qualification-pending text predates later accepted receipts.'])
assert report['private_correctness_total']==771 and report['production_correctness_total']==1653 and report['private_clock_cells']==234
assert sum(agg[n]['cells'] for n in ['inline-auth-fixed-c23-a4-final.json','gc-pressure-inline-auth-fixed-c23-a3.json'])==52
put('C23-NATIVE-REPORT.json',report)
rows=['| Wasm | C22 ms/start | C23 ms/start | Wasmtime ms/start | C23/C22 时间 | C23/C22 cycles | C23/C22 instructions |','| --- | ---: | ---: | ---: | ---: | ---: | ---: |']
for x in report['access_clock']:
 if x['case'] in [ty+'-'+op for ty in ['i8','i16','i32','f32'] for op in ['write','write-read']]:
  n=x['ns'];f=next((f for f in report['access_fixed'] if f['case']==x['case']),None);a=f['c23_over_c22'] if f else {};cy=format(a['cycles'],'.4f') if 'cycles' in a else '未采集';it=format(a['instructions'],'.4f') if 'instructions' in a else '未采集'
  rows.append(f"| {x['case']} | {n[old]/1e6:.6f} | {n[new]/1e6:.6f} | {n['wasmtime-drc']/1e6:.6f} | {x['c23_over_c22']:.4f} | {cy} | {it} |")
pr=['| GC 压力 | C23/C22 时间 | C23/Wasmtime 时间 | C23/C22 cycles | C23/C22 instructions |','| --- | ---: | ---: | ---: | ---: |']
for x in report['gc_pressure_clock']:
 f=next(f for f in report['gc_pressure_fixed'] if f['case']==x['case'])['c23_over_c22'];pr.append(f"| {x['case']} | {x['c23_over_c22']:.4f} | {x['c23_over_wasmtime']:.4f} | {f['cycles']:.4f} | {f['instructions']:.4f} |")
prefix=(R/'source/c23-readme-prefix.txt').read_text();suffix=(R/'source/c23-readme-suffix.txt').read_text()
(OUT/'README.md').write_text(prefix+'\n\n'+'\n'.join(rows)+'\n\n'+'\n'.join(pr)+'\n\n'+suffix)
manifest={str(p.relative_to(OUT)):N.sha(p) for p in sorted(OUT.rglob('*')) if p.is_file()};put('MANIFEST.json',dict(files=manifest,file_count=len(manifest),bytes=sum((OUT/p).stat().st_size for p in manifest)))
archive=R/'source/C23-NATIVE-BUNDLE-a1.tar.gz';assert not archive.exists()
with tarfile.open(archive,'w:gz') as t:t.add(OUT,arcname='gc_jit_access_c23')
N.persist(archive);assert N.sha(archive)==N.sha(N.BASE/'recovery'/archive.relative_to(R));N.save(R/'source/C23-NATIVE-BUNDLE-a1-export-receipt.json',dict(archive=str(archive),sha=N.sha(archive),bytes=archive.stat().st_size,manifest_sha=N.sha(OUT/'MANIFEST.json'),files=len(manifest),correctness_cells=sum(actual),debug_restarts=48,post=file_check()));print('C23_EXPORT_OK',N.sha(archive),archive.stat().st_size,len(manifest),len(receipts),flush=True)
