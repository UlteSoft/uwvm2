#!/usr/bin/env python3
"""Cold blocks are excluded; stop after the successful indirect musttail jump."""
import subprocess,re,json,hashlib
from pathlib import Path
import argparse
parser=argparse.ArgumentParser(description='Compare actual x86 interpreter memory-handler success paths with a qualified baseline.')
parser.add_argument('baseline',type=Path);parser.add_argument('current',type=Path);parser.add_argument('output',type=Path)
args=parser.parse_args();out=args.output;out.mkdir(parents=True,exist_ok=True)
paths=[args.baseline,args.current]
def select(path):
 text=subprocess.check_output(['objdump','-d','--no-show-raw-insn',str(path)],text=True)
 parts=re.split(r'(?m)^([0-9a-f]+) <([^>]+)>:\n',text);result={}
 for i in range(1,len(parts),3):
  symbol,body=parts[i+1:i+3]
  if '7optable' not in symbol:continue
  family='size' if '19uwvmint_memory_sizeI' in symbol else 'atomic' if any(t in symbol for t in ('11atomic_loadI','12atomic_storeI','10atomic_rmwI')) else 'plain' if 'memop' in symbol and any(t in symbol for t in ('load','store')) and 'trap' not in symbol else None
  if not family:continue
  lines=[]
  for line in body.splitlines():
   m=re.match(r'\s*[0-9a-f]+:\s*(\S+)\s*(.*)',line)
   if m:
    lines.append(m[1]+' '+m[2].split('#')[0].strip())
    if m[1].startswith('jmp') and m[2].startswith('*'):break
  result[symbol]=(family,lines)
 return result
before,after=map(select,paths); rows=[];counts={}
for symbol,(family,body) in after.items():
 old=before.get(symbol); same=old is not None and old[1]==body
 rows.append({'symbol':symbol,'family':family,'same':same,'before':None if old is None else old[1],'after':body})
 counts.setdefault(family,{'total':0,'same':0});counts[family]['total']+=1;counts[family]['same']+=same
report={'inputs':{str(p):hashlib.file_digest(p.open('rb'),'sha256').hexdigest() for p in paths},'counts':counts,'rows':rows}
(out/'interpreter-memory-handler-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(counts))
for row in rows:
 if not row['same']:
  print('DIFF',row['family'],row['symbol'][:100]);print('BEFORE',row['before']);print('AFTER',row['after']);break

assert len(rows)>1000 and all(r['same'] for r in rows), counts
print('PASS: all selected successful handler paths are instruction-identical and end in an indirect tail jump')
