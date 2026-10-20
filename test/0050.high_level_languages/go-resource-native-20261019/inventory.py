from pathlib import Path
import json,hashlib,os
os.sched_setaffinity(0,{15})
o=Path('/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1/root-language-go-resource-native-20261019-a4');rows=[]
for a in json.loads((o/'go-producer-result.json').read_bytes())['artifacts']:
 b=Path(a['path']).read_bytes();assert b[:8]==b'\0asm\x01\0\0\0';pos=8
 def leb():
  global pos
  value=0
  for i in range(5):
   v=b[pos];pos+=1;value|=(v&127)<<(7*i)
   if v<128:
    assert value<1<<32;return value
  raise ValueError('u32 LEB bound')
 names=[]
 while pos<len(b):
  sid=b[pos];pos+=1;n=leb();end=pos+n;assert end<=len(b)
  if sid==0:
   length=leb();assert pos+length<=end;names.append(b[pos:pos+length].decode('utf-8'))
  pos=end
 rows.append({**a,'custom_sections':names,'debug_info_present':'.debug_info'in names})
old=json.loads((o.parent/'root-language-go-sdk-20261019-a3/producer-final-result.json').read_bytes())['artifacts']
comparison=[]
for row in rows:
 prior=next(a for a in old if a['backend']==row['backend']and a['profile']==row['profile']and a['path'].endswith('/resources.wasm'))
 assert hashlib.sha256(Path(prior['path']).read_bytes()).hexdigest()==prior['sha256']
 comparison.append({'backend':row['backend'],'profile':row['profile'],'old_bytes':prior['bytes'],'new_bytes':row['bytes'],'saved_bytes':prior['bytes']-row['bytes'],'old_sha256':prior['sha256'],'new_sha256':row['sha256']})
(o/'size-comparison.json').write_text(json.dumps({'scope':'Actual raw Wasm byte counts, not performance or whole-VM size','products':comparison},indent=2)+'\n')
result={'status':'ACTUAL_BINARY_CUSTOM_SECTION_INVENTORY','scope':'Inert final Wasm parsing only; producer Debug flags do not imply usable source debugging','products':rows}
(o/'custom-section-inventory.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps([(Path(r['path']).parent.name,Path(r['path']).name,r['custom_sections'])for r in rows]))
