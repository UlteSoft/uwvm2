import sys,os,subprocess,json
from pathlib import Path
D=Path(__file__).parent
assert os.environ['TMPDIR']==str(D/'tmp')
if sys.argv[1] in ('joint-inputs-r29','joint-unit-r29','joint-linux-integrated-r29','joint-cross-r29','joint-vm-r29','joint-archive-r29','joint-report-r29','joint-historical-retire-r29','joint-storage-r29','joint-delivery-r29'):
 import runpy
 route={'joint-inputs-r29':'bootstrap.py','joint-unit-r29':'linux-integrated.py','joint-linux-integrated-r29':'linux-integrated.py','joint-cross-r29':'cross-integrated.py','joint-vm-r29':'vm.py','joint-archive-r29':'archive.py','joint-report-r29':'report.py','joint-historical-retire-r29':'historical_retire.py','joint-storage-r29':'final_storage.py','joint-delivery-r29':'delivery.py'}[sys.argv[1]]
 runpy.run_path(str(D/'rounds/wasip1-dispatch-20261007-r29'/route),run_name='__main__')
elif sys.argv[1] in ('joint-storage-r28','joint-delivery-r28'):
 import runpy
 assert len(sys.argv)==2
 route={'joint-storage-r28':'final_storage.py','joint-delivery-r28':'delivery.py'}[sys.argv[1]]
 runpy.run_path(str(D/'rounds/wasip1-world-install-20261007-r28'/route),run_name='__main__')
elif sys.argv[1]=='joint-source-retire-r28':
 import runpy
 assert len(sys.argv)==2
 runpy.run_path(str(D/'rounds/wasip1-world-install-20261007-r28/retire_sources.py'),run_name='__main__')
elif sys.argv[1]=='joint-cold-product-r28':
 import runpy
 assert len(sys.argv)==4
 runpy.run_path(str(D/'rounds/wasip1-world-install-20261007-r28/cold_product.py'),run_name='__main__')
elif sys.argv[1]=='joint-failed-archive-r28':
 import runpy
 assert len(sys.argv)==2
 runpy.run_path(str(D/'rounds/wasip1-world-install-20261007-r28/failed_archive.py'),run_name='__main__')
elif sys.argv[1]=='joint-cold-rehome-r28':
 import runpy
 assert len(sys.argv)==2
 runpy.run_path(str(D/'rounds/wasip1-world-install-20261007-r28/cold_rehome.py'),run_name='__main__')
elif sys.argv[1]=='joint-inputs-r28':
 import runpy
 assert len(sys.argv)==2
 runpy.run_path(str(D/'rounds/wasip1-world-install-20261007-r28/bootstrap.py'),run_name='__main__')
elif sys.argv[1] in ('joint-linux-integrated-r28','joint-cross-r28','joint-vm-r28','joint-archive-r28','joint-report-r28'):
 import runpy
 route={'joint-linux-integrated-r28':'linux-integrated.py','joint-cross-r28':'cross-integrated.py','joint-vm-r28':'vm.py','joint-archive-r28':'archive.py','joint-report-r28':'report.py'}[sys.argv[1]]
 runpy.run_path(str(D/'rounds/wasip1-world-install-20261007-r28'/route),run_name='__main__')
elif sys.argv[1]=='joint-delivery-r27':
 import runpy
 assert len(sys.argv)==2
 runpy.run_path(str(D/'rounds/wasip1-joint-prepare-20261007-r27/delivery.py'),run_name='__main__')
elif sys.argv[1]=='joint-report-r27':
 import runpy
 assert len(sys.argv)==2
 runpy.run_path(str(D/'rounds/wasip1-joint-prepare-20261007-r27/report.py'),run_name='__main__')
elif sys.argv[1]=='joint-macos-delivery-r27':
 import runpy
 assert len(sys.argv)==3 and sys.argv[2] in ('uwvm2','uwvm2-ros')
 runpy.run_path(str(D/'rounds/wasip1-joint-prepare-20261007-r27/macos_delivery.py'),run_name='__main__')
elif sys.argv[1]=='joint-cold-store-r27':
 import runpy
 assert len(sys.argv)==2
 runpy.run_path(str(D/'rounds/wasip1-joint-prepare-20261007-r27/cold_store.py'),run_name='__main__')
elif sys.argv[1]=='joint-pch-archive-r27':
 import runpy
 assert len(sys.argv)==2
 runpy.run_path(str(D/'rounds/wasip1-joint-prepare-20261007-r27/pch_archive.py'),run_name='__main__')
elif sys.argv[1]=='joint-vm-r27':
 import runpy
 assert len(sys.argv)==4 and sys.argv[2] in ('windows','freebsd') and sys.argv[3] in ('uwvm2','uwvm2-ros')
 runpy.run_path(str(D/'rounds/wasip1-joint-prepare-20261007-r27/vm.py'),run_name='__main__')
elif sys.argv[1]=='joint-linux-integrated-r27':
 import runpy
 assert len(sys.argv)==3 and sys.argv[2] in ('uwvm2','uwvm2-ros')
 runpy.run_path(str(D/'rounds/wasip1-joint-prepare-20261007-r27/linux-integrated.py'),run_name='__main__')
elif sys.argv[1]=='joint-archive-r27':
 import runpy
 assert len(sys.argv)==4 and sys.argv[2] in ('linux','linux-integrated','windows','freebsd','macos') and sys.argv[3] in ('uwvm2','uwvm2-ros')
 runpy.run_path(str(D/'rounds/wasip1-joint-prepare-20261007-r27/archive.py'),run_name='__main__')
elif sys.argv[1]=='joint-cross-r27':
 import runpy
 assert len(sys.argv)==4 and sys.argv[2] in ('windows','freebsd','macos') and sys.argv[3] in ('uwvm2','uwvm2-ros')
 runpy.run_path(str(D/'rounds/wasip1-joint-prepare-20261007-r27/cross-integrated.py'),run_name='__main__')
elif sys.argv[1]=='joint-qualified-r27':
 import runpy
 assert len(sys.argv)==3 and sys.argv[2] in ('uwvm2','uwvm2-ros')
 runpy.run_path(str(D/'rounds/wasip1-joint-prepare-20261007-r27/qualified.py'),run_name='__main__')
elif sys.argv[1]=='joint-finite-r27':
 import runpy
 assert len(sys.argv)==3 and sys.argv[2] in ('uwvm2','uwvm2-ros')
 runpy.run_path(str(D/'rounds/wasip1-joint-prepare-20261007-r27/finite.py'),run_name='__main__')
elif sys.argv[1]=='joint-recheck-r27':
 import runpy
 assert len(sys.argv)==3 and sys.argv[2] in ('uwvm2','uwvm2-ros')
 runpy.run_path(str(D/'rounds/wasip1-joint-prepare-20261007-r27/recheck.py'),run_name='__main__')
elif sys.argv[1]=='joint-prepare-r27':
 import runpy
 assert len(sys.argv)==3 and sys.argv[2] in ('uwvm2','uwvm2-ros')
 runpy.run_path(str(D/'rounds/wasip1-joint-prepare-20261007-r27/linux.py'),run_name='__main__')
elif sys.argv[1]=='cross-jit-r26':
 import runpy
 route=sys.argv[2]
 assert route in ('sdk','openssl','product','vm','storage_test','sdkcompact','productarchive','sdk_target_audit','coff_linux','pre_vm_compact','helper_rebuild','stack_diagnostic','pe_stack_diagnostic','main_rebuild','compact_freebsd_base','linux_product','sdk_freebsd','restore_linux_portable','product_freebsd','vm_freebsd','sdkcompact_freebsd','productarchive_freebsd','sdk_macos_restore','sdk_macos','product_macos','sdkcompact_macos','darwin_probe','migration_linux','migration_vm','macos_delivery_archive','migration_vm_freebsd','product_readonly','vm_readonly','productarchive_readonly','macho_probe','platform_io_helpers','closing_snapshot','archive_final_evidence','delivery_amendment')
 sys.argv.insert(1,'runtime-tests')
 runpy.run_path(str(D/'rounds/wasip1-cross-jit-20261007-r26'/(route+'.py')),run_name='__main__')
elif sys.argv[1]=='full-compile-r25':
 import runpy
 assert sys.argv[2:]==['build','uwvm2']
 sys.argv.insert(1,'runtime-tests')
 runpy.run_path(str(D/'rounds/wasip1-live-integration-20261007-r25/live.py'),run_name='__main__')
elif sys.argv[1]=='live-integration-r25':
 import runpy
 sys.argv.insert(1,'runtime-tests')
 runpy.run_path(str(D/'rounds/wasip1-live-integration-20261007-r25/live.py'),run_name='__main__')
elif sys.argv[1]=='runtime-tests' and len(sys.argv)>2 and sys.argv[2]=='live-integration-r25':
 import runpy
 runpy.run_path(str(D/'rounds/wasip1-live-integration-20261007-r25/live.py'),run_name='__main__')
elif sys.argv[1] in ('components','runtime-tests') and len(sys.argv)>2 and sys.argv[2]=='atomic-export-r24':
 import runpy
 runpy.run_path(str(D/'rounds/wasip1-atomic-export-20261007-r24/components.py'),run_name='__main__')
elif sys.argv[1]=='atomic-export-r24':
 import runpy
 assert sys.argv[2]=='vm'
 runpy.run_path(str(D/'rounds/wasip1-atomic-export-20261007-r24/vm_test.py'),run_name='__main__')
elif sys.argv[1] in ('components','runtime-tests') and len(sys.argv)>2 and sys.argv[2]=='export-cleanup-r23':
 import runpy
 runpy.run_path(str(D/'rounds/wasip1-export-cleanup-20261006-r23/components.py'),run_name='__main__')
elif sys.argv[1]=='export-cleanup-r23':
 import runpy
 assert sys.argv[2]=='vm'
 runpy.run_path(str(D/'rounds/wasip1-export-cleanup-20261006-r23/vm_test.py'),run_name='__main__')
elif sys.argv[1] in ('components','runtime-tests') and len(sys.argv)>2 and sys.argv[2]=='checkpoint-controller-r22':
 import runpy
 runpy.run_path(str(D/'rounds/wasip1-checkpoint-controller-20261006-r22/components.py'),run_name='__main__')
elif sys.argv[1]=='checkpoint-controller-r22':
 import runpy
 assert sys.argv[2]=='vm'
 runpy.run_path(str(D/'rounds/wasip1-checkpoint-controller-20261006-r22/vm_test.py'),run_name='__main__')
elif sys.argv[1]=='components' and len(sys.argv)>2 and sys.argv[2]=='edit-ack-r21':
 import runpy
 runpy.run_path(str(D/'rounds/wasip1-edit-ack-20261006-r21/components.py'),run_name='__main__')
elif sys.argv[1]=='edit-ack-r21':
 import runpy
 assert sys.argv[2]=='vm'
 runpy.run_path(str(D/'rounds/wasip1-edit-ack-20261006-r21/vm_test.py'),run_name='__main__')
elif sys.argv[1]=='components' and len(sys.argv)>2 and sys.argv[2]=='file-close-r20':
 import runpy
 runpy.run_path(str(D/'rounds/wasip1-file-close-20261006-r20/components.py'),run_name='__main__')
elif sys.argv[1]=='file-close-r20':
 import runpy
 assert sys.argv[2]=='vm'
 runpy.run_path(str(D/'rounds/wasip1-file-close-20261006-r20/vm_test.py'),run_name='__main__')
elif sys.argv[1]=='components' and len(sys.argv)>2 and sys.argv[2]=='checkpoint-ack-r19':
 import runpy
 runpy.run_path(str(D/'rounds/wasip1-checkpoint-ack-20261006-r19/components.py'),run_name='__main__')
elif sys.argv[1]=='checkpoint-ack-r19':
 import runpy
 assert sys.argv[2]=='vm'
 runpy.run_path(str(D/'rounds/wasip1-checkpoint-ack-20261006-r19/vm_test.py'),run_name='__main__')
elif sys.argv[1]=='components':
 if len(sys.argv)>2 and sys.argv[2]=='portable-path-r18':
  import runpy
  runpy.run_path(str(D/'rounds/wasip1-path-admission-20261006-r18/components.py'),run_name='__main__')
 else:import components
elif sys.argv[1]=='portable-path-r18':
 import runpy
 assert sys.argv[2] in ('vm','freebsd-setup')
 runpy.run_path(str(D/'rounds/wasip1-path-admission-20261006-r18'/('vm_test.py' if sys.argv[2]=='vm' else 'freebsd_setup.py')),run_name='__main__')
elif sys.argv[1]=='retire-bootstrap':
 import retire_bootstrap
elif sys.argv[1]=='archive':
 import archive_evidence
elif sys.argv[1]=='file-limit':
 p=D/'tmp/file-limit-probe.bin'
 assert not p.exists()
 result=subprocess.run([sys.executable,'-c','from pathlib import Path;import sys;Path(sys.argv[1]).write_bytes(bytes(1048576))',str(p)],capture_output=True,text=True)
 try:
  assert result.returncode!=0 and p.stat().st_size==65536,(result.returncode,p.stat().st_size)
  proof=dict(passed=True,child_exit=result.returncode,actual_file_bytes=p.stat().st_size,hard_file_limit_bytes=65536,expected_rejection=True,cgroup=Path('/proc/self/cgroup').read_text())
 finally:p.unlink(missing_ok=True)
 (D/'file-limit-results.json').write_text(json.dumps(proof,indent=2)+'\n')
 print('file-size hard limit verified; owned probe retired',flush=True)
else:raise RuntimeError('unsupported recovery suite')
