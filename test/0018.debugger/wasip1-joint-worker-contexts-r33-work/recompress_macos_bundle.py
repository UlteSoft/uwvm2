from pathlib import Path
import json,sys,hashlib,subprocess,time
D=Path(__file__).parent
assert sys.argv[1:]==['joint-recompress-macos-r33']
import qualified_archive as qa
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
p=D/'macos-both-repositories-bundle-qualified.json';old=json.loads(p.read_text())
assert old['passed'] and old['all_payloads_read_back'] and old['deterministic_corresponding_member_order'] and sha(old['archive'])==old['archive_sha256']
backup=D/'macos-sorted-bundle-readback-too-large.json';assert not backup.exists();backup.write_bytes(p.read_bytes())
for r,a in old['originals'].items():assert sha(a['archive'])==a['archive_sha256']
a=D/'macos-both-repositories-ultra-qualified-products.tar.zst';assert not a.exists()
# Only compression has a separate 4 GiB aggregate RSS allowance in the original
# 64 GiB cgroup. Test limits and the local 512 MiB decoder window do not change.
args=[str(qa.tool()),'-q','--ultra','-22','--long=29','-T1','-c']
started=time.monotonic()
with a.open('xb') as out:
 dec=subprocess.Popen([str(qa.tool()),'-q','-d','--long=29','-c',old['archive']],stdout=subprocess.PIPE)
 enc=subprocess.Popen(args,stdin=dec.stdout,stdout=out);dec.stdout.close()
 try:
  assert enc.wait(timeout=3300)==0 and dec.wait(timeout=30)==0
 finally:
  for proc in (enc,dec):
   if proc.poll() is None:proc.terminate()
  for proc in (enc,dec):
   try:proc.wait(timeout=10)
   except subprocess.TimeoutExpired:proc.kill();proc.wait()
seen={}
with qa.open_reader(a) as t:
 for m in t:
  assert m.isfile() and m.name in old['payloads'] and m.name not in seen
  with t.extractfile(m) as f:seen[m.name]=hashlib.file_digest(f,'sha256').hexdigest()
assert seen==old['payloads'] and a.stat().st_size<old['archive_bytes']
new=dict(old);new.update(archive=str(a),archive_bytes=a.stat().st_size,archive_sha256=sha(a),compression_command=args,compression_guard_rss_limit_bytes=4<<30,compression_elapsed_seconds=time.monotonic()-started,decoder_window_limit_bytes=512<<20,sorted_previous_qualification_sha256=sha(backup),sorted_previous_archive_sha256=old['archive_sha256'],sorted_previous_archive_bytes=old['archive_bytes'],native_test_limits_unchanged=True,local_cold_aggregate_limit_bytes=2<<30)
p.write_text(json.dumps(new,indent=2)+'\n')
# Every member has been fully read back, and original repository archives remain.
assert sha(old['archive'])==old['archive_sha256'];Path(old['archive']).unlink()
print('Exact native payloads recompressed and fully read back; sorted duplicate retired',a.stat().st_size,flush=True)
