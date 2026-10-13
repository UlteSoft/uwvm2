#!/usr/bin/env python3
"""Actual bounded DAP guest memory writes, old references and host-only fault injection."""
import argparse
import base64
import importlib.util
import json
import os
from pathlib import Path
import sys
import time
import run_dap_current_broker as live
from run_dap_frame_scope_lifetime import Dispatch, ScheduledBroker
from run_dap_wasm_scope_pages import recurring_stop


class WriteBroker(ScheduledBroker):
    def __init__(self, channel):
        super().__init__(channel); self.before_write = None; self.after_ack = None

    def request(self, command):
        if command.startswith('set wasm memory ') and self.before_write:
            self.before_write()
        reply = super().request(command)
        if command.startswith('set wasm memory ') and self.after_ack:
            hook, self.after_ack = self.after_ack, None
            hook(reply)
        return reply


def arguments(memory, address, data, offset=0, module=0, **extra):
    return {'memoryReference':f'wasm-memory:{module}:{memory}:{address}', 'offset':offset,
            'data':base64.b64encode(data).decode(), **extra}


def read(client, memory, address, count):
    body = client.request('readMemory', {'memoryReference':f'wasm-memory:0:{memory}:{address}', 'count':count})['body']
    live.require(body['address'] == str(address) and body['unreadableBytes'] == 0, 'actual guest memory read address', body)
    return base64.b64decode(body['data'], validate=True)


def handles(client, thread):
    frames = client.request('stackTrace', {'threadId':thread, 'startFrame':0, 'levels':1})['body']['stackFrames']
    live.require(len(frames) == 1 and frames[0]['name'].startswith('Wasm frame 0 '), 'actual canonical current Wasm frame', frames)
    frame = frames[0]; rows = client.request('scopes', {'frameId':frame['id']})['body']['scopes']
    locals_scope = [row for row in rows if row['name'] == 'Wasm locals']
    live.require(len(locals_scope) == 1, 'actual ordinary Wasm locals scope', rows)
    reference = locals_scope[0]['variablesReference']
    values = client.request('variables', {'variablesReference':reference})['body']['variables']
    live.require(len(values) == 1 and values[0]['value'] == 'i32=17', 'actual stopped local marker', values)
    # The adapter's current Wasm source route accepts the same selected opaque
    # frame ID and returns a label. Query it now before testing its retirement;
    # do not claim a WAT listing or an IDE-advertised source location.
    label = client.request('source', {'sourceReference':frame['id']})['body']['content']
    live.require(label.startswith(';; module 0 function 0\n'), 'actual current source label route', label)
    return frame['id'], reference, frame['id']


def retired(client, old):
    replies = []
    for command, key, reference in (('scopes', 'frameId', old[0]), ('variables', 'variablesReference', old[1]), ('source', 'sourceReference', old[2])):
        reply = client.request(command, {key:reference}, success=False)
        live.require('body' not in reply, 'retired pre-write reference published DATA', reply)
        replies.append(reply)
    return replies


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--uwvm', required=True, type=Path); parser.add_argument('--wasm', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path); parser.add_argument('--policy', required=True, choices=('instruction', 'unwind'))
    parser.add_argument('--ros', action='store_true'); parser.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    live.require(sys.platform == 'linux' and os.uname().machine == 'x86_64', 'Linux x86_64 required')
    args.out.mkdir(mode=0o700); root = Path(__file__).resolve().parents[2]
    spec = importlib.util.spec_from_file_location('actual_memory_writes_dap', root/'tools/debug/dap_adapter.py')
    dap = importlib.util.module_from_spec(spec); spec.loader.exec_module(dap)
    paths = (args.uwvm, args.wasm, Path(__file__), Path(live.__file__), root/'tools/debug/dap_adapter.py', root/'tools/debug/secure_server.py',
             root/'test/0018.debugger/run_dap_frame_scope_lifetime.py', root/'test/0018.debugger/run_dap_wasm_scope_pages.py',
             root/'test/0017.runtime/run_debug_source_step_cli.py', root/'test/0017.runtime/run_debug_source_inline_metadata_cli.py')
    pins = {str(p):live.sha(p) for p in paths}
    function, body = live.source_cli.metadata_cli.function(args.wasm, 'spin')
    groups, cursor = live.source_cli.metadata_cli.u32(body, 0, len(body))
    live.require(groups == 1 and body[cursor:cursor+2] == b'\x01\x7f', 'actual one i32 local declaration'); cursor += 2
    loop = body.find(b'\x03\x40\x23\x01', cursor)
    live.require(loop > cursor and body.find(b'\x03\x40\x23\x01', loop+1) == -1, 'unique actual recurring safepoint')
    target = loop+2-cursor; mode = ['-Raot'] if args.ros else ['-Rcc', 'jit', '-Rcm', 'full']
    vm = ['--wasm-feature-enable-memory64', '--wasm-feature-enable-multi-memory', '--wasm-feature-enable-bulk-memory',
          '-m', 'run', *mode, '-Rct', '0', '-Rllvm-call-stack', args.policy, '-Rllvm-exception-dispatch', 'native-unwind',
          '-Rllvm-cache-path', 'disable', '--run', str(args.wasm)]
    server = live.BrokerSession(args.uwvm, root/'tools/debug/secure_server.py', root/'tools/debug/dap_adapter.py', vm, args.out)
    broker = None; record = {'passed':False, 'pins':pins, 'writes':[], 'range_refusals':[], 'invalid':[], 'native_refusals':[], 'cache_retirements':[]}
    try:
        time.sleep(3); live.require(server.child.poll() is None, 'memory fixture exited before debugger startup', server.child.poll())
        broker = WriteBroker(dap.UnixBroker(str(server.directory))); client = Dispatch(dap, broker); client.adapter.step_level = 'wasm'
        init = client.request('initialize', {'supportsMemoryEvent':True, 'supportsInvalidatedEvent':True})
        record['initialize'] = init
        if not args.baseline:
            live.require(init['body'].get('supportsWriteMemoryRequest') is True, 'actual write capability missing', init)
            before = len(broker.commands)
            record['running_refusal'] = client.request('writeMemory', arguments(0,17,b'\x00'), success=False)
            live.require([r['command'] for r in broker.commands[before:]] == ['status'], 'running write reached mutation')
        current, point = live.breakpoint_begin(client, function, target); client.evaluate(f'delete {point}')
        if args.baseline:
            before_bytes = read(client,0,17,4); first = len(broker.commands)
            reply = client.request('writeMemory', arguments(0,17,b'\x00\x01\x7f\xff'), success=None)
            after_bytes = read(client,0,17,4)
            record['baseline'] = {'response':reply, 'before':before_bytes.hex(), 'after':after_bytes.hex(), 'commands':broker.commands[first:]}
            live.require(reply['success'], 'actual old adapter writeMemory is unavailable', record['baseline'])
        for memory in (0,1):
            for i,count in enumerate((1,2,3,128,255,256)):
                current = recurring_stop(client,function,target); thread = current['thread']; old = handles(client,thread)
                address = 64+i*512; normalized = address-47; data = bytes((n*73+count+memory)&255 for n in range(count))
                def before_write():
                    clean = client.adapter.stop_key is None and all(not getattr(client.adapter,name) for name in
                        ('frames','scopes','wasm_local_scope_stops','source_scope_stops','wasm_scope_stops','wasip1_scope_stops','wasm_object_views','native_code_refs','deep_wasm_paths'))
                    live.require(clean, 'actual caches survived before guest commit'); record['cache_retirements'].append(True)
                broker.before_write = before_write; first = len(broker.commands)
                reply = client.request('writeMemory', arguments(memory,address,data,-47,allowPartial=False))
                broker.before_write = None; commands = broker.commands[first:]
                live.require([r['command'] for r in commands] == ['status',f'set wasm memory 0 {memory} {thread} {normalized} bytes {data.hex()}'], 'write was split/replayed or followed by a fragile copy', commands)
                live.require(reply['body'] == {'offset':-47, 'bytesWritten':count} and client.adapter.stop_key is None, 'actual committed write response', reply)
                wire = client.requests[-1]['messages']
                live.require(wire[0]['type'] == 'response' and [(m['event'],m['body']) for m in wire[1:]] ==
                    [('memory',{'memoryReference':f'wasm-memory:0:{memory}:{address}', 'offset':-47, 'count':count}),
                     ('invalidated',{'areas':['stacks','variables']})], 'actual negotiated write event ordering', wire)
                after = live.actual_location(client)
                live.require(after['stop'] > current['stop'] and 'applied=1' in commands[-1]['reply'] and f'address-bytes={4 if memory==0 else 8}' in commands[-1]['reply'], 'actual native commit/width', commands)
                stale = retired(client,old); actual = read(client,memory,normalized,count)
                live.require(actual == data, 'actual binary guest bytes differ from DAP payload', (actual.hex(),data.hex()))
                record['writes'].append({'memory':memory,'count':count,'address':address,'offset':-47,'data':data.hex(),'before':current,'after':after,'response':reply,'actual':actual.hex(),'stale':stale,'commands':commands,'messages':wire})
        for memory in (0,1):
            end = client.request('writeMemory',arguments(memory,131071,b'\xfe'))
            live.require(end['body']['bytesWritten'] == 1 and read(client,memory,131071,1) == b'\xfe', 'last actual guest byte write', end)
            current = live.actual_location(client); old = handles(client,current['thread']); first = len(client.events)
            zero = client.request('writeMemory',arguments(memory,131072,b''))
            live.require(zero['body'] == {'offset':0,'bytesWritten':0} and len(client.events) == first and live.actual_location(client)['stop'] == current['stop'], 'empty write changed stop/events', zero)
            client.request('scopes',{'frameId':old[0]})
            record.setdefault('ends',[]).append({'memory':memory,'last_byte':end,'zero':zero})
            for address,data in ((131071,b'\x00\x01'),(131000,bytes(range(256))),(131072,b'\x00'),(131073,b'')):
                before_data = read(client,memory,131000,72); before = live.actual_location(client); first = len(broker.commands)
                reply = client.request('writeMemory',arguments(memory,address,data),success=False)
                commands = broker.commands[first:]; after = live.actual_location(client); actual = read(client,memory,131000,72)
                live.require('body' not in reply and actual == before_data and after['stop'] == before['stop'], 'failed actual range changed guest bytes/stop', (reply,commands))
                live.require(sum(r['command'].startswith('set wasm memory ') for r in commands) == (1 if data else 0), 'failed range split or retried write', commands)
                record['range_refusals'].append({'memory':memory,'address':address,'count':len(data),'before':before_data.hex(),'after':actual.hex(),'response':reply,'commands':commands})
        for module,memory in ((999,0),(0,2)):
            first = len(broker.commands); reply = client.request('writeMemory',arguments(memory,17,b'\x00',module=module),success=False)
            live.require('body' not in reply and sum(r['command'].startswith('set wasm memory ') for r in broker.commands[first:]) == 1, 'unavailable selector was replayed', reply)
            record['range_refusals'].append({'module':module,'memory':memory,'response':reply})
        invalid = [{'data':'AR=='},{'data':'AQ==\n'},{'data':base64.b64encode(bytes(257)).decode()}, {'offset':True},{'allowPartial':True},
                   {'memoryReference':'wasm-memory:0:0:18446744073709551615'},{'memoryReference':'$rsp'},{'offset':-(1<<53)}]
        for extra in invalid:
            entry = arguments(0,17,b'\x00'); entry.update(extra); first = len(broker.commands)
            reply = client.request('writeMemory',entry,success=False)
            live.require('body' not in reply and len(broker.commands) == first, 'invalid write caused I/O', reply); record['invalid'].append(reply)
        current = recurring_stop(client,function,target); thread = current['thread']
        record['native_step'] = client.evaluate(f'step asm {thread}').decode(); native = dap.parse_status(record['native_step'])
        live.require(native[0] == 'stopped' and native[2][0].get('native_pc'), 'actual native trap missing', record['native_step'])
        first = len(broker.commands); reply = client.request('writeMemory',arguments(0,17,b'\x00'),success=False)
        live.require([r['command'] for r in broker.commands[first:]] == ['status'] and 'body' not in reply, 'native trap authorized guest/host write', reply)
        record['native_refusals'].append(reply)
        frame = client.request('stackTrace',{'threadId':thread,'levels':1})['body']['stackFrames'][0]
        for reference in (frame['instructionPointerReference'],native[2][0]['native_pc']):
            first = len(broker.commands); reply = client.request('writeMemory',{'memoryReference':reference,'data':'AA=='},success=False)
            live.require(len(broker.commands) == first and 'body' not in reply, 'native code/display/PC granted memory mutation', reply)
            record['native_refusals'].append(reply)
        current = recurring_stop(client,function,target); first = len(broker.commands)
        resumed = {'before':current}
        def resume_after_ack(packet):
            resumed['ack'] = packet; resumed['continue'] = broker.request('continue')
        broker.after_ack = resume_after_ack
        resumed['response'] = client.request('writeMemory',arguments(0,4000,b'\x00\x01\x7f\xff'))
        resumed['commands'] = broker.commands[first:]; resumed['status_after'] = broker.request('status')
        live.require(resumed['response']['body']['bytesWritten'] == 4 and resumed['status_after'] == 'running\n' and client.adapter.stop_key is None,
                     'acknowledged write was erased by independent actual resume', resumed)
        live.require([r['command'] for r in resumed['commands']] == ['status','set wasm memory 0 0 1 4000 bytes 00017fff','continue'], 'known committed write was retried or post-read', resumed)
        current,point = live.breakpoint_begin(client,function,target); client.evaluate(f'delete {point}')
        resumed['actual'] = read(client,0,4000,4).hex(); live.require(resumed['actual'] == '00017fff','actual committed-before-resume bytes',resumed)
        record['commit_then_resume'] = resumed
        # Fault injection closes the real authenticated channel only AFTER its
        # real commit reply was received. The VM effect is genuine; simulated
        # adapter reply loss is explicitly separate from kernel/network faults.
        current = live.actual_location(client); old = handles(client,current['thread']); lost = {'before':current}; first = len(broker.commands)
        def lose_real_reply(packet):
            lost['actual_ack'] = packet; broker.channel.close()
            raise ConnectionError('injected adapter reply loss after actual committed broker reply')
        broker.after_ack = lose_real_reply
        lost['response'] = client.request('writeMemory',arguments(1,4000,b'\xff\x80\x01\x00'),success=False)
        lost['commands'] = broker.commands[first:]
        live.require('unconfirmed' in lost['response']['message'] and 'body' not in lost['response'] and client.adapter.stop_key is None
                     and len(lost['commands']) == 2, 'lost actual commit replayed or exposed old references', lost)
        broker.channel = dap.UnixBroker(str(server.directory))
        lost['after'] = live.actual_location(client); lost['actual'] = read(client,1,4000,4).hex(); lost['stale'] = retired(client,old)
        live.require(lost['after']['stop'] > current['stop'] and lost['actual'] == 'ff800100' and 'applied=1' in lost['actual_ack'], 'actual commit absent behind injected loss', lost)
        recovery = client.request('writeMemory',arguments(1,4000,b'\xa7\xa7\xa7\xa7'))
        live.require(read(client,1,4000,4) == bytes([167])*4 and recovery['body']['bytesWritten'] == 4, 'fresh recovery write/read failed', recovery)
        lost['recovery'] = recovery; record['lost_reply_injection'] = lost
        current = live.actual_location(client); client.evaluate(f"set wasm global 0 0 {current['thread']} bits i32 0")
        result = client.evaluate('continue')
        for _ in range(20):
            if b'guest exited: 0' in result: break
            result = client.evaluate('wait')
        live.require(b'guest exited: 0' in result, 'actual write fixture natural exit', result)
        record.update(passed=True,actual_guest_exit=result.decode())
    finally:
        if 'client' in locals(): record['requests'] = client.requests
        if broker is not None:
            record['broker_commands'] = broker.commands; broker.before_write = broker.after_ack = None; broker.close()
        try:
            if record['passed']:
                try: server.child.wait(timeout=10)
                except BaseException as error:
                    record.update(passed=False,natural_exit_wait_error=repr(error)); raise
        finally:
            try: record['cleanup'] = server.close()
            finally:
                record['pins_after'] = {n:live.sha(Path(n)) for n in pins}
                live.require(record['pins_after'] == pins,'immutable actual memory write inputs changed')
                (args.out/'wasm-memory-writes.json').write_text(json.dumps(record,indent=2)+'\n')
    print(f'run_dap_wasm_memory_writes: PASS policy={args.policy} writes<=256 memory32+memory64 atomic-range stale=39 acknowledged-resume+injected-loss natural-exit=0')
    return 0


if __name__ == '__main__': raise SystemExit(main())
