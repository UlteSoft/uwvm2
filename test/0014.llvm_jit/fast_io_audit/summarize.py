#!/usr/bin/env python3
"""Summarize paired benchmark logs without treating small differences as proven wins."""
import argparse,json,statistics
from collections import defaultdict
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('results',type=Path);p.add_argument('--timing',type=Path);p.add_argument('--out',type=Path,required=True)
a=p.parse_args();samples=defaultdict(lambda:defaultdict(list));checksums=defaultdict(lambda:defaultdict(list))
for file in sorted((a.timing or a.results).glob('benchmark-*.log')):
 version=file.stem.rsplit('-',1)[1]
 for line in file.read_text().splitlines():
  if line.startswith('BENCH '):
   _,name,time,checksum=line.split();samples[name][version].append(float(time));checksums[name][version].append(checksum)
rows=[]
for name,data in samples.items():
 assert len(data['before'])==len(data['after'])==27
 assert checksums[name]['before']==checksums[name]['after']
 before,after=(statistics.median(data[x]) for x in ['before','after'])
 rows.append(dict(name=name,before_ns=before,after_ns=after,speedup=before/after,
  elapsed_reduction_percent=100*(1-after/before),samples_ns=data))
 print(f'{name}: {before:.3f} -> {after:.3f} ns; ratio {before/after:.3f}x')
profiles=['o3','sanitize','no-exceptions','s390x-linux-gnu','i686-linux-gnu','aarch64-linux-gnu']
for profile in profiles:
 assert (a.results/(profile+'-before-run.log')).read_bytes()==(a.results/(profile+'-after-run.log')).read_bytes()
report=dict(cpu='Intel Core i9-14900HX, E-core CPU 16',compiler='Clang 23, libc++, -O3',
 timing='CLOCK_THREAD_CPUTIME_ID; wall times retained in raw logs',
 caveat='Shared host. Native-LZ decode samples are bimodal even for the identical baseline binary; no stable codec-wide speedup is claimed.',
 memory_limit_bytes=int((a.results/'memory.max').read_text()),memory_peak_bytes=int((a.results/'memory.peak').read_text()),
 cpuset=(a.results/'cpuset.cpus.effective').read_text().strip(),samples_per_variant=27,
 profiles=profiles,sources=json.loads((a.results/'sources.json').read_text()),results=rows)
a.out.write_text(json.dumps(report,indent=2)+'\n')
