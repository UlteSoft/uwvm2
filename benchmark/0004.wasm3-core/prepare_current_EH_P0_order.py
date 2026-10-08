#!/usr/bin/env python3
"""Fixed source-bound EH P0 command ordering only; never launch a guest/tool."""
from pathlib import Path
import argparse,hashlib,json,os,tempfile,types
HERE=Path(__file__).parent
PARENT_SHA='51f4b9a268391d61881d8110a01b65cccfa212db42d00a2c2e9e1c33cd2cb05f'
B=Path('/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924')
COLD=B/'builds/current-R3c-EH-cold-20261002-r2'
SUMMARY=B/'evidence/current-R3c-EH-cold-complete-small-packet-20261002-r2/summary.json'
OUT=B/'builds/current-R3c-EH-P0-order-20261003-r1'
PINS={'cold-after.json':'2c42e273aaaa16db126394d7d5bcb214124a7f7088795c3bb39adde5f4fba536',
      'plan.json':'339cef20684a879cfb2e7d299a8ca41d2f50aebcbd623ee04a8c481a398f5920',
      'commands.json':'3f52f3bdf8122924094e6f317a9e4433371477b8f9a30de52f8351953d1b8408',
      'summary.json':'404c59488a9edc0fb6d6836321f8ced33c35e6628c65fa09af7bc7bfcee3a223'}
ORDER=['eh-throws-unwind-native-unwind','eh-throws-unwind-auto',
       'eh-normal-unwind-native-unwind','plain-normal-unwind-native-unwind',
       'eh-throws-instruction-native-unwind','eh-throws-instruction-auto',
       'eh-normal-instruction-native-unwind','plain-normal-instruction-native-unwind',
       'eh-normal-unwind-auto','plain-normal-unwind-auto',
       'eh-normal-instruction-auto','plain-normal-instruction-auto']

def require(ok,message):
    if not ok:raise RuntimeError(message)
def sha(path):
    with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def parent():
    path=HERE/'prepare_current_EH_thread_commands.py';require(sha(path)==PARENT_SHA,'Frozen parent changed')
    m=types.ModuleType('fixed_eh_commands');m.__file__=str(path)
    exec(compile(path.read_bytes(),str(path),'exec'),m.__dict__);return m

def order(rows,product,definitions,observed):
    """Pure mapping: actual argv is preserved after peeling fixed env wrapper."""
    require(set(rows)==set(ORDER) and len(rows)==12,'Exact twelve EH cells required')
    mapped={}
    for label,argv in rows.items():
        require(argv[:3]==['env','-u','UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT'] and len(argv)>8,'Exact capture-clear wrapper')
        assignments=argv[3:7];require(all(type(v) is str and '=' in v for v in assignments),'Exact env assignments')
        env=dict(v.split('=',1) for v in assignments)
        require(set(env)=={'LD_LIBRARY_PATH','RAYON_NUM_THREADS','UWVM_TEST_CPUSET','PYTHONDONTWRITEBYTECODE'} and
                env['RAYON_NUM_THREADS']=='1' and env['UWVM_TEST_CPUSET']=='0,2,4,6,16-31' and env['PYTHONDONTWRITEBYTECODE']=='1',
                'Original actual EH environment changed')
        native=argv[7:];require(native[0]==product['product']['path'],'Actual product changed')
        trace=native[native.index('-Rllvm-call-stack')+1];dispatch=native[native.index('-Rllvm-exception-dispatch')+1]
        name=next(n for n in definitions if label.startswith(n.replace('_','-')+'-'))
        require(label==name.replace('_','-')+'-'+trace+'-'+dispatch and trace in ('instruction','unwind') and dispatch in ('auto','native-unwind'),'Actual option/label mismatch')
        require(observed[label]=='native' if dispatch=='native-unwind' or name=='plain_normal' else observed[label]=='r2-phase','Pinned actual cold dispatch differs')
        mapped[label]={'label':label,'fixture':name,'profile':'ordinary/'+trace+'/'+dispatch,
          'argv':['taskset','-c','0',*native],'environment_delta':env,
          'remove_environment':['UWVM2_TEST_CAPTURE_NATIVE_JIT_OBJECT'],
          'cold_observed_pending_plan':observed[label],'must_reobserve_pending_plan':True,
          'expected_self_check':definitions[name],'whole_guest_HW_scope':'startup/JIT/execution/output of this single TID, not Wasm-only',
          'formal_acceptance':False}
    throws=[label for label in ORDER if label.startswith('eh-throws-')]
    return {'plain':[mapped[label] for label in ORDER]+[mapped[label] for label in reversed(throws)],
            'pure_hw':[mapped[label] for label in throws],
            'vtune':[dict(mapped[label],collector='hotspots',sampling_mode='hw') for label in throws if label.endswith('native-unwind')]+
                    [dict(mapped['eh-throws-unwind-native-unwind'],collector='uarch-exploration',pmu_collection_mode='summary')]}

def historical_init_only(summary,cold_plan):
    # Actual archived after_init_only is an init PID, not a boolean. This only
    # authenticates the old cold receipt; a new P0 admission must be live/fresh.
    return (type(summary.get('after_init_only')) is int and summary['after_init_only']==10154 and
            cold_plan.get('current_init')==[10154,21355])

def main():
    argparse.ArgumentParser(description=__doc__).parse_args()
    p=parent();require(not OUT.exists(),'Fresh order directory required')
    prefix,tail,product=p.parent_product()  # Original source/MD/all3TU/RSP/SDK checks unchanged.
    paths={name:SUMMARY if name=='summary.json' else COLD/name for name in PINS}
    for name,path in paths.items():require(sha(path)==PINS[name],'Actual fixed cold receipt changed: '+name)
    after=json.loads(paths['cold-after.json'].read_bytes());summary=json.loads(SUMMARY.read_bytes())
    cold_plan=json.loads(paths['plan.json'].read_bytes())
    require(after['passed'] is True and summary['passed'] is True and after['actual_EH_cells']==summary['actual_EH_cell_count']==12 and
            after['product']==product and after['source_id']==summary['source_id']==p.SID and
            after['source_product_transitive_MD_SDK_tools_before_after_equal'] is True and
            summary['root_all_stages_rc0'] is True and historical_init_only(summary,cold_plan) and summary['OOM_events_unchanged'] is True,'Actual cold/product closure changed')
    original=dict(json.loads(paths['commands.json'].read_bytes()))
    selected={label:original[label] for label in ORDER}
    expected={label.replace('_','-'):argv for label,argv in p.lists(prefix,tail)['eh-cold']}
    require(selected==expected,'Cold actual argv differs from original frozen producer')
    require(set(after['actual_cell_proofs'])==set(ORDER),'Actual cold cells differ')
    for label,argv in selected.items():
        proof=after['actual_cell_proofs'][label]
        require(proof['actual_argv']==argv and proof['actual_root_returncode']==0 and proof['original_PIDFD_retired'] is True,'Actual cold cell not complete')
        require(p.content_pin(proof['raw_log']['path'])=={k:proof['raw_log'][k] for k in ('bytes','sha256')},'Actual cold raw log changed')
    for name,definition in p.EH.items():require(sha(p.F/(name+'.wasm'))==definition['sha256'],'Pinned same Wasm changed')
    observed={row['label']:row['dispatch_plan'] for row in summary['cells']}
    commands=order(selected,product,p.EH,observed)
    OUT.mkdir()
    for family,rows in commands.items():p.save(OUT/(family+'-commands.json'),rows)
    p.save(OUT/'plan.json',{'schema':'uwvm-current-R3c-EH-P0-order-v1','product':product,
      'cold_receipts':{name:p.pin(path) for name,path in paths.items()},'original_parent':p.pin(HERE/'prepare_current_EH_thread_commands.py'),
      'source_helper':p.pin(__file__),'commands':{name:p.pin(OUT/(name+'-commands.json')) for name in commands},
      'actual_P0_plain_passed':False,'actual_HW_passed':False,'actual_VTune_passed':False,'formal_acceptance':False,
      'requires_current_boot_admission_and_unchanged_protocol_before_after_closure':True,
      'actual_loaded_DSO_maps_qualified':False,'temperature_policy':'observation_only',
      'short_auto_sample_policy':'retain actual result unqualified if exec/frequency/window proof is absent; no fake padding',
      'notes':['Order producer never launches; keeper current-source adapter/guardian must be reviewed separately.',
       'Move environment_delta to real Popen env; clean capture/profiler/preload/audit and observe actual child.',
       'Observed cold dispatch is not authority for the next process; reobserve owning-source/cache/body/plan.',
       'Thread multiTID samples are excluded from this single-TID EH counter plan.']})
    print('PREPARED EH P0 order only: 16 plain,4 HW,3 VTune; native/performance pending',OUT)
if __name__=='__main__':main()
