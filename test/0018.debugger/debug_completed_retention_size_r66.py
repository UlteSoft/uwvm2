"""Closed result accounting for a bounded debugger test supervisor.

Unknown disappearance remains fatal. Only a completed own result with
verified, fsynced replacement bytes can be charged after its plain file retires.
"""
from pathlib import Path
import hashlib,json,lzma,re
MAX_RESULT=8<<20
def sha(path):
 with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def owned_session(session,write_root):
 session=Path(session);write_root=Path(write_root)
 assert session.is_relative_to(write_root) and session!=write_root
 assert re.fullmatch(r'session-[0-9]{4}(?:-[A-Za-z0-9_-]+)?',session.name)
 p=session
 while p!=write_root:
  assert not p.is_symlink();p=p.parent
 return session
def completed_result(session):
 session=Path(session);plain=session/'results.json';retained=False;record=None
 try:
  with plain.open('rb') as f:raw=f.read(MAX_RESULT+1)
 except FileNotFoundError:
  retained=True
  receipt=session/'results-retention.json'
  with receipt.open('rb') as f:encoded=f.read(65537)
  assert len(encoded)<=65536;record=json.loads(encoded)
  assert record['archive']=='results.json.xz' and record['fsync_completed'] is True and record['stream_before_after_equal'] is True
  assert 0<record['bytes']<=MAX_RESULT and 0<record['original_bytes']<=MAX_RESULT
  archive=session/'results.json.xz';assert not archive.is_symlink()
  assert archive.stat().st_size==record['bytes'] and sha(archive)==record['sha256']
  with lzma.open(archive,'rb') as f:raw=f.read(record['original_bytes']+1)
  assert len(raw)==record['original_bytes'] and hashlib.sha256(raw).hexdigest()==record['original_sha256']
  assert archive.stat().st_size==record['bytes'] and sha(archive)==record['sha256'] and not plain.exists()
 assert 0<len(raw)<=MAX_RESULT
 result=json.loads(raw)
 assert result['passed'] is True and result['original_guest_wait_returncode']==0 and result['cleanup']['returncode']==0
 broker=session/'broker.raw';assert not broker.is_symlink() and sha(broker)==result['cleanup']['broker_log_sha256']
 return result,dict(retained=retained,original_bytes=len(raw),archive_sha256=record['sha256'] if record else None)
def retired_result_charge(path,write_root):
 path=Path(path);assert path.name=='results.json';session=owned_session(path.parent,write_root)
 result,receipt=completed_result(session)
 assert receipt['retained'] and not path.exists()
 # Conservative charge keeps the vanished original in this scan's quota,
 # even when the compressed replacement is also observed.
 return receipt['original_bytes'],dict(path=str(path),**receipt,actual_original_waits_and_broker_bytes_verified=True)
