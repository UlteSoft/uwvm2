from pathlib import Path
import json,hashlib,os
os.sched_setaffinity(0,{15})
o=Path('/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1/root-language-go-sdk-20261019-a3');rows=[]
for a in json.loads((o/'producer-final-result.json').read_bytes())['artifacts']:
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
result={'status':'ACTUAL_BINARY_CUSTOM_SECTION_INVENTORY','scope':'Inert final Wasm parsing only; producer Debug flags do not imply usable source debugging','products':rows}
(o/'custom-section-inventory.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps([(Path(r['path']).parent.name,Path(r['path']).name,r['custom_sections'])for r in rows]))
