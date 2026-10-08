#!/usr/bin/env python3
"""Pure controls for a NEW, narrowly scoped native terminal observer.

No proc reads, launches, signals, FD calls, waiting or old guard modifications.
A serialized dict is never ownership authority. The private owner_cookie and
original PIDFD must remain held by the keeper's actual Popen-entry owner. The
keeper obtains every input from actual observations; success here requires a
repeat of the entire unchanged original check and final counter/closure gates.
It cannot authorize a signal, qualify a sample, or repair the old failed row.
"""
from copy import deepcopy
import re

MM_FIELDS = ('vsize','rss_pages','startcode','endcode','startstack','start_data',
             'end_data','start_brk','arg_start','arg_end','env_start','env_end')
ORIGINAL_ERROR = 'Owned command changed: no unique eligible original child'
IDENTITY = ('pid','birth','ppid','pgid','uid','cgroup','cpus')

def terminal_guest_transition(previous, initial, terminal, *, expected):
    failures=[]
    def check(ok,why):
        if not ok: failures.append(why)
    try:
        check(type(expected['owner_cookie']) is object,'Private controller cookie missing')
        check(type(expected['original_pidfd']) is int and expected['original_pidfd']>=0,
              'Original PIDFD invalid')
        check(expected['role']=='guest' and type(expected['pid']) is int and
              expected['pid']>0 and type(expected['birth']) is int and expected['birth']>0 and
              expected['pgid']==expected['pid'] and expected['uid']==[1000]*4 and
              expected['cpus']=='0','Expected native identity invalid')
        check(isinstance(expected['argv'],list) and expected['argv'] and
              all(type(v) is str for v in expected['argv']) and
              expected['argv'][0]==expected['executable'] and
              expected['executable'].startswith('/') and
              isinstance(expected['elf_sha256'],str) and
              re.fullmatch('[0-9a-f]{64}',expected['elf_sha256']) is not None,
              'Expected exact native argv/ELF pin missing')
        check(isinstance(previous,dict),'No previous accepted native execution')
        if not isinstance(previous,dict): return {'passed':False,'failures':failures}
        for name,proof in (('previous',previous),('initial',initial),('terminal',terminal)):
            check(proof['owner_cookie'] is expected['owner_cookie'],name+' private owner changed')
            check(type(proof['original_pidfd']) is int and
                  proof['original_pidfd']==expected['original_pidfd'],name+' original PIDFD changed')
            check(proof['reaped'] is False and proof['process_returncode'] is None,
                  name+' reaped numeric PID is not live original ownership')
            check(type(proof['time_ns']) is int and proof['time_ns']>0,
                  name+' actual timestamp missing')
        check(previous['complete_original_check_passed'] is True,
              'Prior partial check cannot authenticate native execution')
        actual=previous['actual_exec']
        check(all(actual.get(k)==expected[k] for k in IDENTITY) and
              actual.get('argv')==expected['argv'] and actual.get('executable')==expected['executable'],
              'Prior actual native identity or loaded executable incomplete/changed')
        loaded=previous['loaded_ELF']
        check(loaded.get('resolved_path')==expected['executable'] and
              loaded.get('sha256')==expected['elf_sha256'],
              'Prior actual loaded ELF path/bytes differ from frozen pin')
        check(previous['time_ns']<=initial['time_ns']<terminal['time_ns'],
              'Terminal observation not fresh after prior complete check and initial failure')
        check(initial['error']==ORIGINAL_ERROR,'Unrelated original error rejected')
        row=initial['row']
        check(all(row.get(k)==expected[k] for k in IDENTITY),'Initial ownership/CPU changed')
        check(row.get('argv')==[],'Nonempty wrong argv remains hard failure')
        check(row.get('state') in ('R','S','D','Z','T') and
              type(row.get('rss')) is int and row['rss']>=0,
              'Malformed actual initial proc row')
        check(terminal['pidfd_ready_before'] is True and terminal['pidfd_ready_after'] is True,
              'Original PIDFD did not prove retirement around fresh observations')
        stat=terminal['actual_stat'];fresh=terminal['row'];absent=terminal['actual_stat_absent_reads']
        check(type(absent) is int and absent in (0,2),'Invalid actual stat-absence count')
        if absent==0:
            check(isinstance(stat,dict) and isinstance(fresh,dict),
                  'No actual fresh Z row/stat')
            if not isinstance(stat,dict) or not isinstance(fresh,dict):
                return {'passed':False,'failures':failures}
            check(all(stat.get(k)==expected[k] for k in ('pid','birth','ppid','pgid')) and
                  stat.get('state')=='Z','Fresh stat is live or has wrong birth/ancestry')
            check(type(stat.get('mm')) is dict and set(stat['mm'])==set(MM_FIELDS) and
                  all(type(v) is int and v==0 for v in stat['mm'].values()),
                  'Fresh terminal MM not all twelve actual zero fields')
            check(all(fresh.get(k)==expected[k] for k in IDENTITY) and
                  fresh.get('state')=='Z' and fresh.get('argv')==[] and fresh.get('rss')==0,
                  'Fresh terminal UID/cgroup/CPU/argv/identity changed')
        elif absent==2:
            check(stat is None and fresh is None,
                  'Two actual absent stat reads cannot override a returned live/wrong row')
        else:
            check(False,'One missing read is not original terminal proof')
    except (KeyError,TypeError,AttributeError,ValueError):
        failures.append('Missing/malformed actual proof carrier')
    return {'passed':not failures,'failures':failures}

def controls():
    cookie=object()
    expected=dict(owner_cookie=cookie,original_pidfd=9,pid=311320,birth=2872261,
        ppid=311172,pgid=311320,uid=[1000]*4,cgroup='0::/original-known-cgroup\n',cpus='0',
        executable='/actual/frozen/native-gc-baseline',
        argv=['/actual/frozen/native-gc-baseline','mutate-numeric','16000000','1065886208','3100901888'],
        elf_sha256='9de9198dabc3e2c32ac351f25e0a5893ff60ddc2ca4510c22f825db68c0f31a8',role='guest')
    def common():
        return dict(owner_cookie=cookie,original_pidfd=9,reaped=False,process_returncode=None)
    identity={k:deepcopy(expected[k]) for k in IDENTITY}
    previous=common();previous.update(time_ns=100,complete_original_check_passed=True,
        actual_exec=dict(identity,argv=deepcopy(expected['argv']),executable=expected['executable']),
        loaded_ELF=dict(resolved_path=expected['executable'],sha256=expected['elf_sha256']))
    initial=common();initial.update(time_ns=200,error=ORIGINAL_ERROR,
        row=dict(identity,state='R',argv=[],rss=2826240))
    terminal=common();terminal.update(time_ns=300,pidfd_ready_before=True,pidfd_ready_after=True,
        row=dict(identity,state='Z',argv=[],rss=0),actual_stat_absent_reads=0,
        actual_stat={**{k:expected[k] for k in ('pid','birth','ppid','pgid')},
            'state':'Z','mm':dict.fromkeys(MM_FIELDS,0)})
    def copy_proof(value):
        # Preserve the private controller object; copy data-only observations.
        result=deepcopy(value);result['owner_cookie']=cookie;return result
    def trial(label,*,before=previous,start=initial,end=terminal,want):
        result=terminal_guest_transition(before,start,end,expected=expected)
        assert result['passed'] is want,(label,result)
        return label
    valid=[trial('actual same-birth Z with original ready PIDFD',want=True)]
    absent=copy_proof(terminal);absent.update(actual_stat_absent_reads=2,row=None,actual_stat=None)
    valid.append(trial('two actual absent stats while original entry remains unreaped',end=absent,want=True))
    rejected=[]
    for name,key,value in [('birth','birth',2872262),('PID','pid',1),('parent','ppid',1),
            ('PGID','pgid',1),('UID','uid',[0]*4),('CG','cgroup','other'),('P0','cpus','16')]:
        for stage in ('previous','initial','terminal'):
            p,i,t=map(copy_proof,(previous,initial,terminal))
            target=p['actual_exec'] if stage=='previous' else i['row'] if stage=='initial' else t['row']
            target[key]=value
            rejected.append(trial(stage+' changed '+name,before=p,start=i,end=t,want=False))
    for stage in ('previous','initial','terminal'):
        for key,value in [('owner_cookie',object()),('original_pidfd',10),
                          ('reaped',True),('process_returncode',0)]:
            p,i,t=map(copy_proof,(previous,initial,terminal));{'previous':p,'initial':i,'terminal':t}[stage][key]=value
            rejected.append(trial(stage+' invalid '+key,before=p,start=i,end=t,want=False))
    for key,value in [('complete_original_check_passed',False),('actual_exec',{}),('time_ns',250)]:
        p=copy_proof(previous);p[key]=value;rejected.append(trial('missing prior '+key,before=p,want=False))
    rejected.append(trial('no prior actual native execution',before=None,want=False))
    for key,value in [('argv',['/different/native']),('executable','/different/native')]:
        p=copy_proof(previous);p['actual_exec'][key]=value;rejected.append(trial('prior actual '+key+' wrong',before=p,want=False))
    for key,value in [('resolved_path','/different/native'),('sha256','0'*64)]:
        p=copy_proof(previous);p['loaded_ELF'][key]=value;rejected.append(trial('prior loaded ELF '+key+' wrong',before=p,want=False))
    for key,value in [('error','Owned UID/cgroup changed'),('time_ns',400)]:
        i=copy_proof(initial);i[key]=value;rejected.append(trial('initial invalid '+key,start=i,want=False))
    i=copy_proof(initial);i['row']['argv']=['/changed/native'];rejected.append(trial('nonempty wrong initial argv',start=i,want=False))
    for key,value in [('pidfd_ready_before',False),('pidfd_ready_after',False),('time_ns',150),
                      ('actual_stat_absent_reads',1),('actual_stat_absent_reads',True)]:
        t=copy_proof(terminal);t[key]=value;rejected.append(trial('terminal invalid '+key+'='+repr(value),end=t,want=False))
    for key,value in [('state','R'),('birth',2872262),('pid',1),('ppid',1),('pgid',1)]:
        t=copy_proof(terminal);t['actual_stat'][key]=value;rejected.append(trial('fresh actual stat wrong '+key,end=t,want=False))
    for name in MM_FIELDS:
        t=copy_proof(terminal);t['actual_stat']['mm'][name]=1;rejected.append(trial('nonzero actual MM '+name,end=t,want=False))
    for key,value in [('state','R'),('argv',['changed']),('rss',1)]:
        t=copy_proof(terminal);t['row'][key]=value;rejected.append(trial('fresh actual row wrong '+key,end=t,want=False))
    for key in MM_FIELDS:
        t=copy_proof(terminal);t['actual_stat']['mm'].pop(key);rejected.append(trial('missing actual MM '+key,end=t,want=False))
    for key,value in [('actual_stat_absent_reads',1),('row',{'state':'R'}),('actual_stat',{'state':'R'}),('reaped',True)]:
        t=copy_proof(absent);t[key]=value;rejected.append(trial('absent transition invalid '+key,end=t,want=False))
    for stage,key in [('previous','actual_exec'),('previous','loaded_ELF'),('initial','row'),('terminal','actual_stat')]:
        p,i,t=map(copy_proof,(previous,initial,terminal));{'previous':p,'initial':i,'terminal':t}[stage].pop(key)
        rejected.append(trial(stage+' missing actual carrier '+key,before=p,start=i,end=t,want=False))
    assert len(valid)==2 and len(rejected)==89
    assert len(set(valid+rejected))==len(valid)+len(rejected)
    return {'schema':'uwvm-native-terminal-transition-synthetic-controls-v1',
        'valid_controls':len(valid),'rejected_controls':len(rejected),
        'native_execution_performed':False,'proc_observations_performed':False,
        'old_failed_row_qualified':False,'original_guard_modified':False,
        'controls':{'valid':valid,'rejected':rejected}}

if __name__=='__main__':
    import json
    print(json.dumps(controls(),indent=2))
