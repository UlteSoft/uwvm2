from pathlib import Path
import traceback
import os,sys,resource,subprocess,time,json,secrets,shutil,hashlib,socket,base64,re,array,stat,fcntl
D=Path(__file__).parent; B=Path('/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924');Q=Path('/home/macromodel/.local/opt/qemu-10.2.1/usr')
A=json.loads((D.parent.parent.parent/'wasip1-active-environment.json').read_text())
component_only=False
migration_only=False
platform=sys.argv[2]; repo=sys.argv[3]; assert sys.argv[1]=='joint-vm-r32' and platform in ('windows','freebsd') and repo in ('uwvm2','uwvm2-ros')
nonce=secrets.token_hex(16); R=D/('vm-'+platform+'-joint-preparation-'+nonce[:8]); R.mkdir(mode=0o700); IPC=Path('/tmp/uwp-'+nonce[:12]);IPC.mkdir(mode=0o700)
env=dict(os.environ);env['LD_LIBRARY_PATH']=str(Q/'lib/x86_64-linux-gnu')+':'+str(Q/'lib')+':'+env.get('LD_LIBRARY_PATH','')
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def stamp(p):
 s=p.stat();return dict(dev=s.st_dev,ino=s.st_ino,bytes=s.st_size,mtime=s.st_mtime_ns,ctime=s.st_ctime_ns,mode=s.st_mode)
def run(argv):subprocess.run(list(map(str,argv)),check=True,env=env,timeout=180)
isofs=R/'isofs';isofs.mkdir();digests={};expected=[];tests=[];compiled_inputs={};source_hashes={}
def ship(src,name):
 src=Path(src);dest=isofs/name
 # The immutable own input is hardlinked, avoiding duplicate tmpfs payloads.
 os.link(src,dest);digests[name]=sha(src)
repos=[repo];retire_duplicates=[]
O=D/'products'/platform/repo
proof=json.loads((O/'qualified.json').read_text());assert proof['passed'] and proof['platform']==platform and proof['repo']==repo and proof['source_manifest_sha256']==sha(D/'inputs.json')
assert sha(O/'fixture.exe')==proof['binary_sha256'] and all(sha(k)==h for k,h in proof['dependencies'].items())
for label in ('runtime','host-api'):
 qualified=O/(label+'-qualified.json');q=json.loads(qualified.read_text());assert all(sha(k)==h for k,h in q['dependencies'].items());pin=R/(repo+'-'+label+'-qualified.json');shutil.copy2(qualified,pin);compiled_inputs[str(pin)]=sha(pin)
pin=R/(repo+'-fixture-qualified.json');shutil.copy2(O/'qualified.json',pin);compiled_inputs[str(pin)]=sha(pin)
for name in ('fixture.exe','checkpoint.wasm','aliases.wasm'):
 if name!='fixture.exe':assert sha(O/name)==proof['wasm'][name.split('.')[0]]
 ship(O/name,repo+'-'+name)
for fixture in ('checkpoint','aliases'):
 for policy in ('instruction','unwind'):
  tests.append(dict(label=repo+'-'+fixture+'-'+policy,binary=repo+'-fixture.exe',args=['FILE:'+repo+'-'+fixture+'.wasm',policy,'DIR:'+repo+'-'+fixture+'-'+policy],checks=209 if platform=='windows' else 214,pattern=r'^debug_wasip1_checkpoint_runtime PASS checks=(\d+) policy='+policy+r'\r?$',repo=repo,policy=policy,fixture=fixture))
group=json.loads((O/'environment-group-qualified.json').read_text());assert group['passed'] and group['source_manifest_sha256']==sha(D/'inputs.json')
assert sha(O/'environment-group.exe')==group['binary_sha256']==proof['group_binary_sha256']
assert all(sha(k)==h for k,h in group['dependencies'].items())
for name in ('environment-group.exe','group-main.wasm','group-provider.wasm'):
 if name!='environment-group.exe':assert sha(O/name)==proof['group_wasm'][name.split('.')[0]]
 ship(O/name,repo+'-'+name)
for policy in ('instruction','unwind'):
 tests.append(dict(label=repo+'-environment-group-'+policy,binary=repo+'-environment-group.exe',args=['FILE:'+repo+'-group-main.wasm','FILE:'+repo+'-group-provider.wasm',policy,'DIR:'+repo+'-environment-group-'+policy],checks=485,pattern=r'^debug_wasip1_environment_group_runtime PASS checks=(\d+) policy='+policy+r'\r?$',repo=repo,policy=policy,fixture='environment-group'))
for name in ('worker-chain-main.wasm','worker-chain-provider.wasm'):
 assert sha(O/name)==proof['group_wasm'][name.split('.')[0]]
 ship(O/name,repo+'-'+name)
for policy in ('instruction','unwind'):
 tests.append(dict(label=repo+'-worker-chain-'+policy,binary=repo+'-environment-group.exe',args=['FILE:'+repo+'-worker-chain-main.wasm','FILE:'+repo+'-worker-chain-provider.wasm',policy,'DIR:'+repo+'-worker-chain-'+policy,'worker-chain'],checks=485,pattern=r'^debug_wasip1_environment_group_runtime PASS checks=(\d+) policy='+policy+r'\r?$',repo=repo,policy=policy,fixture='worker-chain'))
tests.sort(key=lambda t: t['label'].endswith('-public-cli'))
expected=[t['label'] for t in tests]
if platform=='freebsd':
 base=D.parent/'wasip1-cross-jit-20261007-r26/freebsd-clean-packed.qcow2'
 baseproof=json.loads((D.parent/'wasip1-cross-jit-20261007-r26/freebsd-base-compaction.json').read_text());assert sha(base)==baseproof['packed_sha256'];baseformat='qcow2'
 script=['#!/bin/sh','set -eu','exec >/dev/ttyu0 2>&1',"trap 'status=$?; trap - EXIT; echo UWVM_SCRIPT_STATUS_$status; shutdown -p now; exit $status' EXIT",'mkdir /tmp/uwvm-seed','mount -t cd9660 -o ro /dev/iso9660/CIDATA /tmp/uwvm-seed','mkdir /tmp/uwvm-work','chown uwvmprobe /tmp/uwvm-work','echo UWVM_BEGIN_'+nonce,'uname -a']
 for name,h in digests.items():script+=['test "$(sha256 -q /tmp/uwvm-seed/'+name+')" = '+h,'cp /tmp/uwvm-seed/'+name+' /tmp/uwvm-work/'+name,'chmod 0555 /tmp/uwvm-work/'+name]
 for t in tests:
  argv=['/tmp/uwvm-work/'+t['binary']]
  for a in t['args']:
   if a.startswith('DIR:'):
    path='/tmp/uwvm-work/'+a[4:]+'-root';script+=['mkdir '+path,'chown uwvmprobe '+path];argv.append(path)
   elif a.startswith('FIFO:'):
    path='/tmp/uwvm-work/'+a[5:]+'-fifo';script+=['mkfifo '+path,'chown uwvmprobe '+path];argv.append(path)
   elif a.startswith(('FILE:','WORK:')):argv.append('/tmp/uwvm-work/'+a.split(':',1)[1])
   else:argv.append(a)
  expected_checks=t['checks']
  log='/tmp/uwvm-work/'+t['label']+'.log'
  script+=['echo UWVM_START_'+nonce+'_'+t['label'],'set +e','su -m uwvmprobe -c "'+' '.join(argv)+'" >'+log+' 2>&1','status=$?','set -e','cat '+log,'echo UWVM_TEST_'+nonce+'_'+t['label']+'_STATUS_$status','test "$status" = 0',"grep -Eq '"+t['pattern'].replace(r'\d','[0-9]').replace(r'\r?','')+"' "+log]
  if t.get('export'):
   for index,path in enumerate(t['export']):script+=['printf "UWVM_METADATA_'+nonce+'_'+t['repo']+'_'+t['policy']+'_'+str(index)+'_"','base64 </tmp/uwvm-work/'+path+' | tr -d "\\n"','echo']
 script+=['echo UWVM_END_'+nonce]
 cfg={'users':[{'name':'uwvmprobe','shell':'/bin/sh','locked':False}],'hostname':'uwvm-portable-checkpoint','ssh_pwauth':False,'write_files':[{'path':'/etc/rc.conf.d/'+n,'permissions':'0600','content':n+'_enable="NO"\n'} for n in ('firstboot_pkgs','firstboot_pkg_upgrade','sshd')]+[{'path':'/tmp/uwvm-native-test.sh','permissions':'0700','content':'\n'.join(script)+'\n'}],'runcmd':['/bin/sh /tmp/uwvm-native-test.sh']}
 (isofs/'user-data').write_text('#cloud-config\n'+json.dumps(cfg,indent=2)+'\n');(isofs/'meta-data').write_text(json.dumps({'instance-id':'uwvm-'+nonce,'local-hostname':'uwvm-portable-checkpoint'}))
else:
 base=Path('/home/macromodel/Documents/qemu/uwvm2-win11-storage/data.img');baseformat='raw';varsbase=base.parent/'windows.vars';shutil.copy2(varsbase,R/'windows.vars')
 ps=["$ErrorActionPreference='Stop'","$port=New-Object IO.Ports.SerialPort 'COM1',115200,'None',8,'One'","$port.Open()",'$port.WriteLine("UWVM_BEGIN_'+nonce+'")','$port.WriteLine([Environment]::OSVersion.VersionString)',"$work=Join-Path $env:TEMP 'uwvm-"+nonce+"';New-Item -ItemType Directory -Path $work | Out-Null",'$root=Split-Path -Parent $MyInvocation.MyCommand.Path','try {']
 for name,h in digests.items():ps+=['$src=Join-Path $root "'+name+'"','if((Get-FileHash $src -Algorithm SHA256).Hash.ToLowerInvariant() -ne "'+h+'"){throw "input hash changed"}','$null=$src']
 for t in tests:
  ps+=['$binary=Join-Path $root "'+t['binary']+'"'];argv=[]
  for i,a in enumerate(t['args']):
   if a.startswith(('DIR:','FILE:','WORK:')):
    ps+=['$arg'+str(i)+'=Join-Path '+('$root' if a.startswith('FILE:') else '$work')+' "'+a.split(':',1)[1]+('-root' if a.startswith('DIR:') else '')+'"'];argv.append('$arg'+str(i))
    if a.startswith('DIR:'):ps+=['New-Item -ItemType Directory -Path $arg'+str(i)+' | Out-Null']
   else:argv.append('"'+a+'"')
  expected_checks=t['checks']
  ps+=['$port.WriteLine("UWVM_START_'+nonce+'_'+t['label']+'")','$out=Join-Path $work "'+t['label']+'.log"',"$ErrorActionPreference='Continue'",'& $binary '+' '.join(argv)+' *> $out','$status=$LASTEXITCODE',"$ErrorActionPreference='Stop'",'Get-Content $out | ForEach-Object {$port.WriteLine($_)}','$port.WriteLine("UWVM_TEST_'+nonce+'_'+t['label']+'_STATUS_$status")','if($status -ne '+str(t.get('expected_status',0))+'){throw "native test failed"}',*(["if(-not (Select-String -Path $out -Pattern '"+t['pattern']+"' -Quiet)){throw 'missing native PASS'}"] if t['pattern'] else [])]
  if t.get('export'):
   for index,path in enumerate(t['export']):ps+=['$metadata=Join-Path $work "'+path+'"','$port.WriteLine("UWVM_METADATA_'+nonce+'_'+t['repo']+'_'+t['policy']+'_'+str(index)+'_"+[Convert]::ToBase64String([IO.File]::ReadAllBytes($metadata)))']
 ps+=['$port.WriteLine("UWVM_END_'+nonce+'")','} catch {$port.WriteLine("UWVM_ERROR_'+nonce+' "+$_)} finally {$port.Close()}','shutdown.exe /s /t 0']
 (isofs/'run.ps1').write_text('\n'.join(ps),encoding='utf-8-sig')
base_before=stamp(base);run([Q/'bin/qemu-img','create','-f','qcow2','-F',baseformat,'-b',base,R/'disk.qcow2'])
disk=R/'disk.qcow2';disk_storage_proof=None
iso=Path(A['selected_sdks']['windows']['path']).parent/'iso-tool/usr/bin/genisoimage';run([iso,'-quiet','-J','-R','-V','CIDATA','-o',R/'inputs.iso',isofs])
argv=[str(Q/'bin/qemu-system-x86_64'),'-name','uwvm-joint-prepare-'+platform+'-'+nonce[:8],'-machine','q35,dump-guest-core=off,mem-merge=off','-accel','kvm','-cpu','host','-smp','2','-m','2048','-no-user-config','-nodefaults','-display','none','-vga','none','-nic','none','-monitor','none','-parallel','none','-L',str(Q/'share/qemu')]
if platform=='freebsd':argv+=['-bios',str(Q/'share/seabios/bios-256k.bin')]
else:argv+=['-device','VGA,romfile='+str(Q/'share/seabios/vgabios-stdvga.bin'),'-drive','if=pflash,unit=0,format=raw,readonly=on,file='+str(base.parent/'windows.rom'),'-drive','if=pflash,unit=1,format=raw,file='+str(R/'windows.vars')]
argv+=['-device','virtio-scsi-pci,id=scsi0,addr=0xa','-drive','if=none,id=osdisk,format=qcow2,file='+str(disk)+(',cache=none' if platform=='windows' else ''),'-device','scsi-hd,drive=osdisk,bus=scsi0.0,bootindex=1','-drive','if=none,id=inputs,media=cdrom,format=raw,readonly=on,file='+str(R/'inputs.iso'),'-device','scsi-cd,drive=inputs,bus=scsi0.0','-serial','file:'+str(R/'serial.log'),'-qmp','unix:'+str(IPC/'qmp.sock')+',server=on,wait=off','-S']
receipt=dict(os=platform,nonce=nonce,mode='joint-preparation-native',target_source_manifest_sha256=sha(D/'inputs.json'),input_access='Direct CDFS readonly input; newly compiled joint candidate preparation against actual full LLVM runtime',argv=argv,input_sha256=digests,base_before=base_before,base_path=str(base),tests=tests,passed=False,compiled_inputs=compiled_inputs,source_hashes=source_hashes,private_disk_storage=disk_storage_proof)
(R/'receipt.json').write_text(json.dumps(receipt,indent=2));print('VM run '+str(R),flush=True)
# Give this owned VM a KVM descriptor without changing host ACLs or groups.
acl_before=subprocess.check_output(['getfacl','-cp','/dev/kvm'])
try:
 kvm_fd=os.open('/dev/kvm',os.O_RDWR);provider='direct'
except PermissionError:
 listener=socket.socket(socket.AF_UNIX);listener.bind(str(IPC/'kvm.sock'));listener.listen(1);listener.settimeout(30)
 token=secrets.token_hex(32)
 code="import socket,os,array; s=socket.socket(socket.AF_UNIX); s.connect('/ipc/kvm.sock'); f=os.open('/dev/kvm',os.O_RDWR); s.sendmsg(["+repr(token.encode())+"],[(socket.SOL_SOCKET,socket.SCM_RIGHTS,array.array('i',[f]))]); os.close(f); s.close()"
 helper=['docker','run','--rm','--user=1000:1000','--group-add='+str(os.stat('/dev/kvm').st_gid),'--cap-drop=ALL','--network=none','--read-only','--security-opt=no-new-privileges','--device=/dev/kvm:/dev/kvm:rw','--cpuset-cpus=16','--memory=33554432','--memory-swap=33554432','-v',str(IPC)+':/ipc:rw','--entrypoint','python3','uwvm3-implementation-checkpoint:20261002-kvm-r1','-c',code]
 proc=subprocess.Popen(helper,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
 try:
  connection,_=listener.accept()
  with connection:
   payload,ancillary,flags,address=connection.recvmsg(128,socket.CMSG_SPACE(array.array('i').itemsize))
  assert payload==token.encode() and not flags,(payload,flags,proc.communicate(timeout=30))
  rights=array.array('i')
  for level,kind,data in ancillary:
   assert level==socket.SOL_SOCKET and kind==socket.SCM_RIGHTS
   rights.frombytes(data[:len(data)-(len(data)%rights.itemsize)])
  assert len(rights)==1;kvm_fd=rights[0]
  out,err=proc.communicate(timeout=30);assert proc.returncode==0,(out,err)
  provider={'helper_argv':helper,'image_id':subprocess.check_output(['docker','image','inspect','--format','{{.Id}}','uwvm3-implementation-checkpoint:20261002-kvm-r1']).decode().strip()}
 except BaseException:
  if proc.poll() is None:proc.kill()
  out,err=proc.communicate(timeout=30);(R/'kvm-helper-error.log').write_bytes(out+b'\n'+err)
  raise
 finally:
  listener.close()
  if proc.poll() is None:proc.kill();proc.wait()
info=os.fstat(kvm_fd);assert stat.S_ISCHR(info.st_mode) and (os.major(info.st_rdev),os.minor(info.st_rdev))==(10,232)
assert fcntl.ioctl(kvm_fd,0xae00)==12
assert subprocess.check_output(['getfacl','-cp','/dev/kvm'])==acl_before
argv[argv.index('kvm')]='kvm,device=/dev/fdset/1';argv+=['-add-fd','fd='+str(kvm_fd)+',set=1,opaque=owned-kvm-'+nonce]
receipt['argv']=argv;receipt['kvm_descriptor_provider']=provider;receipt['host_kvm_acl_sha256']=hashlib.sha256(acl_before).hexdigest();receipt['host_kvm_permissions_unchanged']=True
with (R/'qemu.log').open('wb') as log:
 try:child=subprocess.Popen(argv,env=env,stdout=log,stderr=subprocess.STDOUT,pass_fds=(kvm_fd,),preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_FSIZE,(1<<30,1<<30)))
 finally:os.close(kvm_fd)
 started=time.monotonic()
 try:
  while not (IPC/'qmp.sock').exists():assert child.poll() is None,'QEMU bootstrap failed';assert time.monotonic()-started<30;time.sleep(.1)
  sock=socket.socket(socket.AF_UNIX);sock.connect(str(IPC/'qmp.sock'));sock.settimeout(45);stream=sock.makefile('rwb',buffering=0);json.loads(stream.readline());seq=0
  def qmp(command,args=None):
   global seq
   seq+=1;packet={'execute':command,'id':seq}
   if args is not None:packet['arguments']=args
   receipt['qmp_last_request']=packet;receipt['qmp_last_request_started']=time.monotonic()-started
   stream.write(json.dumps(packet).encode()+b'\n')
   while True:
    row=json.loads(stream.readline())
    if row.get('id')==seq:assert 'error' not in row,row;receipt['qmp_successful_requests']=seq;return row.get('return')
  qmp('qmp_capabilities');receipt['kvm']=qmp('query-kvm');assert receipt['kvm']['enabled'];qmp('cont')
  if platform=='windows':
   for _ in range(30):assert child.poll() is None;time.sleep(5)
   qmp('screendump',{'filename':str(R/'boot.png'),'format':'png'})
   qmp('send-key',{'keys':[{'type':'qcode','data':'meta_l'},{'type':'qcode','data':'r'}],'hold-time':80});time.sleep(5)
   qmp('send-key',{'keys':[{'type':'qcode','data':'ctrl'},{'type':'qcode','data':'a'}],'hold-time':80});time.sleep(3)
   command="& 'D:\\run.ps1'";text='powershell.exe -NoProfile -ExecutionPolicy Bypass -EncodedCommand '+base64.b64encode(command.encode('utf-16le')).decode()
   for c in text:
    codes=[c] if c.isdigit() or 'a'<=c<='z' else ['shift',c.lower()] if 'A'<=c<='Z' else {' ':['spc'],'.':['dot'],'-':['minus'],'/':['slash'],'=':['equal'],'+':['shift','equal']}[c]
    qmp('send-key',{'keys':[{'type':'qcode','data':code} for code in codes],'hold-time':80});time.sleep(.15)
   qmp('send-key',{'keys':[{'type':'qcode','data':'ret'}],'hold-time':80});time.sleep(10);qmp('screendump',{'filename':str(R/'after-command.png'),'format':'png'})
  last=0
  while child.poll() is None:
   assert time.monotonic()-started<900,'VM deadline'
   if time.monotonic()-last>45:print('VM '+platform+' elapsed='+str(round(time.monotonic()-started)),flush=True);last=time.monotonic()
   assert (R/'disk.qcow2').stat().st_size<900<<20,'private overlay stopping bound'
   assert (R/'serial.log').stat().st_size<16<<20,'serial bound'
   time.sleep(.5)
  data=(R/'serial.log').read_text(errors='replace');print('\n'.join(line for line in data.splitlines() if re.search(r'checks(?:=| )\d+|\d+ checks passed|UWVM_(TEST_|END_|ERROR_)',line)),flush=True);receipt['serial_sha256']=sha(R/'serial.log');receipt['base_after']=stamp(base)
  receipt['passed']=child.returncode==0 and receipt['base_after']==base_before and 'UWVM_END_'+nonce in data and all('UWVM_TEST_'+nonce+'_'+t['label']+'_STATUS_'+str(t.get('expected_status',0)) in data for t in tests)
  receipt['test_statuses']={t['label']:('UWVM_TEST_'+nonce+'_'+t['label']+'_STATUS_'+str(t.get('expected_status',0)) in data) for t in tests}
  receipt['test_pass_checks']={}
  for t in tests:
   start='UWVM_START_'+nonce+'_'+t['label'];end='UWVM_TEST_'+nonce+'_'+t['label']+'_STATUS_'+str(t.get('expected_status',0))
   at=data.find(start);finish=data.find(end,at) if at>=0 else -1
   if t['pattern']:
    found=re.search(t['pattern'],data[at:finish],re.M) if finish>=at>=0 else None
    actual=int(found[1]) if found else None
    receipt['test_pass_checks'][t['label']]=actual
    receipt['passed']=receipt['passed'] and found is not None and (actual>=t['checks'] if t.get('minimum') else actual==t['checks'])
   else:
    receipt['test_pass_checks'][t['label']]=1 if finish>=at>=0 else None
    receipt['passed']=receipt['passed'] and finish>=at>=0
  exports=[]
  for repo,policy,index,encoded in re.findall(r'^UWVM_METADATA_'+nonce+r'_(uwvm2(?:-ros)?)_(instruction|unwind)_([0])_([A-Za-z0-9+/=]+)\r?$',data,re.M):
   payload=base64.b64decode(encoded,validate=True)
   assert 156<=len(payload)<16<<20 and int.from_bytes(payload[:8],'little')==0x0031504953505755 and hashlib.sha256(payload[:-32]).digest()==payload[-32:]
   dest=D/('migration-source-'+platform+'-'+repo+'-'+policy+'.uwp');exports.append((dest,payload))
  assert not exports,'joint rehearsal tests must not invent portable exports';assert receipt['passed'],'guest tests or retirement failed'
  assert all(sha(k)==h for proof_path in compiled_inputs for k,h in json.loads(Path(proof_path).read_text())['dependencies'].items()),'source changed during actual guest execution'
  for dest,payload in exports:
   dest.write_bytes(payload);receipt.setdefault('exported_metadata',{})[str(dest)]=sha(dest)
 except BaseException:
  receipt['failure_traceback']=traceback.format_exc();raise
 finally:
  if child.poll() is None:child.kill()
  child.wait(timeout=20);receipt['qemu_exit']=child.returncode;(R/'receipt.json').write_text(json.dumps(receipt,indent=2));print(json.dumps({'os':platform,'passed':receipt['passed'],'receipt':str(R/'receipt.json')}),flush=True)
  # Preserve inputs hashes, serial proof and receipts; retire only own bulky VM files.
  if receipt['passed']:
   for name in ('disk.qcow2','inputs.iso','windows.vars'):(R/name).unlink(missing_ok=True)
   retired_inputs={}
   for name,h in digests.items():
    p=isofs/name;assert sha(p)==h;p.unlink();retired_inputs[name]=h
   receipt['retired_owned_shipping_hardlinks']=retired_inputs;(R/'receipt.json').write_text(json.dumps(receipt,indent=2))
  shutil.rmtree(IPC)
  if receipt['passed']:
   for p,h in retire_duplicates:assert sha(p)==h;p.unlink()

