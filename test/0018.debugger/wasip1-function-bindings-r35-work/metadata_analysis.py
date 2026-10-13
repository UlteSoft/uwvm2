"""Verify compiler-command regression and append historical corrections."""
from pathlib import Path
import hashlib, json, runpy, sys
D=Path(__file__).parent; OLD=D.parent/'wasip1-prepared-retirement-20261008-r34'
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
assert Path('/proc/self/cgroup').read_text()=='0::/system.slice/docker-d3b12e6d323a0f28c2408989fff41a1ca1a991e19945919bc870337dd0afae03.scope\n'
runpy.run_path(str(D/'test_cross_cached_compile_row.py'),run_name='__main__')
sys.path.insert(0,str(OLD)); import qualified_archive as archive
matrix_path=OLD/'final-native-matrix-qualified.json'; matrix_before=sha(matrix_path)
matrix=json.loads(matrix_path.read_text()); assert matrix['passed'] and matrix['native_runs']==48
corrections=[]; references=[]
for repo in ('uwvm2','uwvm2-ros'):
    group=next(g for g in matrix['groups'] if g['repo']==repo and g['os']=='macos')
    recovery_path=Path(group['recovery_receipt']); assert sha(recovery_path)==group['recovery_receipt_sha256']
    recovery=json.loads(recovery_path.read_text()); assert recovery['passed'] and recovery['all_payloads_read_back']
    assert sha(recovery['archive'])==recovery['archive_sha256']==group['archive_sha256']
    seen={}
    with archive.open_reader(recovery['archive']) as stream:
        for member in stream:
            assert member.isfile() and member.name in recovery['payloads'] and member.name not in seen
            with stream.extractfile(member) as payload: seen[member.name]=hashlib.file_digest(payload,'sha256').hexdigest()
    assert seen==recovery['payloads']
    products=OLD/'products-v8/macos'/repo
    results_path=products/'results.json'; assert sha(results_path)==seen['results.json']
    results=json.loads(results_path.read_text())
    for label in ('runtime','host-api'):
        proof_path=products/(label+'-qualified.json'); old_hash=sha(proof_path)
        proof=json.loads(proof_path.read_text()); assert seen[label+'.o']==proof['object_sha256']
        rows=[r for r in results if r['name']==label+'-compile' and r['passed'] and r['exit']==0]
        assert len(rows)==1; actual=rows[0]
        assert actual['log_sha256']==seen[label+'-compile.log']
        assert actual['argv'][0].endswith('/clang++') and '-c' in actual['argv']
        assert actual['argv'][actual['argv'].index('-o')+1]==str(products/(label+'.o'))
        assert sha(actual['argv'][0])==proof['compiler_sha256']
        assert all(sha(p)==h for p,h in proof['dependencies'].items())
        row=dict(repo=repo,stage=label,original_qualification=str(proof_path),original_qualification_sha256=old_hash,
                 recorded_argv=proof['argv'],actual_compile_argv=actual['argv'],results_sha256=sha(results_path),
                 object_sha256=proof['object_sha256'],compile_log_sha256=actual['log_sha256'],
                 archive_sha256=recovery['archive_sha256'],all_archive_payloads_read_again=True,
                 payload_count=len(seen),original_source_manifest_sha256=matrix['source_manifest_sha256'],
                 original_qualification_preserved=True,new_native_execution_claimed=False)
        (corrections if proof['argv']!=actual['argv'] else references).append(row)
        assert sha(proof_path)==old_hash
assert len(corrections)==2 and all(r['repo']=='uwvm2' for r in corrections)
assert len(references)==2 and all(r['repo']=='uwvm2-ros' for r in references)
assert sha(matrix_path)==matrix_before
receipt=dict(passed=True,corrections=corrections,unchanged_correct_references=references,
             old_native_matrix_sha256=matrix_before,old_native_matrix_preserved=True,
             old_native_runs=48,new_native_runs=0,controller_sha256=sha(__file__),
             explanation='Cached compile reused its authenticated object, but its qualification argv was overwritten with a later validator row. These additive records correct provenance without rewriting the historical archives or claiming new native tests.')
(D/'historical-compile-command-correction.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('historical cached compiler provenance: two corrected, two independently consistent; all payloads read',flush=True)
