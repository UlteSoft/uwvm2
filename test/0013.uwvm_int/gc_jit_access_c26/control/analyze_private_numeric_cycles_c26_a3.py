from pathlib import Path
import sys,json,collections,re,csv,bisect
R=Path('/tmp/uwvm-gc-perf-20261011-r74');sys.path.insert(0,str(R/'control'));import native as N
orig=R/'control/validate_profile_a3.py';source=orig.read_text();assert source.count("j={'raw_sha'")==1
prefix=source.split("j={'raw_sha'",1)[0]
assert prefix.count('A.mkdir(exist_ok=False)')==1;prefix=prefix.replace('A.mkdir(exist_ok=False)','assert A.exists()')
old='target=zero=jit=0;times=[]';new='target=zero=jit=0;times=[];cycle_ips=collections.Counter()';assert prefix.count(old)==1;prefix=prefix.replace(old,new)
old='events[str(event_id)]+=1;times.append(t)';new="events[str(event_id)]+=1;times.append(t)\n if ids[event_id]['type']==int(pathlib.Path('/sys/bus/event_source/devices/cpu_core/type').read_text()) and ids[event_id]['config']==0x3c:cycle_ips[ip]+=1";assert prefix.count(old)==1;prefix=prefix.replace(old,new)
proof=R/'source/private-numeric-codegen-c26-a3/hardware-assembly-proof-c26-a3.json';rows=[]
with N.lease():
 for x in json.loads(proof.read_text())['rows']:
  P=R/'profiles'/x['profile'];raw=json.loads((P/'validation-a3/raw-record-validation.json').read_text());assert raw['parser_sha']==N.sha(orig);oldargv=sys.argv;sys.argv=[str(orig),str(P)];g={'__file__':str(orig),'__name__':'raw_cycle_replay'};exec(compile(prefix,str(orig),'exec'),g);sys.argv=oldargv
  assert g['target']==raw['target_roi_samples'] and g['total']==raw['total_sample_records'] and N.sha(g['f'])==raw['raw_sha'] and not g['pending']
  ips=g['cycle_ips'];instructions=[]
  for a in x['asm']:
   assert N.sha(a['file'])==a['sha'];lines=Path(a['file']).read_text().splitlines()
   for l in lines:
    m=re.match(r'^([0-9a-f]+):\s+(.*)$',l)
    if m:instructions.append((int(m[1],16),m[2].strip(),a['lo']))
  instructions.sort();addresses=[a for a,_,_ in instructions]
  joined=[]
  for ip,n in ips.most_common(20):
   if not any(r['lo']<=ip<r['hi'] for r in g['cap']['rx']):continue
   i=bisect.bisect_right(addresses,ip)-1;assert i>=0;a,ins,lo=instructions[i];joined.append(dict(ip=hex(ip),offset=hex(ip-lo),samples=n,instruction=ins,instruction_start=hex(a),exact_instruction_start=ip==a))
  locks=[]
  for a,ins,lo in instructions:
   if re.match(r'^xchg\b',ins):locks.append(dict(offset=hex(a-lo),instruction=ins,exact_cycle_sample_ips=ips[a],cycle_samples_within8bytes=sum(n for ip,n in ips.items() if a-8<=ip<a+12)))
  ms=list(csv.DictReader((P/'validation-a3/summary.csv').open()));fields={}
  for m in ms:
   if m['Metric Name'] in ['CPI Rate','Back-End Bound','Retiring','Lock Latency','L1 Latency Dependency','Average CPU Frequency'] and m['Metric Name'] not in fields:fields[m['Metric Name']]=m['Metric Value']
  rows.append(dict(case=x['case'],variant=x['variant'],analysis=x['analysis'],profile=x['profile'],cycle_samples=sum(ips.values()),jit_cycle_samples=sum(n for ip,n in ips.items() if any(r['lo']<=ip<r['hi'] for r in g['cap']['rx'])),top_cycle_instructions=joined,lock_sites=locks,vtune_summary_metrics=fields,source_raw_sha=raw['raw_sha'],source_assembly=[a['sha'] for a in x['asm']]));print('CYCLE_ASSEMBLY_JOIN',x['case'],x['variant'],x['analysis'],joined[:3],flush=True)
 N.save(R/'source/private-numeric-codegen-c26-a3/cycle-assembly-analysis-c26-a3.json',dict(rows=rows,worker_sha=N.sha(__file__),original_parser_sha=N.sha(orig),replay_edits=['reuseexistingvalidationdirectory','extra per-core-cycle-eventIP counter only'],scope='Exact core-cycle event IDs only; same userPID/TID/core2/clockROI filter independently replayed. IPs joined to actualRX disassembly. Non-precise cycles samples can skid; neighborhood counts are not causal perinstructioncycle attribution. Dense1ms sampling still does not make percategory percentages an exact causal speedup estimate. Fixed unscaled counters provide comparison.'))
