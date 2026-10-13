#!/usr/bin/env python3
"""Real current GC array pages through original local and nested member paths."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import sys
import run_dap_current_broker as live
from run_dap_frame_scope_lifetime import Dispatch, ScheduledBroker


class MemberBroker(ScheduledBroker):
    def __init__(self, channel):
        super().__init__(channel); self.after_member = None
    def request(self, command):
        result = super().request(command)
        if command.startswith('members ') and self.after_member is not None:
            hook = self.after_member
            if hook(): self.after_member = None
        return result


def roots(client, thread):
    result = client.request('uwvm/wasmState', {'threadId':thread, 'selection':'locals', 'count':3})
    rows = result['body']['variables']
    live.require(len(rows) == 3 and rows[0]['value'] == '161', 'actual initialized GC fixture locals', result)
    live.require(rows[1].get('indexedVariables') == 161 and rows[2].get('indexedVariables') == 1 and rows[1].get('namedVariables') == 0, 'actual DAP GC cardinality hints', rows)
    array, box = rows[1]['variablesReference'], rows[2]['variablesReference']
    members = client.request('variables', {'variablesReference':box})['body']['variables']
    live.require(len(members) == 1 and members[0]['variablesReference'] > 0 and 'memoryReference' not in members[0], 'actual nested GC root', members)
    live.require(members[0].get('indexedVariables') == 161 and members[0].get('namedVariables') == 0, 'actual nested GC cardinality hint', members)
    return array, members[0]['variablesReference']


def verify(values, indices):
    indices = list(indices)
    live.require([v['name'] for v in values] == [f'[{i}]' for i in indices] and
                 [v['value'] for v in values] == [str(1000+i) for i in indices], 'actual original-index GC array page', values)
    live.require(all(v['variablesReference'] == 0 and 'memoryReference' not in v for v in values), 'GC page host memory capability', values)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--uwvm', required=True, type=Path)
    parser.add_argument('--wasm', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--policy', required=True, choices=('instruction','unwind'))
    parser.add_argument('--ros', action='store_true'); args = parser.parse_args()
    live.require(sys.platform == 'linux' and os.uname().machine == 'x86_64', 'Linux x86_64 required')
    args.out.mkdir(mode=0o700); root = Path(__file__).resolve().parents[2]
    spec = importlib.util.spec_from_file_location('actual_member_pages_dap', root/'tools/debug/dap_adapter.py')
    dap = importlib.util.module_from_spec(spec); spec.loader.exec_module(dap)
    paths = (args.uwvm,args.wasm,Path(__file__),Path(live.__file__),root/'tools/debug/dap_adapter.py',root/'tools/debug/secure_server.py',
        root/'test/0018.debugger/run_dap_frame_scope_lifetime.py',root/'test/0017.runtime/run_debug_source_step_cli.py',root/'test/0017.runtime/run_debug_source_inline_metadata_cli.py')
    pins = {str(p):live.sha(p) for p in paths}
    function, body = live.source_cli.metadata_cli.function(args.wasm,'spin')
    groups, cursor = live.source_cli.metadata_cli.u32(body,0,len(body))
    live.require(groups == 3, 'actual three typed local groups')
    for declaration in (b'\x01\x7f', b'\x01\x63\x00', b'\x01\x63\x01'):
        live.require(body[cursor:cursor+len(declaration)] == declaration, 'actual i32/array/box local types', body)
        cursor += len(declaration)
    loop = body.find(b'\x03\x40\x23\x01',cursor)
    live.require(loop > cursor and body.find(b'\x03\x40\x23\x01',loop+1) == -1, 'unique recurring GC fixture safepoint')
    target = loop+2-cursor
    mode = ['-Raot'] if args.ros else ['-Rcc','jit','-Rcm','full']
    vm = ['--wasm-feature-enable-gc','--wasm-feature-enable-function-references','--wasm-feature-enable-reference-types',
          '-m','run',*mode,'-Rct','0','-Rllvm-call-stack',args.policy,'-Rllvm-exception-dispatch','native-unwind','-Rllvm-cache-path','disable','--run',str(args.wasm)]
    server = live.BrokerSession(args.uwvm,root/'tools/debug/secure_server.py',root/'tools/debug/dap_adapter.py',vm,args.out)
    broker = None; record = {'passed':False,'pins':pins,'pages':[]}
    try:
        broker = MemberBroker(dap.UnixBroker(str(server.directory))); client = Dispatch(dap,broker)
        current, point = live.breakpoint_begin(client,function,target); client.evaluate(f'delete {point}')
        thread = current['thread']; array, nested = roots(client,thread)
        for ref, route in ((array,'direct-local'),(nested,'nested-field')):
            for first,count in ((129,8),(63,80),(159,64),(161,4),((1<<64)-1,1)):
                reply = client.request('variables',{'variablesReference':ref,'start':first,'count':count,'filter':'indexed'})
                verify(reply['body']['variables'],range(first,min(first+count,161)))
                record['pages'].append({'route':route,'start':first,'count':count,'response':reply})
            reply = client.request('variables',{'variablesReference':ref,'start':158,'count':0})
            verify(reply['body']['variables'],range(158,161))
            reply = client.request('variables',{'variablesReference':ref})
            verify(reply['body']['variables'],range(161))
            record.setdefault('all_members',{})[route] = reply
            begin = len(broker.commands); named = client.request('variables',{'variablesReference':ref,'filter':'named'})
            live.require(named['body']['variables'] == [] and not any(r['command'].startswith('members ') for r in broker.commands[begin:]),'indexed-only named filter')
        external = broker.request(f'step wasm {thread}')
        record['stale_after_external'] = {'step':external, 'direct':client.request('variables',{'variablesReference':array},success=False),
            'nested':client.request('variables',{'variablesReference':nested},success=False)}
        array, nested = roots(client,thread)
        copied = 0
        def advance_after_copy():
            nonlocal copied
            copied += 1
            if copied == 2:
                broker.after_member = None
                record['mid_copy_external_step'] = broker.request(f'step wasm {thread}')
                return True
            return False
        broker.after_member = advance_after_copy
        record['mid_copy_refusal'] = client.request('variables',{'variablesReference':nested,'start':63,'count':80},success=False)
        live.require('body' not in record['mid_copy_refusal'], 'partial GC page published across real external stop')
        array, nested = roots(client,thread)
        fresh = client.request('variables',{'variablesReference':nested,'start':129,'count':8})
        verify(fresh['body']['variables'],range(129,137)); record['fresh_after_mid_copy'] = fresh
        client.evaluate(f'set wasm global 0 0 {thread} bits i32 0'); result = client.evaluate('continue')
        for _ in range(20):
            if b'guest exited: 0' in result: break
            result = client.evaluate('wait')
        live.require(b'guest exited: 0' in result,'actual GC fixture natural logical exit',result)
        record.update(passed=True,actual_guest_exit=result.decode(),total_members=161)
    finally:
        if 'client' in locals(): record['requests'] = client.requests
        if broker is not None: record['broker_commands'] = broker.commands; broker.close()
        try:
            if record['passed']:
                try: server.child.wait(timeout=10)
                except BaseException as error:
                    record.update(passed=False,natural_exit_wait_error=repr(error)); raise
        finally:
            try: record['cleanup'] = server.close()
            finally:
                record['pins_after'] = {n:live.sha(Path(n)) for n in pins}
                live.require(record['pins_after'] == pins,'immutable actual GC page inputs changed')
                (args.out/'wasm-member-pages.json').write_text(json.dumps(record,indent=2)+'\n')
    print(f'run_dap_wasm_member_pages: PASS policy={args.policy} direct/nested-members=161 natural-exit=0')
    return 0


if __name__ == '__main__': raise SystemExit(main())
