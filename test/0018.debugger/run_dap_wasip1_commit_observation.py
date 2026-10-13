#!/usr/bin/env python3
"""Real WASIp1 checkpoint rollback and post-ACK channel-loss injection on Linux."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import sys
import time
import run_dap_current_broker as live
from run_dap_frame_scope_lifetime import Dispatch, ScheduledBroker
from run_dap_wasm_scope_pages import recurring_stop
from run_dap_wasm_memory_writes import read, retired


class CommitBroker(ScheduledBroker):
    def __init__(self, channel):
        super().__init__(channel); self.after_edit = None; self.before_edit = None; self.attempts = []

    def request(self, command):
        self.attempts.append(command)
        editing = command.startswith(('set wasip1 ', 'unset wasip1 '))
        if editing and self.before_edit: self.before_edit()
        reply = super().request(command)
        if editing and self.after_edit:
            hook, self.after_edit = self.after_edit, None
            hook(reply)
        return reply


def exported_spin(path):
    metadata = live.source_cli.metadata_cli
    imported = None; target = None; bodies = None
    for kind, payload in metadata.sections(path):
        if kind == 2:
            imported, at = metadata.u32(payload, 0, len(payload))
            live.require(imported == 4, 'actual four WASIp1 function imports')
            names = []
            for _ in range(imported):
                pair = []
                for _ in range(2):
                    size, at = metadata.u32(payload, at, len(payload)); live.require(size <= len(payload)-at, 'actual import bounds')
                    pair.append(payload[at:at+size]); at += size
                live.require(at < len(payload) and payload[at] == 0, 'actual imported function kind')
                _, at = metadata.u32(payload, at+1, len(payload)); names.append(pair)
            live.require(at == len(payload) and names == [[b'wasi_snapshot_preview1',n] for n in (b'fd_pread',b'fd_pwrite',b'fd_seek',b'fd_tell')], 'actual WASIp1 imports', names)
        elif kind == 7:
            count, at = metadata.u32(payload, 0, len(payload))
            for _ in range(count):
                size, at = metadata.u32(payload, at, len(payload)); live.require(size < len(payload)-at, 'actual export bounds')
                name = payload[at:at+size]; at += size; tag = payload[at]
                index, at = metadata.u32(payload, at+1, len(payload))
                if name == b'spin' and tag == 0:
                    live.require(target is None, 'unique spin export'); target = index
            live.require(at == len(payload), 'actual export extent')
        elif kind == 10:
            count, at = metadata.u32(payload, 0, len(payload)); bodies = []
            for _ in range(count):
                size, at = metadata.u32(payload, at, len(payload)); live.require(size <= len(payload)-at, 'actual body bounds')
                bodies.append(payload[at:at+size]); at += size
            live.require(at == len(payload), 'actual code extent')
    live.require(imported == 4 and target is not None and bodies is not None and imported <= target < imported+len(bodies), 'actual exported local function')
    body = bodies[target-imported]; groups, at = metadata.u32(body, 0, len(body))
    live.require(groups == 1 and body[at:at+2] == b'\x01\x7f', 'actual one i32 local'); at += 2
    tail = body.find(b'\x23\x00\x0d\x00', at)
    live.require(tail > at and body.find(b'\x23\x00\x0d\x00',tail+1) == -1, 'unique recurring final guest safepoint')
    return target, tail-at


def state_rows(client, selection):
    reply = client.request('uwvm/wasip1State', {'moduleId':0,'selection':selection,'count':64})['body']
    live.require(reply['available'] and not reply['more'], 'actual finite WASIp1 state', reply)
    return reply['variables']


def labels(client):
    current = live.actual_location(client)
    frames = client.request('stackTrace',{'threadId':current['thread'],'levels':1})['body']['stackFrames']
    live.require(len(frames) == 1 and frames[0]['name'].startswith('Wasm frame 0 '),'actual cooperative frame',frames)
    frame = frames[0]; scopes = client.request('scopes',{'frameId':frame['id']})['body']['scopes']
    locals_scope = [r for r in scopes if r['name'] == 'Wasm locals']
    live.require(len(locals_scope) == 1,'actual ordinary locals scope',scopes)
    reference = locals_scope[0]['variablesReference']
    values = client.request('variables',{'variablesReference':reference})['body']['variables']
    live.require(len(values) == 1 and values[0]['value'] == 'i32=17','actual local marker',values)
    content = client.request('source',{'sourceReference':frame['id']})['body']['content']
    live.require(content.startswith(f";; module 0 function {current['function']}\n"),'actual selected source label',content)
    old = frame['id'],reference,frame['id']
    wasi = [r['variablesReference'] for r in scopes if r['name'].startswith('WASIp1 ')]
    live.require(len(wasi) == 4, 'actual four WASIp1 references', scopes)
    return old, wasi


def stale(client, old):
    responses = retired(client, old[0])
    for ref in old[1]:
        response = client.request('variables',{'variablesReference':ref},success=False)
        live.require('body' not in response, 'old WASIp1 reference survived operation', response); responses.append(response)
    return responses


def edit(client, operation, **fields):
    return client.request('uwvm/wasip1Edit',{'moduleId':0,'operation':operation,**fields})


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--uwvm',required=True,type=Path); p.add_argument('--wasm',required=True,type=Path)
    p.add_argument('--out',required=True,type=Path); p.add_argument('--policy',required=True,choices=('instruction','unwind'))
    p.add_argument('--ros',action='store_true'); p.add_argument('--baseline',action='store_true')
    args = p.parse_args(); live.require(sys.platform == 'linux' and os.uname().machine == 'x86_64','Linux x86_64 required')
    args.out.mkdir(mode=0o700); root = Path(__file__).resolve().parents[2]
    spec = importlib.util.spec_from_file_location('actual_wasip1_commit_dap',root/'tools/debug/dap_adapter.py')
    dap = importlib.util.module_from_spec(spec); spec.loader.exec_module(dap)
    paths = (args.uwvm,args.wasm,Path(__file__),Path(live.__file__),root/'tools/debug/dap_adapter.py',root/'tools/debug/secure_server.py',
             root/'test/0018.debugger/run_dap_frame_scope_lifetime.py',root/'test/0018.debugger/run_dap_wasm_scope_pages.py',
             root/'test/0018.debugger/run_dap_wasm_memory_writes.py',root/'test/0017.runtime/run_debug_source_step_cli.py',
             root/'test/0017.runtime/run_debug_source_inline_metadata_cli.py')
    pins = {str(f):live.sha(f) for f in paths}; function,target = exported_spin(args.wasm)
    mode = ['-Raot'] if args.ros else ['-Rcc','jit','-Rcm','full']
    vm = ['-m','run',*mode,'-Rct','0','-Rllvm-call-stack',args.policy,'-Rllvm-exception-dispatch','native-unwind','-Rllvm-cache-path','disable','--run',str(args.wasm)]
    server = live.BrokerSession(args.uwvm,root/'tools/debug/secure_server.py',root/'tools/debug/dap_adapter.py',vm,args.out)
    broker = None; record = {'passed':False,'baseline':args.baseline,'pins':pins,'faults':[],'cache_retirements':[]}
    try:
        time.sleep(3); live.require(server.child.poll() is None,'actual fixture startup',server.child.poll())
        broker = CommitBroker(dap.UnixBroker(str(server.directory))); client = Dispatch(dap,broker); client.adapter.step_level = 'wasm'
        client.request('initialize',{'supportsInvalidatedEvent':True})
        current,point = live.breakpoint_begin(client,function,target); client.evaluate(f'delete {point}')
        edit(client,'setEnvironment',name='R47',value='saved')
        created = edit(client,'createFile',valueHex='4100427f'); fd = created['body']['descriptor']
        rows = state_rows(client,'fds'); owner = next(r for r in rows if r['descriptor'] == fd)
        rights = {'descriptor':fd,'expectedBase':owner['rightsBase'],'expectedInheriting':owner['rightsInheriting']}
        alias = edit(client,'duplicateDescriptor',**rights)['body']['descriptor']
        client.evaluate(f"set wasm global 0 2 {current['thread']} bits i32 {fd:x}")
        client.evaluate(f"set wasm global 0 4 {current['thread']} bits i32 5")
        current = recurring_stop(client,function,target)
        def probe(expected,cursor):
            location = recurring_stop(client,function,target)
            data = read(client,0,128,4); actual_cursor = int.from_bytes(read(client,0,64,8),'little')
            errors = [int.from_bytes(read(client,0,a,4),'little') for a in (32,48)]
            live.require(data == bytes.fromhex(expected) and actual_cursor == cursor and errors == [0,0], 'actual guest pread/tell rollback', (data.hex(),actual_cursor,errors))
            return {'bytes':data.hex(),'cursor':actual_cursor,'errors':errors,'location':location}
        record['before_save'] = probe('4100427f',5)
        def fault(operation, fields, applied):
            old = labels(client); entry = {'operation':operation,'arguments':fields}; first = len(broker.attempts)
            def before():
                clean = client.adapter.stop_key is None and all(not getattr(client.adapter,n) for n in
                    ('frames','scopes','native_code_refs','wasip1_scope_stops','wasm_object_views','deep_wasm_paths'))
                live.require(clean,'copied labels remained before actual WASIp1 edit'); record['cache_retirements'].append(clean)
            def after(reply):
                entry['actual_ack'] = reply; broker.channel.close()
            broker.before_edit = before; broker.after_edit = after
            entry['response'] = client.request('uwvm/wasip1Edit',{'moduleId':0,'operation':operation,**fields},success=None)
            broker.before_edit = None; entry['attempts'] = broker.attempts[first:]
            live.require(len(entry['attempts']) == 3 and entry['attempts'][0] == entry['attempts'][-1] == 'status','post-ACK failure retry or wrong admission',entry)
            live.require(f'applied={int(applied)} ' in entry['actual_ack'],'actual native outcome differs',entry)
            broker.channel = dap.UnixBroker(str(server.directory))
            if args.baseline:
                live.require(not entry['response']['success'] and 'body' not in entry['response'],'actual old post-ACK bug absent',entry)
                record['baseline_reproduced'] = entry
                proof = edit(client,'restoreCheckpoint',slot=2,resourceMode='bindings')
                live.require(proof['body']['applied'],'actual saved checkpoint absent behind failed old response',proof)
                record['baseline_restore_proof'] = proof
            else:
                response = entry['response']; body = response.get('body',{})
                live.require(response['success'] and body.get('applied') is applied and body.get('available') is applied
                             and not body['stopCurrent'] and not body['stopObservationAvailable'] and 'outcome is confirmed' in body['stopObservationDiagnostic'],
                             'confirmed native operation erased by follow-up channel failure',entry)
                messages = client.requests[-1]['messages']
                live.require([m['type'] for m in messages] == (['response','event'] if applied else ['response']), 'operation refresh ordering',messages)
                if applied: live.require(messages[1]['event'] == 'invalidated' and messages[1]['body'] == {'areas':['stacks','variables']},'negotiated refresh absent',messages)
                entry['stale'] = stale(client,old); record['faults'].append(entry)
            return entry
        fault('saveCheckpoint',{'slot':2},True)
        if not args.baseline:
            edit(client,'setEnvironment',name='R47',value='changed')
            current = live.actual_location(client)
            client.evaluate(f"set wasm global 0 3 {current['thread']} bits i32 1")
            client.evaluate(f"set wasm global 0 4 {current['thread']} bits i32 9")
            record['changed_file'] = probe('ff800100',9)
            current = live.actual_location(client)
            client.evaluate(f"set wasm global 0 5 {current['thread']} bits i32 aa")
            recurring_stop(client,function,target)
            live.require(read(client,0,512,1) == b'\xaa','actual guest-written Wasm marker absent')
            for selected in (fd,alias):
                response = edit(client,'closeDescriptor',**{**rights,'descriptor':selected})
                live.require(response['body']['applied'],'actual captured guest binding close',response)
            live.require(not any(r['descriptor'] in (fd,alias) for r in state_rows(client,'fds')),'closed guest aliases remain')
            fault('restoreCheckpoint',{'slot':2,'resourceMode':'bindings'},True)
            record['restored_file'] = probe('4100427f',5)
            restored = state_rows(client,'fds'); env = state_rows(client,'env')
            live.require(all(any(r['descriptor'] == selected and r['rightsBase'] == rights['expectedBase'] and r['rightsInheriting'] == rights['expectedInheriting'] for r in restored) for selected in (fd,alias)), 'actual alias/rights restoration',restored)
            live.require(any(r['value'] == '"R47=saved"' for r in env),'actual environment rollback',env)
            record.update(restored_descriptors=restored,restored_environment=env,wasm_byte_after_wasi_restore=read(client,0,512,1).hex())
            live.require(record['wasm_byte_after_wasi_restore'] == 'aa','WASIp1 restore silently rewound Wasm memory')
            current = live.actual_location(client); client.evaluate(f"set wasm global 0 2 {current['thread']} bits i32 {alias:x}")
            client.evaluate(f"set wasm global 0 4 {current['thread']} bits i32 7")
            record['alias_seek'] = probe('4100427f',7)
            current = live.actual_location(client); client.evaluate(f"set wasm global 0 2 {current['thread']} bits i32 {fd:x}")
            record['original_alias_cursor'] = probe('4100427f',7)
            refusal = fault('restoreCheckpoint',{'slot':2,'resourceMode':'strict'},False)
            live.require(refusal['response']['body']['status'] == 'resource rollback unavailable','strict external-resource refusal',refusal)
            record['after_strict_refusal'] = probe('4100427f',7)
            fault('dropCheckpoint',{'slot':2},True)
            missing = edit(client,'restoreCheckpoint',slot=2,resourceMode='bindings')
            live.require(not missing['body']['applied'] and missing['body']['status'] == 'entry not found','actual dropped slot resurrected',missing)
            record['dropped_slot_refusal'] = missing
            old = labels(client); before = {r['descriptor'] for r in state_rows(client,'fds')}; lost = {}; first = len(broker.attempts)
            def lose(reply):
                lost['actual_ack'] = reply; broker.channel.close(); raise ConnectionError('injected adapter ACK loss after genuine commit')
            broker.after_edit = lose
            lost['response'] = client.request('uwvm/wasip1Edit',{'operation':'createFile','moduleId':0,'valueHex':'0041ff'},success=False)
            live.require('unconfirmed' in lost['response']['message'] and 'body' not in lost['response'] and client.adapter.stop_key is None,'lost actual ACK promoted to known outcome',lost)
            lost['attempts'] = broker.attempts[first:]; live.require(len(lost['attempts']) == 2,'lost actual ACK retried or followed up',lost)
            broker.channel = dap.UnixBroker(str(server.directory)); after = {r['descriptor'] for r in state_rows(client,'fds')}
            live.require(len(after-before) == 1 and before <= after and 'applied=1 ' in lost['actual_ack'],'actual create behind lost ACK missing or duplicated',lost)
            lost['new_descriptor'] = next(iter(after-before)); lost['stale'] = stale(client,old); record['lost_ack_injection'] = lost
            resumed = {}
            def resume(reply):
                resumed['actual_ack'] = reply; resumed['continue'] = broker.request('continue')
            broker.after_edit = resume
            resumed['response'] = edit(client,'setEnvironment',name='R47',value='resumed')
            live.require(resumed['response']['body']['applied'] and not resumed['response']['body']['stopCurrent']
                         and resumed['response']['body']['stopObservationAvailable'],'known edit erased by real independent resume',resumed)
            current,point = live.breakpoint_begin(client,function,target); client.evaluate(f'delete {point}')
            resumed['actual_environment'] = state_rows(client,'env')
            live.require(any(r['value'] == '"R47=resumed"' for r in resumed['actual_environment']),'actual resumed edit absent',resumed)
            record['commit_then_resume'] = resumed
            record['native_step'] = client.evaluate(f"step asm {current['thread']}").decode()
            live.require(dap.parse_status(record['native_step'])[2][0].get('native_pc'),'actual ASM trap absent')
            first = len(broker.attempts)
            record['native_refusal'] = client.request('uwvm/wasip1Edit',{'operation':'saveCheckpoint','slot':3},success=False)
            live.require(broker.attempts[first:] == ['status'] and 'body' not in record['native_refusal'],'ASM trap authorized checkpoint operation')
            current = recurring_stop(client,function,target)
        else: current = live.actual_location(client)
        client.evaluate(f"set wasm global 0 0 {current['thread']} bits i32 0")
        result = client.evaluate('continue')
        for _ in range(20):
            if b'guest exited: 0' in result: break
            result = client.evaluate('wait')
        live.require(b'guest exited: 0' in result,'actual fixture natural exit',result)
        record.update(passed=True,actual_guest_exit=result.decode())
    finally:
        if 'client' in locals(): record['requests'] = client.requests
        if broker is not None:
            record.update(broker_commands=broker.commands,broker_attempts=broker.attempts); broker.after_edit=broker.before_edit=None; broker.close()
        try:
            if record['passed']:
                try: server.child.wait(timeout=10)
                except BaseException as error:
                    record.update(passed=False,natural_exit_wait_error=repr(error)); raise
        finally:
            try: record['cleanup'] = server.close()
            finally:
                record['pins_after'] = {n:live.sha(Path(n)) for n in pins}
                live.require(record['pins_after'] == pins,'immutable actual checkpoint inputs changed')
                (args.out/'wasip1-commit-observation.json').write_text(json.dumps(record,indent=2)+'\n')
    print(f'run_dap_wasip1_commit_observation: PASS policy={args.policy} baseline={args.baseline} faults={len(record["faults"])} natural-exit=0')
    return 0


if __name__ == '__main__': raise SystemExit(main())
