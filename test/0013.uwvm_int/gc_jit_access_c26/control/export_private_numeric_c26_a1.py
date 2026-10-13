from pathlib import Path
import sys,json,shutil,tarfile,os
R=Path('/tmp/uwvm-gc-perf-20261011-r74');O=Path('/tmp/uwvm-gc-perf-20261011-r73');sys.path.insert(0,str(R/'control'));import native as N
N.check();D=R/'source/C26-NATIVE-BUNDLE-a1';D.mkdir(exist_ok=False);external={};missing=[]
def add(p,rel=None,required=True):
 p=Path(p)
 if not p.is_file():
  if required:missing.append(str(p))
  return
 if rel is None:rel=str(p.relative_to(R)) if p.is_relative_to(R) else 'r73/'+str(p.relative_to(O)) if p.is_relative_to(O) else 'external/'+p.name
 q=D/rel
 if q.exists():assert N.sha(q)==N.sha(p),(q,p);return
 q.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,q)
def tree(p):
 p=Path(p);assert p.is_dir(),p
 for x in sorted(p.rglob('*')):
  if x.is_file() and not x.name.endswith(('.o','.partial')):add(x)
def retained(p):
 p=Path(p);assert p.is_file(),p
 external[str(p)]={'durable_path':str(p.resolve()),'bytes':p.stat().st_size,'sha256':N.sha(p)}
main=['performance-c25-post-reboot-a1.json','candidate-validation-c25-post-reboot-a1.json','validation-private-numeric-c26-a3.json','publication-c26-a3-all-vms-a1.json','codegen-private-numeric-c26-a3-publication.json','codegen-private-numeric-c26-a3-wrapper-name-collision.json','inline-auth-fixed-c26-a2-final.json','gc-pressure-inline-auth-fixed-c26-a3-complete.json','inline-auth-clock-c26-a3.json','gc-pressure-inline-auth-clock-c26-a3-a1.json','hardware-private-numeric-c26-a3.json','hardware-private-numeric-c26-a4-independent-asm-audit.json','hardware-private-numeric-c26-a3-cycle-assembly-join.json','repo-sync-c26-a5-all-three-qualified.json','cache-reload-c26-a7.json','debug-inline-auth-three-repos-c26-a5.json','checkpoint-private-numeric-c26-a6.json','actual-readwrite-mca-c26-a3-campaign.json']
for n in main:add(R/'evidence'/n)
expected_nonzero=set()
for repo in ['uwvm2','uwvm2-stable','uwvm2-ros']:
 p=R/'build/repo-sync-c26-a5'/repo;add(p/'build-binding.json');add(p/'owned-intermediate-cleanup.json');retained(p/'uwvm');q=R/'evidence'/('repo-sync-c26-a5-'+repo+'-qualification.json');add(q);j=json.loads(q.read_text());assert j['passed']==521
 for ci,row in enumerate(j['rows']):
  if row['expected_trap']:expected_nonzero.add('repo-sync-c26-a5-'+repo+'-case-'+str(ci))
 add(R/'source/repo-sync-c26-current-a5'/repo/'current-source-binding.json');tree(R/'evidence'/('debug-inline-auth-three-repos-c26-a5-'+repo))
for n in ['scoped-c25-known-length-patches-a1.json','scoped-private-numeric-c26-patches-a2.json','scoped-private-numeric-c26-patches-a3.json','wasmtime-dynamic-library-binding-post-reboot-c25-a1.json','restore-'+N.BOOT+'.json','repo-sync-c26-a5-overlay-merged.json','c26-exact-fresh-product-files-a5.json','three-repository-c26-source-sync-a5.json','wasmtime-source-bound-analysis-c9.json']:
 add(R/'source'/n)
for n in ['typed-known-length-fixtures-c25-a1','private-numeric-publication-fixtures-c26-a3','typed-array-store-bench-c21','typed-store-gc-pressure-c21','hardware-specs-private-numeric-c26-a3','fixed-gc-pressure-inline-auth-c26-a3-complete','private-numeric-codegen-c26-a3','production-private-codegen-c26-a5']:tree(R/'source'/n)
for p in sorted((R/'profiles').glob('codegen-private-c26-a3-*')):tree(p)
hardware=json.loads((R/'evidence/hardware-private-numeric-c26-a3.json').read_text())
for row in hardware['rows']:
 Q=Path(row['qualified']).parent
 for p in sorted(Q.rglob('*')):
  if p.is_file():
   if p.stat().st_size<2<<20:add(p)
   else:retained(p)
 P=Q.parent
 for p in sorted(P.rglob('*')):
  if not p.is_file() or p.is_relative_to(Q):continue
  if p.suffix in ['.bin','.txt','.json','.stdout','.stderr','.log','.csv','.xml'] and p.stat().st_size<2<<20 and not any(x in p.parts for x in ['data.0','sqlite-db']):add(p)
  else:retained(p)
for name in ['inline-auth-fixed-c26-a2-final.json','gc-pressure-inline-auth-fixed-c26-a3-complete.json']:
 for row in json.loads((R/'evidence'/name).read_text())['rows']:tree(row['dir'])
for folder,binding in [('deferred-typed-array-access-known-length','build-binding-c25-a1.json'),('deferred-private-numeric-array-lock','build-binding-c26-a2.json'),('deferred-private-numeric-array-lock-numeric-struct','build-binding-c26-a3.json')]:
 p=R/'build'/folder;add(p/binding,required=False);retained(p/'uwvm')
# Include every build flag of the measured candidate and critical source, without full source archives.
private=R/'source/uwvm2-candidate-c26-a3'
fs=json.loads((R/'source/c26-exact-fresh-product-files-a5.json').read_text())['repos']['uwvm2']['qualified_files']
for f in fs:
 p=private/f
 if p.exists():add(p,'frozen-private-source-c26-a3/'+f)
for p in private.glob('*binding*.json'):add(p)
ledger=D/'evidence/native-receipts-and-output.jsonl';ledger.parent.mkdir(exist_ok=True);ledger_count=0;guards=[]
prefixes=['repo-sync-c26-a5-','cache-reload-c26-a7-','debug-inline-auth-three-repos-c26-a5','production-private-codegen-c26-a5-','production-private-codegen-c26-a6-','validation-private-numeric-c26-a3-','private-numeric-publication-c26-a3-','fixed-gc-pressure-inline-auth-c26-a3-complete-','inline-auth-clock-c26-a3-','gc-pressure-inline-auth-clock-c26-a3-','private-numeric-c26-a3-','actual-readwrite-mca-c26-a','checkpoint-private-numeric-c26-a','candidate-validation-c25','validation-c25-post','hardware-private-numeric-c26-a3-']
with ledger.open('w')as out:
 for p in sorted((R/'evidence').iterdir()):
  if not p.is_file() or not any(p.name.startswith(x)for x in prefixes) or p.name.endswith(('.heartbeat.json','.started.json','.preexec.json','.partial')) or p.suffix not in ['.json','.stdout','.stderr']:continue
  if p.stat().st_size>1<<20:retained(p);continue
  text=p.read_text(errors='replace');out.write(json.dumps(dict(path=str(p),sha256=N.sha(p),bytes=p.stat().st_size,text=text))+'\n');ledger_count+=1
  if p.suffix=='.json':
   try:j=json.loads(text)
   except ValueError:continue
   if isinstance(j,dict)and'argv'in j and'guard'in j:guards.append(j)
controls=['native.py','fixed_counter_worker_c25_a2.py','disjoint_pmu_c20_fixed_v2.py','profile_worker_known_length_c26_a3.py','validate_profile_a3.py','hardware_private_numeric_c26_a3.py','hardware_proof_private_numeric_c26_a4_live.py','analyze_private_numeric_cycles_c26_a3.py','fixed_private_numeric_c26_a3_complete.py','clock_private_numeric_c26_a3.py','clock_gc_pressure_private_numeric_c26_a4.py','test_private_numeric_fixtures_c26_a3.py','test_private_numeric_publication_c26_a3.py','build_test_three_repos_private_numeric_c26_a5.py','test_cache_reload_private_numeric_c26_a7.py','run_debug_three_repos_private_numeric_c26_a5.py','debug_typed_array_store_three_repos_c21_a1.py','gdb_production_private_codegen_c26_a6.py','mca_actual_readwrite_private_numeric_c26_a1.py','mca_actual_readwrite_private_numeric_c26_a2.py','mca_actual_readwrite_private_numeric_c26_a3.py','checkpoint_private_numeric_c26_a5.py']
for n in controls:add(R/'control'/n)
add(O/'control/enter.sh');add(O/'control/entry.py');add(N.BASE/'control'/('reboot-'+N.BOOT+'.json'),'control/reboot-receipt.json',required=False)
for p in [O/'build/wasmtime-bench',O/'source/wasmtime-capi/lib/libwasmtime.so',R/'build/deferred-typed-array-access-inline-static32/uwvm']:retained(p)
for p in sorted((R/'source').glob('*build-plan*c25*.json')):add(p)
add(R/'source/build-plan-typed-array-load-c20.json')
assert not missing,missing
N.save(D/'SSH-RETAINED-MANIFEST.json',dict(files=external,scope='Raw VTune PERF/SQL and exact native ELFs remain on disk-backed SSH paths, with SHA and resolved durable_path. Compact local bundle excludes their bulk.'))
fixed=json.loads((R/'evidence/gc-pressure-inline-auth-fixed-c26-a3-complete.json').read_text());fr=[]
for row in fixed['summary']:
 c=row['counts'];a=c['private-numeric-array-lock-numeric-struct'];w=c['wasmtime-drc'];fr.append(dict(case=row['case'],c26_cycles_per_Wasm_invocation=a['cycles'],wasmtime_cycles_per_Wasm_invocation=w['cycles'],c26_over_wasmtime=a['cycles']/w['cycles'],c26_over_c23=row['c26_over_c23']['cycles']))
report=dict(status='native-tested-experimental-profile',profile='typed-array-access-private',boot=N.BOOT,cgroup=N.CG,qualification=dict(three_repository_Wasm_cells=1563,per_repository=521,signed_cache_cells=84,debug_cells=36,frozen_private_checkpoint_cells=16,publication_assembly_cells=10,production_signed_cache_assembly_cells=6,hardware_profiles=22,final_fixed_counter_cells=54,long_clock_cells=198,pressure_clock_cells=36),fixed_cycles=fr,clock=json.loads((R/'evidence/inline-auth-clock-c26-a3.json').read_text())['summary'],measurement_boundary='Unscaled grouped target-user counters >=99.99% running, P-core2, READY after JIT+warmup. Fixed counters include tiny user protocol and VM transitions per complete Wasm invocation. Internal POSIX clocks delimit execution. No CPU idle gate.',algorithm='Whole original-opcode function proof denies publication/calls/EH/extern/unknown families anywhere, sealed only after finalization. Checked in-activation constructor full-token SSA/local facts only. Immutable logical length reused after retained authentication; precise roots, traps, barriers and checked fallback retained. No native object address cached across GC.',source_qualification='Frozen measured C26a3 private benchmark ELF with full flags is separate from three current seven-macro product builds. Concurrent checkpoint observer edits preserved; full3TU builds+Wasm+cache+actual RX positive/negative proof qualified. Performance applies only to its exact bound benchmark config, not directly to different product ELFs.',limits=['Reference-read clock ratio14.94 and reference-write10-12 remain; no allGC performance victory.','DRC vs tracing collector work differs; cycle collection parity not claimed.','New experimental profile restricted Linux x86_64 LLVM JIT; otherarch/QEMU C26 validation not performed.','Nonprecise cycles can skid; IP counts are locality evidence, not accurate individual instruction latency.','Checkpoint16 accepted cells use frozen C26a3 test API; not three current-repo checkpoint API tests. Two-participant rendezvous required Pcores6,8,10,12; singletonCPU6 failed2-positive episode coverage and is rejected.','MCA actual RX success trace uses Alder Lake P scheduling model; excludes collision/trap/allocator paths and does not model lock coherence/cache misses/branch flow.','Fixed specs inherited inner_operations_per_rep=1024 and array.len normalization_scope strings. These are stale for numeric fixtures and not used in any computation. per_rep is raw_count divided by same driver reps for same Wasm SHA: one complete Wasm invocation. Original receipts immutable.'],guards=dict(receipts=len(guards),max_observed_group_memory=max(x.get('peak_group_memory',0)for x in guards),guarded_withdrawals=[x['label']for x in guards if x.get('guard')],nonzero_expected_traps=[x['label']for x in guards if x.get('returncode')and x['label']in expected_nonzero],nonzero_other_receipts=[dict(label=x['label'],returncode=x['returncode'])for x in guards if x.get('returncode')and x['label']not in expected_nonzero]),post=N.check(),ledger_records=ledger_count)
N.save(D/'C26-NATIVE-REPORT.json',report)
readme='''本轮 C26 LLVM JIT 数值数组优化

新增 xmake 选项 `--gc-jit-access-profile=typed-array-access-private`（实验性，Linux x86_64 LLVM JIT），统一开启七项访问宏。已同步并原生构建测试 uwvm2、uwvm2-stable、uwvm2-ros；默认配置保持不变。

资格证明使用已经验证的原始 opcode/type，整个函数结束才封定。仅当前 activation 的检查后数组构造事实可消除数值访问锁；循环后的全局/表/引用字段发布、调用、尾调用、外部桥接、死分支发布也撤销整个函数资格。未知参数、debug/checkpoint、imports、lazy/tiered/OSR保留原检查。对象引用仍是 opaque token，不能当裸地址，没有跨GC保存地址。

P-core2 固定硬件计数器三轮中位数，每单位为同一Wasm的完整一次调用、排除JIT：

|测试|新版/旧版周期|新版/Wasmtime周期|
|---|---:|---:|
'''
for row in fr:readme+='|'+row['case']+'|'+format(row['c26_over_c23'],'.4f')+'|'+format(row['c26_over_wasmtime'],'.4f')+'|\n'
readme+='''
纯i32写入比Wasmtime少11.13%周期，读写混合仍多7.49%；GC压力i32/f32接近持平，i8/i16仍有差距。数据绑定冻结的private benchmark ELF及其完整编译宏。三个七宏普通产品构建被正确性、签名缓存和实际RX机器码验证，不冒充相同的测时ELF；查看build bindings区分配置。

22份硬件Hotspots/Microarchitecture全部零PERF丢样；独立解析事件类型cpu_core、目标TID/CPU2、RX字节与原始PERF。旧纯读写Lock Latency约62.5%，新为0。安全private RX无object flag XCHG，publication反例保留。三个普通产品的签名缓存命中RX均通过同样正/反例。周期采样可能skid，不把某IP样本比例作为单指令准确耗时。

汇编显示剩余UWVM逐操作token hash桶查找、owner/type/kind检查、root槽写入；Wasmtime更多使用类型化堆偏移与检查。实际RX success trace的llvm-mca：C26为52指令、11.5模型吞吐周期，Wasmtime为55指令、8.5周期。模型不能表达真实锁一致性、cache miss或分支执行，不代替PMU实测。[LLVM官方说明](https://llvm.org/docs/CommandGuide/llvm-mca.html)，[Intel硬件分析说明](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-1/general-exploration-analysis.html)。

本机Wasmtime HEAD 65abe37529a6bca836efcac1dcb920d5c7890580的crates/cranelift/src/func_environ/gc.rs用于源码分析；实测49.0.2共享库另有SHA/inode/映射绑定。不能称HEAD就是采样库commit。既有source-bound分析保留版本边界。

正确性：各仓库521项，共1563项，含两个期望校验拒绝和指定陷阱消息检查；84次新进程签名缓存miss/hit；36次debug读取/修改/类型范围拒绝；冻结C26a3的16项checkpoint capture/retire/continue和两参与者census。没有整个GC图checkpoint回滚或全架构测试已完成的结论。

未解决：reference-read内部计时14.94倍，reference-write10–12倍，DAG约2.07倍，exnref约1.29倍。数值数组胜项不代表整个GC胜出。DRC与追踪回收的循环垃圾处理和工作量不同。其余22类时钟结果及旧/新UWVM六项工作指标相等验证见报告。

失败保留并排除：MCA抽取器a1/a2进入cold分支，a3完整成功；checkpoint初版fixture API/路径及单CPU两参与者覆盖失败；先前codegen wrapper输出名冲突，child10项证明完整保留但不冒充正常外层receipt。fixed模板留下无关array.len单位文字，计算始终raw_count/reps；报告明确更正单位，原件不改写。

唯一64GiB cgroup、swap0、OOM0，维持32/40/48GiB准入及撤回门槛，无CPU空闲准入，无共享cache reclaim。原始PERF/SQL/ELF保留SSH磁盘并记录SHA；本地只带代码、fixtures、RX汇编、报告和紧凑receipt，控制文件占用。
'''
(D/'README.md').write_text(readme)
manifest={str(p.relative_to(D)):N.sha(p)for p in sorted(D.rglob('*'))if p.is_file()};total=sum(p.stat().st_size for p in D.rglob('*')if p.is_file());assert total<45<<20,total
N.save(D/'MANIFEST.json',dict(files=manifest,bytes=total,status='native-qualified-c26-frozen-perf-and-fresh3repo-correctness',remote_retained_manifest_sha=N.sha(D/'SSH-RETAINED-MANIFEST.json')))
archive=R/'source/C26-NATIVE-BUNDLE-a1.tar.gz';assert not archive.exists()
with tarfile.open(archive,'w:gz',compresslevel=6)as t:t.add(D,arcname='gc_jit_access_c26')
N.persist(archive);N.save(R/'source/C26-NATIVE-BUNDLE-a1-export.json',dict(archive=str(archive),sha256=N.sha(archive),bytes=archive.stat().st_size,uncompressed=total,files=len(manifest),manifest_sha=N.sha(D/'MANIFEST.json'),post=N.check()));print('C26_BUNDLE_EXPORTED',archive.stat().st_size,total,len(manifest),flush=True)
