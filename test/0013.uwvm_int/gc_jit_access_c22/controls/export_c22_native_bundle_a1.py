from pathlib import Path
import sys,json,shutil,tarfile,base64
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
N.check();OUT=R/'source/C22-NATIVE-BUNDLE-a1';OUT.mkdir(exist_ok=False)
def read(p):return json.loads(Path(p).read_text())
def put(rel,j):
 p=OUT/rel;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(j,ensure_ascii=False,indent=2)+'\n')
def copy(p,rel):
 p=Path(p);q=OUT/rel;q.parent.mkdir(parents=True,exist_ok=True);assert not q.exists(),q;shutil.copy2(p,q);assert N.sha(p)==N.sha(q)
names=['inline-auth-correctness-c22-a1.json','inline-auth-new-fixtures-c22-a1.json','inline-auth-private-extra-c22-a1.json','foreign-typed-store-private-c22-a1.json','inline-auth-live-collection-c22-a1.json','repo-sync-c22-a1-all-three-qualified.json','repo-sync-c22-a1-foreign-isolated-features-qualification.json','foreign-typed-store-three-repos-c22-a1.json','production-live-gc-c22-a1.json','debug-inline-auth-three-repos-c22-a1.json','cache-reload-c22-a2.json','inline-auth-gdb-c22-a1.json','inline-auth-production-gdb-c22-a2.json','inline-auth-clock-c22-a1.json','gc-pressure-inline-auth-clock-c22-a1.json','inline-auth-fixed-c22-a4.json','gc-pressure-inline-auth-fixed-c22-a2.json','inline-auth-hardware-c22-a1.json','gc-pressure-inline-auth-hardware-c22-a1.json','three-production-inline-auth-clock-c22-a1.json']+['repo-sync-c22-a1-'+r+'-qualification.json' for r in ['uwvm2','uwvm2-stable','uwvm2-ros']]
agg={n:read(R/'evidence'/n) for n in names}
for n in names:copy(R/'evidence'/n,Path('evidence')/n)
counts={'private_main':379,'new32_old_new_wasmtime':96,'private_foreign_features':36,'private_new_foreign_stores':12,'private_actual_gc_pairs':8,'production_main':1233,'production_foreign_features':108,'production_new_foreign_stores':36,'production_actual_gc':12,'signed_cache_miss_hit':24}
actual=[len(agg['inline-auth-correctness-c22-a1.json']['rows'])]+[agg[n]['cells'] for n in ['inline-auth-new-fixtures-c22-a1.json','inline-auth-private-extra-c22-a1.json','foreign-typed-store-private-c22-a1.json','inline-auth-live-collection-c22-a1.json']]+[sum(agg['repo-sync-c22-a1-'+r+'-qualification.json']['cases'] for r in ['uwvm2','uwvm2-stable','uwvm2-ros'])]+[agg[n]['cells'] for n in ['repo-sync-c22-a1-foreign-isolated-features-qualification.json','foreign-typed-store-three-repos-c22-a1.json','production-live-gc-c22-a1.json','cache-reload-c22-a2.json']]
assert actual==list(counts.values()) and sum(actual)==1944,actual
for r in ['uwvm2','uwvm2-stable','uwvm2-ros']:assert agg['repo-sync-c22-a1-'+r+'-qualification.json']['passed']==411
for j in agg.values():
 for row in j.get('rows',[]):
  if 'pass_' in row:assert row['pass_']
  if 'pass' in row:assert row['pass']
receipt_names=set();receipt_shas=set()
def refs(x):
 if isinstance(x,dict):
  for k,v in x.items():
   if k in ['receipt','assembly_receipt','label','profile'] and isinstance(v,str) and (R/'evidence'/(v+'.json')).exists():receipt_names.add(v)
   elif k=='receipt_sha' and isinstance(v,str):receipt_shas.add(v)
   else:refs(v)
 elif isinstance(x,list):
  for v in x:refs(v)
for j in agg.values():refs(j)
scope=read(R/'source/production-wasi-clock-gc-policy-scope-c22-a1.json');refs(scope);copy(R/'source/production-wasi-clock-gc-policy-scope-c22-a1.json','resources/production-wasi-clock-gc-policy-scope-c22-a1.json')
D=R/'source/typed-array-inline-auth-codegen-c22';hardware=[]
for n in ['hardware-assembly-proof-c22-a1.json','hardware-assembly-proof-gc-pressure-c22-a1.json']:
 j=read(D/n);assert len(j['rows'])==6;refs(j)
 for row in j['rows']:
  assert row['zero_loss'] and row['atom_target_samples']==row['software_target_samples']==0;P=R/'profiles'/row['profile'];q=read(P/'validation-a3/qualified.json');assert q['hardware_runtime_qualified'] and q['native_ROI_ns']>=500000000 and q['target_samples']==row['core_target_samples'];hardware.append(row)
  for f in P.rglob('*'):
   if f.is_file() and f.relative_to(P).parts[0]!='vtune':copy(f,Path('profiles')/P.name/f.relative_to(P))
for p in D.rglob('*'):
 if p.is_file():
  copy(p,Path('codegen')/p.relative_to(D))
  if p.suffix=='.json':refs(read(p))
assert len(hardware)==12
for n in ['inline-auth-fixed-c22-a4.json','gc-pressure-inline-auth-fixed-c22-a2.json']:
 for row in agg[n]['rows']:
  P=Path(row['dir'])
  for p in P.rglob('*'):
   if p.is_file():copy(p,Path('profiles')/P.name/p.relative_to(P))
debug_cells=0
for P in [R/'evidence/debug-inline-auth-private-c22-a1']+[R/'evidence'/('debug-inline-auth-three-repos-c22-a1-'+r) for r in ['uwvm2','uwvm2-stable','uwvm2-ros']]:
 j=read(P/'result.json');assert all(x['pass_'] for x in j['results']);debug_cells+=j['cells'];receipt_names.add(P.name)
 for f in P.rglob('*'):
  if f.is_file():copy(f,Path('debug')/P.name/f.relative_to(P))
assert debug_cells==48
for p in (R/'evidence').glob('*.json'):
 if N.sha(p) in receipt_shas:receipt_names.add(p.stem)
receipts=[];logs=[];found=set()
for name in sorted(receipt_names):
 p=R/'evidence'/(name+'.json');j=read(p);assert 'argv' in j and j['guard'] is None and j['guard_error'] is None and j['pre']['cgroup']==N.CG and j['pre']['boot']==N.BOOT,(name,j);h=N.sha(p);found.add(h);receipts.append(dict(file=str(p),sha256=h,receipt=j))
 for suffix in ['stdout','stderr']:
  f=p.with_suffix('.'+suffix);assert N.sha(f)==j[suffix+'_sha'];logs.append(dict(file=str(f),sha256=N.sha(f),base64=base64.b64encode(f.read_bytes()).decode()))
assert receipt_shas<=found
for n,rows in [('accepted-native-receipts.jsonl',receipts),('accepted-native-logs.jsonl',logs)]:
 (OUT/'evidence'/n).write_text(''.join(json.dumps(x,ensure_ascii=False)+'\n' for x in rows))
rejected=[]
for name in ['fixed-inline-auth-c22-a2-r0-i8-write-wasmtime-drc','fixed-inline-auth-c22-a3-r0-i8-write-typed-array-access','inline-auth-production-gdb-c22-a1-uwvm2']:
 p=R/'evidence'/(name+'.json');rejected.append(dict(name=name,receipt_sha=N.sha(p)));copy(p,Path('rejected')/p.name)
 for suf in ['stdout','stderr']:copy(p.with_suffix('.'+suf),Path('rejected')/(name+'.'+suf))
fixtures=[]
for name in ['inline-array-auth-fixtures-c22-a1','typed-array-store-bench-c21','typed-store-gc-pressure-c21','typed-store-live-collection-c21-a2','typed-store-integrated-clock-c21','foreign-typed-store-c21','repo-sync-c20-fixtures-export-v2']:
 for p in (R/'source'/name).rglob('*'):
  if p.is_file():
   rel=Path('fixtures')/name/p.relative_to(R/'source'/name);copy(p,rel)
   if p.suffix in ['.wat','.wasm']:fixtures.append(dict(remote=str(p),bundle=str(rel),sha256=N.sha(p)))
put('fixture-manifest.json',fixtures)
for p in (R/'control').glob('*.py'):
 if 'c22' in p.name or p.name in ['native.py','fixed_counter_worker_c20_v2.py','disjoint_pmu_c20_fixed_v2.py','disjoint_pmu_read_lock_c20_v1.py','validate_profile_a3.py','debug_typed_array_store_c21_a1.py','debug_typed_array_store_three_repos_c21_a1.py']:copy(p,Path('controls')/p.name)
for name in ['enter.sh','entry.py']:copy(Path('/home/macromodel/Documents/uwvm-gc-perf-20261011-r73/control')/name,Path('controls/entry')/name)
for name in ['hardware-specs-inline-auth-c22-a1','hardware-specs-gc-pressure-inline-auth-c22-a1','inline-auth-gdb-c22-a1','inline-auth-production-gdb-c22-a2']:
 for p in (R/'source'/name).rglob('*'):
  if p.is_file():copy(p,Path('specs')/name/p.relative_to(R/'source'/name))
scoped=read(R/'source/current-scoped-source-c22-a1.json');production={}
for repo in ['uwvm2','uwvm2-stable','uwvm2-ros']:
 S=R/'source/repo-sync-c22-current'/repo;B=R/'build/repo-sync-c22-a1'/repo;b=read(B/'build-binding.json');assert N.sha(S/'current-source-binding.json')==b['input_sha']==b['verified_source_id'] and len(b['optimization_flags'])==6 and N.sha(B/'uwvm')==b['ELF_sha'];production[repo]=dict(ELF_sha=b['ELF_sha'],input_sha=b['input_sha'],profile_flags=b['optimization_flags'],scoped_source=scoped[repo]);copy(S/'current-source-binding.json',Path('bindings')/repo/'source.json');copy(B/'build-binding.json',Path('bindings')/repo/'build.json');copy(B/'owned-intermediate-cleanup.json',Path('resources')/repo/'owned-intermediate-cleanup.json')
 for f,h in scoped[repo].items():assert N.sha(S/f)==h;copy(S/f,Path('bindings')/repo/'scoped-source'/f)
for repo,profile in [('private-c21','typed-array-access'),('private-c22','typed-array-access-inline')]:
 rev='c21' if repo.endswith('c21') else 'c22';copy(R/'source'/('uwvm2-candidate-'+rev)/('source-binding-'+rev+'.json'),Path('bindings')/repo/'source.json');copy(R/'build'/('deferred-'+profile)/('build-binding-'+rev+'.json'),Path('bindings')/repo/'build.json');copy(R/'source'/('build-plan-'+profile+'-'+rev+'.json'),Path('bindings')/repo/'plan.json')
for name in ['typed-store-c21-cache-reclaim-proposal-a1.json','c22-authorized-root-cache-reclaim-a1.json','c22-authorized-root-cache-reclaim-a1-intent.json','c22-reclaim-before-open-failure-clarification-a1.json','current-scoped-source-c22-a1.json']:copy(R/'source'/name,Path('resources')/name)
copy(R/'build/deferred-typed-array-access-inline/owned-intermediate-cleanup-c22.json','resources/private-owned-intermediate-cleanup.json')
WT=R/'source/wasmtime-codegen-analysis-a1'
for name in ['source49-binding-a2.json','binding-a2.json','source-witnesses49-a1.json']:copy(WT/name,Path('bindings/wasmtime49')/name)
p=WT/'bytecodealliance-wasmtime-3c8a3e7/crates/cranelift/src/func_environ/gc.rs';ls=p.read_text().splitlines();put('bindings/wasmtime49/typed-array-source-excerpts.json',dict(file=str(p),sha=N.sha(p),commit='3c8a3e79aa3188a1a12b3aebbaa097abccceac92',excerpts=[dict(first_line=a,text='\n'.join(ls[a-1:b])) for a,b in [(901,928),(945,1048),(1076,1102)]]))
archives=[]
for p in [R/'source/uwvm2-candidate-c22-bound-source.tar.gz',R/'source/repo-current-source-c22.tar.gz']+[R/'source'/('repo-sync-c22-'+r+'-bound-source.tar.gz') for r in production]:
 dst=N.BASE/'recovery'/p.relative_to(R);assert N.sha(p)==N.sha(dst);archives.append(dict(tmp=str(p),persistent=str(dst),sha=N.sha(p),bytes=p.stat().st_size))
report=dict(revision='C22',claim='Opt-in LLVM IR inline local numeric-array authentication; incomplete GC parity.',environment=N.check(),memory_max=int((N.C/'memory.max').read_text()),memory_swap_max=int((N.C/'memory.swap.max').read_text()),production=production,correctness_execution_cells=counts,correctness_total=sum(actual),debug_restarts=48,gdb_actual_path_cells=7,extra_policy_audit_cells=3,private_clock_cells=234,production_access_clock_cells=96,fixed_counter_cells=44,qualified_hardware_profiles=12,accepted_receipts_exported=len(receipts),access_clock=agg['inline-auth-clock-c22-a1.json']['summary'],access_fixed=agg['inline-auth-fixed-c22-a4.json']['summary'],gc_pressure_clock=agg['gc-pressure-inline-auth-clock-c22-a1.json']['summary'],gc_pressure_fixed=agg['gc-pressure-inline-auth-fixed-c22-a2.json']['summary'],production_access_clock=agg['three-production-inline-auth-clock-c22-a1.json']['summary'],hardware=[{k:v for k,v in x.items() if k!='hotspots'} for x in hardware],archives=archives,rejected_attempts=rejected,shared_reclaim_authorization_consumed=True,coverage_limits=['20 representative isolated features, not full formal Wasm3 conformance or complete performance matrix.','No parallelGC/checkpoint restoration/QEMU/crossarch qualification this revision.','Experimental x86_64Linux Clang/int128 only, every TU six macros; default unchanged.','Mutable locks and legacy/subtype/foreign schema remain. Deep depth8 fallback incurs an extra prefix walk.','Reference read still about14.5x Wasmtime here; no DRC cycle-equivalence claim.','Actual GC eligibility gate5(imports) andgate9(exception population) unchanged.','Private33experiment frozen before production6macro cache source-identity change.','Sampled totals with different calibrated reps are not speed ratios; fixed same-repetition unscaled counters supply ratios.','MCA static selected paths omit real coherence/stalls, not whole-VM performance.'])
put('C22-NATIVE-REPORT.json',report)
rows=['| Wasm | C21 ms/start | C22 ms/start | Wasmtime ms/start | C22/C21 | C22/Wasmtime |','| --- | ---: | ---: | ---: | ---: | ---: |']
for x in report['access_clock']:
 if x['case'] in [ty+'-'+op for ty in ['i8','i16','i32','f32'] for op in ['write','write-read']]:
  n=x['ns'];rows.append(f"| {x['case']} | {n['typed-array-access']/1e6:.6f} | {n['typed-array-access-inline']/1e6:.6f} | {n['wasmtime-drc']/1e6:.6f} | {x['c22_over_c21']:.4f} | {x['c22_over_wasmtime']:.4f} |")
pr=['| GC 压力 | C22/C21 时间 | C22/Wasmtime 时间 | C22/C21 cycles | C22/C21 instructions |','| --- | ---: | ---: | ---: | ---: |']
for x in report['gc_pressure_clock']:
 f=next(f for f in report['gc_pressure_fixed'] if f['case']==x['case'])['c22_over_c21'];pr.append(f"| {x['case']} | {x['c22_over_c21']:.4f} | {x['c22_over_wasmtime']:.4f} | {f['cycles']:.4f} | {f['instructions']:.4f} |")
text=(R/'source/c22-final-readme-prefix.txt').read_text()+'\n\n'+'\n'.join(rows)+'\n\n'+(R/'source/c22-final-readme-middle.txt').read_text()+'\n\n'+'\n'.join(pr)+'\n\n'+(R/'source/c22-final-readme-suffix.txt').read_text()
(OUT/'README.md').write_text(text)
manifest={str(p.relative_to(OUT)):N.sha(p) for p in sorted(OUT.rglob('*')) if p.is_file()};put('MANIFEST.json',dict(files=manifest,file_count=len(manifest),bytes=sum((OUT/p).stat().st_size for p in manifest)))
archive=R/'source/C22-NATIVE-BUNDLE-a1.tar.gz';assert not archive.exists()
with tarfile.open(archive,'w:gz') as t:t.add(OUT,arcname='gc_jit_access_c22')
N.persist(archive);assert N.sha(archive)==N.sha(N.BASE/'recovery'/archive.relative_to(R));N.save(R/'source/C22-NATIVE-BUNDLE-a1-export-receipt.json',dict(archive=str(archive),sha=N.sha(archive),bytes=archive.stat().st_size,manifest_sha=N.sha(OUT/'MANIFEST.json'),files=len(manifest),correctness_cells=sum(actual),debug_restarts=48,post=N.check()));print('C22_EXPORT_OK',N.sha(archive),archive.stat().st_size,len(manifest),flush=True)
