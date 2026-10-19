#!/usr/bin/env python3
"""Actual stopped source printer commands, immutable registry and DAP retirement.

Private Source candidate: do not execute outside the keeper's admitted Linux
window. Compile the accompanying original fixture separately; the runner never
compiles a product, modifies Wasm/DWARF, or fabricates an object layout. The
actual product and producer build/MD receipts are mandatory and pinned.
"""
from pathlib import Path
import argparse
import json
import os
import re
import sys

sys.dont_write_bytecode = True
root_spelling = os.environ.get('UWVM_PRETTY_COMMAND_SOURCE_ROOT')
if root_spelling is None and '--source-root' in sys.argv:
    at = sys.argv.index('--source-root')
    if at+1 < len(sys.argv):
        root_spelling = sys.argv[at+1]
if root_spelling is None:
    raise RuntimeError('--source-root is required; broker children inherit the pinned root')
ROOT = Path(root_spelling).resolve(strict=True)
os.environ['UWVM_PRETTY_COMMAND_SOURCE_ROOT'] = str(ROOT)
sys.path.insert(0,str(ROOT/'test/0018.debugger'))
import run_dap_current_broker as live
import run_dap_tinygo_objects as base
import run_dap_pretty_producers as producers

PROFILES = (
    ('seq', 'custom_sequence', 'pretty-printer sequence seq "*CommandSequence<int>" data size cap'),
    ('text', 'custom_text', 'pretty-printer string text "*CommandText" textdata size'),
    ('pointer', 'custom_pointer', 'pretty-printer pointer pointer "*CommandPointer" ptr'),
    ('maybe', 'present', 'pretty-printer optional maybe "*CommandOptional" engaged value'),
)


def identity(position):
    return tuple(position[key] for key in ('thread','function','offset','epoch','stop'))


def qualify_build(args):
    product = json.loads(args.build_receipt.read_text())
    candidates = product.get('repositories',[product])
    rows = [row for row in candidates if row.get('binary_sha256') == live.sha(args.uwvm)]
    live.require(product.get('passed') is True and len(rows)==1,
                 'one actual successful product receipt matches original executable', rows)
    row = rows[0]
    live.require(row.get('passed') is True and row.get('all_product_TUs_fresh') is True and
                 row.get('inputs_before_equals_after') is True and
                 row.get('source_tools_headers_SDK_MD_before_after_equal') is True and
                 Path(row['source_root']).resolve(strict=True)==ROOT,
                 'fresh runtime/main/host-api and actual selected source/MD closure', row)
    producer = json.loads(args.producer_receipt.read_text())
    live.require(producer.get('passed') is True and
                 producer.get('source_sha256')==live.sha(args.source) and
                 producer.get('wasm_sha256')==live.sha(args.wasm) and
                 producer.get('dependencies_before_equals_after') is True,
                 'actual unchanged command producer/MD qualification receipt', producer)
    argv = producer.get('actual_compile_argv',[])
    dependencies = producer.get('actual_dependencies',{})
    live.require(isinstance(argv,list) and all(isinstance(v,str) for v in argv) and
                 '-O0' in argv and '-g' in argv and '-std=c++20' in argv and
                 str(args.source) in argv and '-o' in argv and
                 argv[argv.index('-o')+1]==str(args.wasm) and
                 isinstance(dependencies,dict) and dependencies,
                 'actual compiler command and nonempty actual producer dependency closure', producer)
    actual_source_found = False
    for spelling,pin in dependencies.items():
        path = Path(spelling).resolve(strict=True)
        live.require(isinstance(pin,dict) and live.sha(path)==pin.get('sha256') and
                     path.stat().st_size==pin.get('bytes'), 'unchanged actual producer dependency', spelling)
        actual_source_found = actual_source_found or path==args.source
    live.require(actual_source_found,'actual compiler MD must include original producer source')
    return {'product_source_id':row['source_id'],'producer_compile_argv':argv}


def session(args, folder):
    folder.mkdir(mode=0o700)
    dap = base.load(ROOT/'tools/debug/dap_adapter.py','actual_command_live_protocol')
    mode = [] if args.ros or args.fixed_llvm_full else ['-Rcc','jit','-Rcm','full']
    vm = ['-m','run',*mode,'-Rct','0','-Rllvm-call-stack',args.policy,
          '-Rllvm-cache-path','disable','--run',str(args.wasm)]
    server = live.BrokerSession(args.uwvm,Path(__file__).resolve(),ROOT/'tools/debug/dap_adapter.py',vm,folder)
    record = {'passed':False,'checks':[],'commands':[],'retirement_transactions':[],
              'scope':'actual HOST command configuration at real pauses; exact DAP wire and compiler-owned guest objects'}
    try:
        gate = dap.UnixBroker(str(server.directory))
        try:
            first = gate.request('pause')
            live.require('error:' not in first and 'guest exited:' not in first,'real first queued pause',first)
            record['first_pause'] = first
        finally:
            gate.close()
        client_folder = folder/'client';client_folder.mkdir(mode=0o700)
        client = base.TypedClient(ROOT/'tools/debug/dap_adapter.py',server.directory,client_folder,'source')
        server.clients.append(client)

        def check(label, good, detail=None):
            live.require(good,label,detail)
            record['checks'].append({'case':label,'passed':True})

        def command(text):
            reply = client.evaluate(text).decode()
            record['commands'].append({'command':text,'reply':reply})
            return reply

        initial = live.actual_location(client)
        check('initial default pretty on','is on.' in command('show print pretty'))
        check('initial default elements 200','limit is 200.' in command('show print elements'))
        pre = command('info pretty-printer')
        check('HOST registry query works before first continue','builtin enabled' in pre and
              'No custom pretty-printers registered.' in pre and 'shown=0 total=0 truncated=false' in pre,pre)
        check('HOST setup changes pretty before guest producer','is off.' in command('set print pretty off'))
        check('HOST setup changes elements before guest producer','limit is 2.' in command('set print elements 2'))
        check('HOST setup registration needs no guessed guest layout','preflight registered.' in
              command('pretty-printer pointer preflight "*CommandPointer" ptr'))
        for verb in ('disable','enable','delete'):
            expected={'disable':'disabled','enable':'enabled','delete':'deleted'}[verb]
            check('HOST setup '+verb,'preflight '+expected+'.' in command('pretty-printer '+verb+' preflight'))
        check('all HOST setup commands preserve original pause',identity(live.actual_location(client))==identity(initial))
        record['host_setup_position'] = initial

        line = next(i for i,text in enumerate(args.source.read_text().splitlines(),1)
                    if 'PRETTY_COMMANDS_LIVE_STOP' in text)
        function = producers.function_identity(args.wasm,'pretty_commands_live_probe')
        point = client.request('setBreakpoints',{'source':{'path':str(args.source)},
                                               'breakpoints':[{'line':line}]})['body']['breakpoints']
        check('compiler source marker breakpoint is verified',len(point)==1 and point[0]['verified'],point)
        client.request('continue',{'threadId':initial['thread']})
        position = producers.wait_stop(client)
        check('actual original Wasm function identity',position['function']==function,position)
        record.update(position=position,source_line=line,actual_function=function,breakpoint=point[0])

        def fresh_frame():
            frames = client.request('stackTrace',{'threadId':position['thread']})['body']['stackFrames']
            rows = [row for row in frames if row.get('source',{}).get('path')==str(args.source) and
                    row.get('line')==line and 'pretty_commands_live_probe' in row['name']]
            live.require(len(rows)==1,'one actual current source activation',frames)
            return rows[0]

        def scalar(name):
            return base.scalar(command(f"print /r {position['thread']} {position['stop']} {name}"))

        def stable(label):
            check(label+' actual stop identity',identity(live.actual_location(client))==identity(position))
            check(label+' original guest state',scalar('sentinel')==1007 and scalar('progress')==0)

        def tree(name, override=''):
            option=(override+' ') if override else ''
            packet=command(f"print {option}{position['thread']} {position['stop']} {name}")
            nodes=dap.parse_source_object_display(packet,position['stop'],position['thread'])
            live.require(nodes is not None and nodes,'actual compiler-owned copied object packet',(name,packet))
            return nodes

        def copies(nodes):
            return [nodes[i] for i in nodes[0]['children'] if nodes[i]['detached_copy']]

        def members(reference,**page):
            rows=client.request('variables',dict(variablesReference=reference,**page))['body']['variables']
            for row in rows:producers.readonly(row)
            return rows

        def mint():
            frame=fresh_frame()
            scopes=client.request('scopes',{'frameId':frame['id']})['body']['scopes']
            source=[row for row in scopes if row['name']=='Source variables']
            live.require(len(source)==1 and source[0]['variablesReference']>0,'real positive Source scope',scopes)
            scope=source[0]['variablesReference']
            values=members(scope)
            live.require(any(row['name']=='sentinel' for row in values),'actual scope labels before configuration',values)
            obj=client.request('evaluate',{'expression':'custom_sequence','context':'watch','frameId':frame['id']})['body']
            producers.readonly(obj)
            live.require(obj['variablesReference']>0 and members(obj['variablesReference']),
                         'real positive copied object reference before configuration',obj)
            return {'frame':frame['id'],'scope':scope,'object':obj['variablesReference']}

        def mutate(text, expected=None, invalid=False):
            # Mint AND successfully expand each old reference before the config.
            # Queue config+old reference requests in one actual DAP write. A new
            # stackTrace is required afterwards even when a config is rejected.
            old=mint()
            before_policy=(command('show print pretty'),command('show print elements'),command('pretty-printer list'))
            check('read-only policy queries retain existing references '+text,
                  bool(members(old['scope'])) and bool(members(old['object'])))
            event_at=len(client.events)
            requests=[('evaluate',{'expression':text,'context':'repl'}),
                      ('variables',{'variablesReference':old['scope']}),
                      ('variables',{'variablesReference':old['object']}),
                      ('evaluate',{'expression':'sentinel','context':'watch','frameId':old['frame']})]
            replies=[client.response(number) for number in client.queue(requests)]
            check('configuration reply '+text,replies[0].get('success') is (not invalid),replies[0])
            if expected is not None:
                check('configuration acknowledgement '+text,expected in replies[0].get('body',{}).get('result',''),replies[0])
            check('configuration retires existing Source scope/object/frame '+text,
                  all(row.get('success') is False for row in replies[1:]),replies)
            check('configuration never emits continued '+text,
                  all(row.get('event')!='continued' for row in client.events[event_at:]),client.events[event_at:])
            fresh=fresh_frame()
            check('configuration remints source frame '+text,fresh['id']!=old['frame'],(old,fresh))
            stable('configuration '+text)
            if invalid:
                after_policy=(command('show print pretty'),command('show print elements'),command('pretty-printer list'))
                check('invalid configuration leaves published preferences/registry unchanged '+text,
                      after_policy==before_policy,(before_policy,after_policy))
            record['retirement_transactions'].append({'command':text,'invalid':invalid,'old':old,'responses':replies,'new_frame':fresh['id']})

        stable('source marker baseline')
        fresh_frame()
        for expression,type_name,fields in (
                ('custom_sequence','CommandSequence',{'data','size','cap'}),
                ('custom_text','CommandText',{'textdata','size'}),
                ('custom_pointer','CommandPointer',{'ptr'}),
                ('present','CommandOptional',{'engaged','value'})):
            original=tree(expression,'/r')
            original_fields={original[i]['name'] for i in original[0]['children']}
            check('actual compiler declaration fields '+expression,
                  type_name in original[0]['type'] and original_fields==fields and
                  original[0].get('pretty') is None and not copies(original),original)
        check('HOST pretty setting survives actual producer initialization','is off.' in command('show print pretty'))
        check('HOST element budget survives actual producer initialization','limit is 2.' in command('show print elements'))
        check('default raw policy applies',tree('builtin_values')[0].get('pretty') is None)
        forced=tree('builtin_values','/p')
        check('print /p overrides raw default for one query',forced[0].get('pretty') is not None and
              'size=5' in forced[0]['pretty'] and [n['value'] for n in copies(forced)]==['7','8'],forced)
        check('print /p does not mutate default','is off.' in command('show print pretty'))
        mutate('set print pretty on','is on.')
        check('print /r overrides enabled printer',tree('builtin_values','/r')[0].get('pretty') is None)
        check('print /r does not mutate default','is on.' in command('show print pretty'))

        def sequence(name,expected):
            nodes=tree(name)
            check('actual sequence pretty '+name,nodes[0].get('pretty') is not None and 'size=5' in nodes[0]['pretty'],nodes)
            indexed=[row for row in copies(nodes) if row['indexed']]
            check('actual copied sequence elements '+name,[row['name'] for row in indexed]==[f'[{i}]' for i in range(len(expected))] and
                  [producers.numeric(row['value']) for row in indexed]==expected,indexed)
            frame=fresh_frame()
            for context in ('watch','hover','variables'):
                body=client.request('evaluate',{'expression':name,'context':context,'frameId':frame['id']})['body']
                producers.readonly(body)
                check('DAP exact current pretty summary '+context+' '+name,body['result']==nodes[0]['pretty'],body)
                shown=members(body['variablesReference'],filter='indexed') if body['variablesReference'] else []
                check('DAP copied indexed elements '+context+' '+name,
                      [producers.numeric(row['value']) for row in shown]==expected,shown)
                if shown:
                    check('DAP copied indexed pagination '+name,
                          members(body['variablesReference'],filter='indexed',start=1,count=1)==shown[1:2])
            return nodes

        mutate('set print elements 0','limit is 0.')
        zero=sequence('builtin_values',[])
        check('zero elements is summary-only with no detached payload',not copies(zero) and '...' in zero[0]['pretty'],zero)
        mutate('set print elements 2','limit is 2.')
        two=sequence('builtin_values',[7,8])
        check('truncated sequence still expands copied prefix','...' in two[0]['pretty'],two)
        mutate('set print elements 200','limit is 200.')
        sequence('builtin_values',[7,8,9,10,11])
        check('builtin string is truly formatted','builtin command text' in tree('builtin_text')[0].get('pretty',''))

        for name,expression,install in PROFILES:
            mutate(install,name+' registered.')
        listing=command('pretty-printer list')
        info=command('info pretty-printer')
        check('info/list describe all real configured profiles',all(name+' enabled ' in listing for name,_,_ in PROFILES) and
              'shown=4 total=4 truncated=false' in listing and listing==info,(listing,info))
        for spelling in ('type="*CommandSequence<int>" data=data length=size capacity=cap',
                         'type="*CommandText" data=textdata length=size',
                         'type="*CommandPointer" data=ptr',
                         'type="*CommandOptional" engaged=engaged value=value'):
            check('list retains configured compiler-field paths '+spelling,spelling in listing,listing)

        def custom(expression):
            nodes=tree(expression)
            summary=nodes[0].get('pretty')
            check('custom compiler type is formatted '+expression,summary is not None,nodes)
            payload=copies(nodes)
            if expression=='custom_sequence':
                check('custom size/cap and copied sequence data','size=5 capacity=5' in summary and
                      [row['value'] for row in payload]==['41','42','43','44','45'],nodes)
            elif expression=='custom_text':
                check('custom length-delimited character payload','string length=19' in summary and 'custom bounded text' in summary,nodes)
            elif expression=='custom_pointer':
                check('custom typed pointer copies actual pointee',summary=='stored pointer = 43' and
                      len(payload)==1 and payload[0]['name']=='*' and payload[0]['value']=='43',nodes)
            elif expression=='present':
                check('custom engaged value comes from compiler field',summary=='engaged = 73' and
                      len(payload)==1 and payload[0]['name']=='value' and payload[0]['value']=='73',nodes)
                absent=tree('absent')
                check('custom disengaged optional does not expose payload',absent[0].get('pretty')=='none' and not copies(absent),absent)
            return nodes

        for _,expression,_ in PROFILES:custom(expression)
        mutate('set print elements 0','limit is 0.')
        sequence('custom_sequence',[])
        for expression in ('custom_pointer','present'):
            zero=tree(expression)
            check('zero budget preserves custom summary without copied payload '+expression,
                  zero[0].get('pretty') is not None and not copies(zero),zero)
        mutate('set print elements 2','limit is 2.')
        sequence('custom_sequence',[41,42])
        mutate('set print elements 200','limit is 200.')
        mutate('set print pretty off','is off.')
        check('global raw preference bypasses custom profile',tree('custom_sequence')[0].get('pretty') is None)
        forced=tree('custom_sequence','/p')
        check('one-query pretty override uses actual custom fields',forced[0].get('pretty') is not None and
              [row['value'] for row in copies(forced)]==['41','42','43','44','45'],forced)
        check('custom one-query pretty leaves global raw unchanged','is off.' in command('show print pretty'))
        mutate('set print pretty on','is on.')
        mutate('pretty-printer disable builtin','builtin disabled.')
        check('builtin group disable bypasses vector printer',tree('builtin_values')[0].get('pretty') is None)
        for _,expression,_ in PROFILES:custom(expression)
        check('builtin disable retains custom profiles','builtin disabled' in command('pretty-printer list'))
        mutate('pretty-printer enable builtin','builtin enabled.')
        sequence('builtin_values',[7,8,9,10,11])
        for name,expression,_ in PROFILES:
            mutate('pretty-printer disable '+name,name+' disabled.')
            disabled=tree(expression)
            check('disabled custom profile has raw fields '+name,disabled[0].get('pretty') is None and not copies(disabled),disabled)
            mutate('pretty-printer enable '+name,name+' enabled.')
            custom(expression)

        for bad in ('set print pretty yes','set print elements -1','set print elements 4097',
                    'pretty-printer pointer rejected "*CommandPointer" ptr()',
                    'pretty-printer delete missing','pretty-printer delete builtin'):
            mutate(bad,invalid=True)
        for name,expression,_ in PROFILES:
            mutate('pretty-printer delete '+name,name+' deleted.')
            deleted=tree(expression)
            check('deleted custom profile keeps raw compiler object '+name,
                  deleted[0].get('pretty') is None and not copies(deleted),deleted)
        listing=command('pretty-printer list')
        check('registry deletion publishes complete empty snapshot','No custom pretty-printers registered.' in listing and
              'shown=0 total=0 truncated=false' in listing and 'builtin enabled' in listing,listing)
        check('final pretty default remains enabled','is on.' in command('show print pretty'))
        check('final element default restored to 200','limit is 200.' in command('show print elements'))
        stable('all command/configuration checks')
        client.request('setBreakpoints',{'source':{'path':str(args.source)},'breakpoints':[]})
        client.request('continue',{'threadId':position['thread']})
        for _ in range(64):
            exited=client.evaluate('wait')
            if b'guest exited:' in exited:
                check('unchanged original producer natural oracle exit',b'guest exited: 0' in exited,exited.decode())
                break
        else:raise AssertionError('actual command producer did not exit naturally')
        server.child.wait(timeout=15)
        check('original broker actual OS wait',server.child.returncode==0,server.child.returncode)
        record.update(passed=True,natural_guest_exit=exited.decode())
    except BaseException as error:
        record['error']=repr(error)
        raise
    finally:
        try:
            record['cleanup']=server.close()
            raw=(folder/'broker.raw').read_text()
            live.require('TinyGo DAP gate: authenticated first command queued before VM exec' in raw,
                         'original first-pause gate evidence',raw)
            if record['passed']:
                live.require('TinyGo DAP original guest wait: 0' in raw,'original guest OS wait evidence',raw)
        except BaseException as error:
            record['passed']=False
            record['cleanup_error']=repr(error)
            raise
        finally:
            (folder/'results.json').write_text(json.dumps(record,indent=2)+'\n')
    return record


def main():
    if len(sys.argv)>1 and sys.argv[1] in ('serve','__vm_gate'):
        return base.main()
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source-root','uwvm','wasm','source','out','build-receipt','producer-receipt'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--ros',action='store_true')
    parser.add_argument('--fixed-llvm-full', action='store_true', help='product has one fixed LLVM-full runtime; omit unavailable selector flags')
    parser.add_argument('--policy',choices=('instruction','unwind'),default='instruction')
    args=parser.parse_args()
    for name in ('source_root','uwvm','wasm','source','build_receipt','producer_receipt'):
        setattr(args,name,getattr(args,name).resolve(strict=True))
    live.require(args.source_root==ROOT,'one explicit actual Source root')
    live.require('uwvm-dbg-pretty-commands-live-v1' in args.source.read_text(),'original versioned command producer')
    admission=base.load(ROOT/'test/0018.debugger/run_pretty_printer_matrix.py','command_live_admission')
    proof=admission.admit()
    build=qualify_build(args)
    args.out.mkdir(mode=0o700,parents=True,exist_ok=False)
    paths=[Path(__file__).resolve(),args.uwvm,args.wasm,args.source,args.build_receipt,args.producer_receipt,
           ROOT/'tools/debug/dap_adapter.py',ROOT/'tools/debug/secure_server.py',Path(base.__file__),
           Path(live.__file__),Path(producers.__file__),ROOT/'test/0018.debugger/run_pretty_printer_matrix.py',
           Path(live.source_cli.__file__),Path(live.source_cli.metadata_cli.__file__)]
    pins={str(path):live.sha(path) for path in paths}
    result={'passed':False,'admission':proof,'pins_before':pins,'build':build,
            'source_packet_only':False,'actual_vscode_ui_qualified':False,
            'scope':'real stopped producer + UnixBroker/DAP command/config/registry semantics; separate from component throughput'}
    try:
        result['session']=session(args,args.out/'actual')
        result['passed']=result['session']['passed']
    except BaseException as error:
        result['error']=repr(error)
        raise
    finally:
        try:
            result['pins_after']={path:live.sha(Path(path)) for path in pins}
            result['passed']=result['passed'] and result['pins_after']==pins
            result['admission_after']=admission.admit()
        except BaseException as error:
            result['passed']=False
            result['final_guard_error']=repr(error)
            raise
        finally:
            (args.out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    live.require(result['passed'],'actual pretty command/config live qualification',result.get('session'))
    print('PASS actual pretty command/config live',len(result['session']['checks']),'checks',
          len(result['session']['retirement_transactions']),'actual DAP reference retirement transactions')
    return 0


if __name__=='__main__':
    raise SystemExit(main())
