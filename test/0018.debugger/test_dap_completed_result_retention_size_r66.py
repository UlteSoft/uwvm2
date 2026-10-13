"""Synthetic regression tests; execute only in the designated Linux cgroup."""
from pathlib import Path
import unittest,tempfile,json,lzma,hashlib,os,subprocess
from debug_completed_retention_size_r66 import retired_result_charge,completed_result,MAX_RESULT
class ResultRetention(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
  self.root=Path(self.tmp.name);self.session=self.root/'session-0001-rust-dwarf5-O1';self.session.mkdir()
  broker=b'TinyGo DAP original guest wait: 0\n';(self.session/'broker.raw').write_bytes(broker)
  self.result=dict(passed=True,original_guest_wait_returncode=0,cleanup=dict(returncode=0,broker_log_sha256=hashlib.sha256(broker).hexdigest()))
  self.keep()
 def keep(self):
  raw=json.dumps(self.result).encode();packed=lzma.compress(raw)
  (self.session/'results.json.xz').write_bytes(packed)
  self.receipt=dict(archive='results.json.xz',bytes=len(packed),sha256=hashlib.sha256(packed).hexdigest(),original_bytes=len(raw),original_sha256=hashlib.sha256(raw).hexdigest(),fsync_completed=True,stream_before_after_equal=True)
  self.write_receipt();self.raw=raw
 def write_receipt(self):(self.session/'results-retention.json').write_text(json.dumps(self.receipt))
 def probe(self):return retired_result_charge(self.session/'results.json',self.root)
 def test_exact_closed_replacement_charged_conservatively(self):
  charge,proof=self.probe();self.assertEqual(charge,len(self.raw));self.assertTrue(proof['actual_original_waits_and_broker_bytes_verified'])
 def test_plain_completed_record_remains_readable(self):
  (self.session/'results.json').write_bytes(self.raw)
  result,receipt=completed_result(self.session);self.assertTrue(result['passed']);self.assertFalse(receipt['retained'])
  with self.assertRaises(AssertionError):self.probe()
 def test_corrupt_archive(self):
  with (self.session/'results.json.xz').open('ab') as f:f.write(b'corrupt')
  with self.assertRaises(AssertionError):self.probe()
 def test_corrupt_original_digest(self):
  self.receipt['original_sha256']='0'*64;self.write_receipt()
  with self.assertRaises(AssertionError):self.probe()
 def test_nonzero_guest_wait(self):
  self.result['original_guest_wait_returncode']=1;self.keep()
  with self.assertRaises(AssertionError):self.probe()
 def test_nonzero_broker_wait(self):
  self.result['cleanup']['returncode']=-9;self.keep()
  with self.assertRaises(AssertionError):self.probe()
 def test_broker_bytes_changed(self):
  (self.session/'broker.raw').write_bytes(b'changed')
  with self.assertRaises(AssertionError):self.probe()
 def test_not_fsynced(self):
  self.receipt['fsync_completed']=False;self.write_receipt()
  with self.assertRaises(AssertionError):self.probe()
 def test_oversized_declared_result(self):
  self.receipt['original_bytes']=MAX_RESULT+1;self.write_receipt()
  with self.assertRaises(AssertionError):self.probe()
 def test_unknown_missing_file(self):
  with self.assertRaises(AssertionError):retired_result_charge(self.session/'unknown.json',self.root)
 def test_outside_owned_root(self):
  with self.assertRaises(AssertionError):retired_result_charge(self.session/'results.json',self.root/'other')
 def test_symlinked_session(self):
  alias=self.root/'session-0002-rust-dwarf5-O1';alias.symlink_to(self.session,target_is_directory=True)
  with self.assertRaises(AssertionError):retired_result_charge(alias/'results.json',self.root)
if __name__=='__main__':
 subprocess.run(['bash',str(Path(__file__).resolve().parents[2]/'tools/ci/require_wasm3_test_cgroup.sh')],check=True)
 unittest.main()
