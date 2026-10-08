#!/usr/bin/env python3
"""Independent Python DATA canonicalization oracle, not VM/authority execution."""
import hashlib
import pathlib
import struct
import sys

def digest(data):
    return hashlib.sha256(data).digest()

def label(text):
    return digest(text.encode("ascii"))

def uleb(value):
    # Independent test reference only; production C++ uses fast_io codecs.
    out = bytearray()
    while True:
        byte = value & 127
        value >>= 7
        out.append(byte | (128 if value else 0))
        if not value:
            return bytes(out)

def pair(key, value):
    key = key.encode("ascii")
    value = value.encode("ascii") if isinstance(value, str) else value
    return uleb(len(key)) + key + uleb(len(value)) + value

def identity(wasm, obj, blob, product):
    assert product in ("uwvm2", "uwvm2-ros")
    fmt = "<8sIIIIQQQQQ"
    magic, version, header, compression, signature, size, payload, isa_size, ctx_size, sig_size = struct.unpack_from(fmt, blob)
    expected_magic = b"UWVMLJC\x01" if product == "uwvm2" else b"UWVMROS\x01"
    assert magic == expected_magic and version == 5 and header == 64
    assert compression == 0 and signature == 1 and sig_size == 64 and size == len(obj)
    assert len(blob) == 64 + isa_size + ctx_size + sig_size + payload and payload == len(obj)
    key = pair("format", "uwvm-ljc-key-v1") + pair("domain", "actual-checkpoint-binding-component") + pair("wasm", "fixture-A")
    isa = pair("target", "component-target") + pair("cpu", "component-cpu") + pair("features", "component-feature")
    cache_product = "uwvm2" if product == "uwvm2" else "uwvm2ros"
    context = pair("product", cache_product) + pair("key", key) + pair("llvm", "component-provider") + pair("uwvm_abi", "component-abi") + pair("codegen", "component-policy")
    assert blob[64:64+isa_size] == isa
    assert blob[64+isa_size:64+isa_size+ctx_size] == context
    assert blob[-payload:] == obj
    wire = bytearray(b"UWCPID02")
    wire += struct.pack("<IIIII4I", 2, 4, 6, 1, 1 if product == "uwvm2" else 2, 2, 0, 4, 0)
    for h in (label("actual component source fixture"), label("actual component compile/link fixture"), digest(obj),
              label("actual component provider fixture"), label("actual component ABI fixture"), label("actual component codegen fixture")):
        wire += h
    wire += struct.pack("<QQ", 1, 2)
    wire += struct.pack("<10Q", 0x554357504c4f4731, 17, 6, 2, 16, 4096, 1048576, 65536, 65536, 32768)
    for h in (label("actual component target+execution"), label("actual component single-source closure"),
              label("actual component state body"), label("actual component empty events")):
        wire += h
    wire += struct.pack("<IIQQ", 1, 0, 1, 1)
    wire += struct.pack("<QQQI", 0, len(wasm), 4, 1)
    wire += digest(wasm) + label("main") + label("actual component gen1 bodies")
    wire += struct.pack("<5QII", 0, 0, 1, len(blob), len(obj), 1, 5)
    for h in (digest(key), digest(isa), digest(context), digest(blob), digest(obj), label("actual component provider/emitter")):
        wire += h
    return digest(wire).hex()

def bundle_identity(wasm_a, wasm_b, object_a, object_b, blobs, product):
    """Independent ordered two-module / 2-partition + full object reference."""
    base_key = pair("format", "uwvm-ljc-key-v1") + pair("domain", "actual-checkpoint-binding-component") + pair("wasm", "fixture-A")
    isa = pair("target", "component-target") + pair("cpu", "component-cpu") + pair("features", "component-feature")
    keys = [base_key + pair("module-ordinal", "0") + pair("partition-count", "2") + pair("partition-ordinal", str(i)) for i in range(2)]
    keys.append(base_key + pair("module-ordinal", "1"))
    cache_product = "uwvm2" if product == "uwvm2" else "uwvm2ros"
    contexts = [pair("product", cache_product) + pair("key", key) + pair("llvm", "component-provider") + pair("uwvm_abi", "component-abi") + pair("codegen", "component-policy") for key in keys]
    payloads = [object_a, object_b, object_b]
    for blob, context, payload in zip(blobs, contexts, payloads):
        magic, version, header, compression, signature, size, encoded_payload, isa_size, context_size, signature_size = struct.unpack_from("<8sIIIIQQQQQ", blob)
        assert magic == (b"UWVMLJC\x01" if product == "uwvm2" else b"UWVMROS\x01")
        assert (version, header, compression, signature, signature_size) == (5, 64, 0, 1, 64)
        assert size == encoded_payload == len(payload)
        assert len(blob) == 64 + isa_size + context_size + signature_size + encoded_payload
        assert blob[64:64 + isa_size] == isa
        assert blob[64 + isa_size:64 + isa_size + context_size] == context
        assert blob[-encoded_payload:] == payload
    wire = bytearray(b"UWCPID02")
    wire += struct.pack("<IIIII4I", 2, 4, 6, 1, 1 if product == "uwvm2" else 2, 2, 0, 4, 0)
    for h in (label("actual component source fixture"), label("actual component compile/link fixture"), digest(object_a),
              label("actual component provider fixture"), label("actual component ABI fixture"), label("actual component codegen fixture")):
        wire += h
    wire += struct.pack("<QQ10Q", 1, 2, 0x554357504c4f4731, 17, 6, 2, 16, 4096, 1048576, 65536, 65536, 32768)
    for h in (label("actual component target+execution"), label("actual component two-source closure"),
              label("actual component state body"), label("actual component empty events")):
        wire += h
    wire += struct.pack("<IIQQ", 1, 0, 2, 3)
    for ordinal, wasm, name, role, generation in ((0, wasm_a, "main", 1, "actual component gen1 bodies"),
                                                  (1, wasm_b, "dependency", 2, "actual component dependency gen1 bodies")):
        wire += struct.pack("<QQQI", ordinal, len(wasm), len(name), role)
        wire += digest(wasm) + label(name) + label(generation)
    for i, (blob, payload, key, context) in enumerate(zip(blobs, payloads, keys, contexts)):
        module, partition, count, role = (0, i, 2, 2) if i < 2 else (1, 0, 1, 1)
        wire += struct.pack("<5QII", module, partition, count, len(blob), len(payload), role, 5)
        for h in (digest(key), digest(isa), digest(context), digest(blob), digest(payload), label("actual component provider/emitter")):
            wire += h
    return digest(wire).hex()

def main():
    if len(sys.argv) != 12:
        raise SystemExit("oracle.py wasm-A wasm-B object-A object-B blob-A blob-B part-0 part-1 full-1 uwvm2|uwvm2-ros actual-stdout")
    wasm_a, wasm_b, object_a, object_b, blob_a, blob_b, p0, p1, f1 = (pathlib.Path(path).read_bytes() for path in sys.argv[1:10])
    product = sys.argv[10]
    assert product in ("uwvm2", "uwvm2-ros")
    expected = identity(wasm_a, object_a, blob_a, product)
    identity(wasm_b, object_b, blob_b, product)  # independent actual B blob/context checks
    expected_bundle = bundle_identity(wasm_a, wasm_b, object_a, object_b, (p0, p1, f1), product)
    actual = pathlib.Path(sys.argv[11]).read_text().strip()
    assert "digest=" + expected in actual and "bundle_digest=" + expected_bundle in actual, (expected, expected_bundle, actual)
    assert "negatives=45" in actual and "complete_modules=2 complete_objects=3" in actual
    assert "canonical_data_only=1 loaded_build_cache_issuer=0 whole_restore=0" in actual
    print("CHECKPOINT_BINDING_ORACLE PASS", product, expected, expected_bundle, "DATA-only; no VM qualification")

if __name__ == "__main__":
    main()
