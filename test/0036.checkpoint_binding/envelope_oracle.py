#!/usr/bin/env python3
"""Independent file/identity/state endian oracle; no production encoder or VM."""
from __future__ import annotations
import hashlib
import pathlib
import struct
import sys

def digest(data): return hashlib.sha256(data).digest()
def label(text): return digest(text.encode('ascii'))
def uleb(value):
    out=bytearray()
    while True:
        byte=value&127; value >>= 7; out.append(byte | (128 if value else 0))
        if not value: return bytes(out)
def pair(key,value):
    key=key.encode('ascii'); value=value.encode('ascii') if isinstance(value,str) else value
    return uleb(len(key))+key+uleb(len(value))+value

def wasm_without_custom(data):
    assert data[:8]==b'\0asm\1\0\0\0'; out=bytearray(data[:8]); cursor=8
    while cursor<len(data):
        first=cursor; kind=data[cursor]; cursor += 1; length=shift=0
        for _ in range(5):
            assert cursor<len(data); byte=data[cursor];cursor+=1;length|=(byte&127)<<shift;shift+=7
            if not byte&128: break
        else: raise AssertionError('bad bounded section length')
        assert length <= len(data)-cursor
        if kind != 0: out += data[first:cursor+length]
        cursor += length
    return bytes(out)
U64_MAX = (1 << 64) - 1


def numeric(kind: int, low: int, high: int = 0) -> tuple[int, ...]:
    return kind, 0, 0, 0, 0, 0, 0, low, high, True


def reference(heap: int, kind: int, target: int = 0) -> tuple[int, ...]:
    return 6, heap, 1, kind, 1 if heap == 13 else 0, 0, target, 0, 0, True


def value(cell: tuple[int, ...]) -> bytes:
    kind, heap, nullable, ref, module, type_index, target, low, high, initialized = cell
    return struct.pack("<BBBBIQIIQQQ", kind, heap, nullable, ref, int(not initialized), module, type_index, 0, target, low, high)


def record(kind: int, flags: int = 0, words: tuple[int, ...] = (), links: tuple[int, ...] = (),
           values: tuple[tuple[int, ...], ...] = (), payload: bytes = b"") -> bytes:
    assert len(words) <= 8
    words += (0,) * (8 - len(words))
    return (struct.pack("<HHI8Q3Q", kind, flags, 0, *words, len(links), len(values), len(payload))
            + b"".join(struct.pack("<Q", link) for link in links)
            + b"".join(value(cell) for cell in values) + payload)


def golden() -> bytes:
    node = reference(13, 3, 12)
    small = list(reference(5, 6))
    small[7] = 0x7fffffff
    unset = list(reference(13, 0)); unset[2] = 0; unset[9] = False
    records = [
        record(1, payload=b"\0asm\1\0\0\0"),
        record(2, words=(1, 1, 1, 1, 1, 1, 1), links=(1, 3, 6, 4, 8, 9, 10, 11)),
        record(3, words=(0, 7, 3), links=(2,), payload=bytes((0, 0x41, 1, 0x0b))),
        record(4, 1, (64, 1 << 48, 0, 1 << 48)),
        record(5, words=(U64_MAX - 3,), links=(4,), payload=bytes((1, 2, 3, 4))),
        record(6, words=(64, 0x100000001, 0, U64_MAX), values=(reference(13, 0),)),
        record(7, words=(0x100000000,), links=(6,), values=(node,)),
        record(8, 1, values=(node,)),
        record(9, words=(2,), links=(1,)),
        record(10, payload=b"\0\xff\1"),
        record(11, values=(reference(1, 1, 3),)),
        record(12, links=(1,), values=(node, numeric(7, 255))),
        record(13, words=(1,), links=(1,), values=(node, node)),
        record(14, links=(9, 23), values=(node, numeric(2, U64_MAX))),
        record(15, values=(reference(3, 7, 24),)),
        record(16, 2, (1, 1), links=(17, 20)),
        record(17, words=(123, 6, 3, 1, 1, 7, 0), links=(3, 18, 19),
               values=(node, tuple(small), numeric(5, 0x0807060504030201, 0x100f0e0d0c0b0a09),
                       reference(2, 2, 15), reference(8, 5, 14), tuple(unset), numeric(1, 0xffffffff),
                       numeric(3, 0x7fc01234), numeric(4, 0x8000000000000000))),
        record(18, words=(100, 200, 0, 1)),
        record(19, 1, (0, 333, 14), links=(9,)),
        record(20, words=(U64_MAX - 3, 4, 0, 999), links=(4,)),
        record(21, 2, (42, 1, 1), payload=b"\x7f"),
        record(22, 2, (1, 100, 1, 7, 1, 1, 0, 5), links=(21,),
               values=(numeric(1, 4), numeric(1, 111)), payload=b"\xde\xad"),
        record(23, links=(3,), values=(numeric(2, 123),)),
        record(24, words=(1,), links=(21,), payload=b"Z"),
        record(15, values=(node,)),
        record(15, values=(tuple(small),)),
        record(15, values=(reference(7, 4, 13),)),
    ]
    retained = (node, reference(8, 5, 14))
    body = struct.pack("<Q", 2) + b"".join(value(cell) for cell in retained) + b"".join(records)
    recording_id = bytes((0x80,)) + bytes(14) + bytes((1,))
    header = struct.pack("<QHHIQ16s11Q", 0x0036545350435755, 6, 128, 0x04030201, 0, recording_id,
                         2, 1, 200, 0, 0x3ffff, 2, len(records), 1, len(retained), len(body), 0)
    assert len(header) == 128
    document = header + body
    return document + struct.pack("<QQ", 0x3645545350435755, len(body)) + hashlib.sha256(document).digest()

def state_bytes(wasm, dependency, logical):
    # Independent known27-object schema model, replacing only its original-Wasm
    # record with actual owner input. No runtime validity/capture is inferred.
    base=golden(); first=128+8+2*48
    assert struct.unpack_from('<Q',base,first+88)[0]==8
    object_header=bytearray(base[first:first+96]);struct.pack_into('<Q',object_header,88,len(wasm))
    body=base[128:first]+object_header+wasm+base[first+96+8:-48]+record(1,payload=dependency)
    header=bytearray(base[:128]);struct.pack_into('<Q',header,56,logical);struct.pack_into('<Q',header,88,28);struct.pack_into('<Q',header,112,len(body))
    prefix=bytes(header)+body
    return prefix+struct.pack('<QQ',0x3645545350435755,len(body))+digest(prefix)

def semantic_hashes(document):
    count, roots, retained = struct.unpack_from('<3Q', document, 88)
    cursor = 128 + roots * 8 + retained * 48
    objects = []
    for _ in range(count):
        begin = cursor
        kind, flags, reserved, *tail = struct.unpack_from('<HHI8Q3Q', document, cursor)
        assert reserved == 0
        words, links_count, values_count, payload_count = tail[:8], *tail[8:]
        cursor += 96
        links = struct.unpack_from('<' + 'Q' * links_count, document, cursor)
        cursor += links_count * 8
        values = document[cursor:cursor + values_count * 48]
        cursor += values_count * 48
        payload = document[cursor:cursor + payload_count]
        cursor += payload_count
        objects.append((kind, flags, words, links, values, payload, document[begin:cursor]))
    assert cursor == len(document) - 48
    modules = [i for i, x in enumerate(objects) if x[0] == 1]
    hashes = []
    for ordinal, module in enumerate(modules):
        instances = [i for i, x in enumerate(objects) if x[0] == 2 and x[3][0] == module + 1]
        by_instance = {i + 1: n for n, i in enumerate(instances)}
        functions = {}
        for x in objects:
            if x[0] != 3 or x[3][0] not in by_instance: continue
            key = by_instance[x[3][0]], x[2][0]
            cell = struct.pack('<QH4Q', key[0], x[1], *x[2][:3], len(x[5])) + x[5]
            if x[1]:
                adapter = objects[x[3][1] - 1]
                cell += struct.pack('<HQQ', adapter[1], *adapter[2][:2])
                if adapter[1] == 3:
                    cell += struct.pack('<QQ', adapter[2][2], len(adapter[5])) + adapter[5]
            assert key not in functions or functions[key] == cell
            functions[key] = cell
        wire = b'UWCPFG02' + struct.pack('<Q', ordinal) + digest(objects[module][5]) + struct.pack('<QQ', len(instances), len(functions))
        wire += b''.join(functions[key] for key in sorted(functions))
        hashes.append(digest(wire))
    events = [x[6] for x in objects if x[0] == 22]
    wire = b'UW CPEV02'.replace(b' ', b'') + document[24:40] + struct.pack('<QQ', struct.unpack_from('<Q', document, 64)[0], len(events))
    return hashes, digest(wire + b''.join(events))

def identity_wire(wa,wb,oa,ob,blobs,body,product):
    flavor=1 if product=='uwvm2' else 2
    key=pair('format','uwvm-ljc-key-v1')+pair('domain','actual-checkpoint-binding-component')+pair('wasm','fixture-A')
    keys=[key+pair('module-ordinal','0')+pair('partition-count','2')+pair('partition-ordinal',str(i)) for i in range(2)]
    keys.append(key+pair('module-ordinal','1'))
    isa=pair('target','component-target')+pair('cpu','component-cpu')+pair('features','component-feature')
    contexts=[pair('product','uwvm2' if flavor==1 else 'uwvm2ros')+pair('key',k)+pair('llvm','component-provider')+
              pair('uwvm_abi','component-abi')+pair('codegen','component-policy') for k in keys]
    payloads=[oa,ob,ob]
    for blob,ctx,obj in zip(blobs,contexts,payloads):
        magic,version,header,compression,signature,size,payload,isa_size,ctx_size,sig_size=struct.unpack_from('<8sIIIIQQQQQ',blob)
        assert magic==(b'UWVMLJC\x01' if flavor==1 else b'UWVMROS\x01')
        assert (version,header,compression,signature,sig_size)==(5,64,0,1,64)
        assert size==payload==len(obj) and len(blob)==64+isa_size+ctx_size+sig_size+payload
        assert blob[64:64+isa_size]==isa and blob[64+isa_size:64+isa_size+ctx_size]==ctx and blob[-payload:]==obj
    wire=bytearray(b'UWCPID02')+struct.pack('<IIIII4I',2,4,6,1,flavor,2,0,4,0)
    for h in (label('actual component source fixture'),label('actual component compile/link fixture'),digest(oa),
              label('actual component provider fixture'),label('actual component ABI fixture'),label('actual component codegen fixture')):
        wire += h
    wire += struct.pack('<QQ10Q',1,2,0x554357504c4f4731,17,6,2,16,4096,1048576,65536,65536,32768)
    for h in (label('actual component target+execution'),label('actual component two-source closure'),digest(body),
              semantic_hashes(body)[1]): wire += h
    wire += struct.pack('<IIQQ',1,0,2,3)
    for ordinal,wasm,name,role,generation in ((0,wa,'main',1,'actual component gen1 bodies'),(1,wb,'dependency',2,'actual component dependency gen1 bodies')):
        wire += struct.pack('<QQQI',ordinal,len(wasm),len(name),role)+digest(wasm)+label(name)+semantic_hashes(body)[0][ordinal]
    for i,(blob,obj,k,ctx) in enumerate(zip(blobs,payloads,keys,contexts)):
        module,ordinal,count,role=(0,i,2,2) if i<2 else (1,0,1,1)
        wire += struct.pack('<5QII',module,ordinal,count,len(blob),len(obj),role,5)
        for h in (digest(k),digest(isa),digest(ctx),digest(blob),digest(obj),label('actual component provider/emitter')):wire+=h
    assert len(wire)==484+2*124+3*240
    return bytes(wire)

def envelope_bytes(identity,body):
    header=struct.pack('<QHHI6Q',0x0034424450435755,4,160,0x04030201,0,len(identity),len(body),2,3,0)
    header+=digest(identity)+digest(body)+bytes(32)
    assert len(header)==160
    prefix=header+identity+body
    return prefix+struct.pack('<QQ',0x34444e4550435755,len(prefix))+digest(prefix)

def main():
    if len(sys.argv)!=13: raise SystemExit('oracle wasm-A wasm-B custom-only-C object-A object-B part0 part1 full1 envelope-A envelope-B product stdout')
    wa,wb,wc,oa,ob,p0,p1,f1,a,b=(pathlib.Path(x).read_bytes() for x in sys.argv[1:11])
    product=sys.argv[11];assert product in ('uwvm2','uwvm2-ros')
    assert wa!=wc and wasm_without_custom(wa)==wasm_without_custom(wc)
    body_a=state_bytes(wa,wb,200);body_b=state_bytes(wa,wb,201)
    identity_a=identity_wire(wa,wb,oa,ob,(p0,p1,f1),body_a,product)
    identity_b=identity_wire(wa,wb,oa,ob,(p0,p1,f1),body_b,product)
    expected_a=envelope_bytes(identity_a,body_a);expected_b=envelope_bytes(identity_b,body_b)
    assert a==expected_a and b==expected_b and a!=b
    out=pathlib.Path(sys.argv[12]).read_text()
    assert 'negatives=40' in out and 'bytes='+str(len(a)) in out and 'truncations='+str(len(a)) in out
    assert 'file_sha='+digest(a).hex() in out and 'two_distinct_saved_bodies_one_target=1' in out
    assert 'canonical_data_only=1 loaded_build_cache_issuer=0 whole_restore=0' in out
    print('CHECKPOINT_ENVELOPE_ORACLE PASS',product,len(a),digest(a).hex(),'DATA-only; no loaded/build/restore authority')
if __name__=='__main__':main()
