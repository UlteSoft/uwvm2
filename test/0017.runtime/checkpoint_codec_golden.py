#!/usr/bin/env python3
"""Independent canonical binary model; never runs native tools or a VM.

This tests schema/codec representation, not semantic module or VM restore.
Generate the same fixture as debug_checkpoint_codec.cc and compare actual
little/big-endian product output files with these bytes on the remote keeper.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import pathlib
import struct

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
        record(3, words=(0, 7, 3), links=(2,)),
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


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", type=pathlib.Path)
    parser.add_argument("--compare", action="append", default=[], type=pathlib.Path)
    args = parser.parse_args()
    expected = golden()
    if args.write:
        with args.write.open("xb") as stream:
            stream.write(expected)
    results = []
    for path in args.compare:
        actual = path.read_bytes()
        if actual != expected:
            raise SystemExit(f"canonical binary mismatch: {path}")
        results.append({"path": str(path), "bytes": len(actual), "sha256": hashlib.sha256(actual).hexdigest(), "equal": True})
    print(json.dumps({"component": "checkpoint schema/codec", "bytes": len(expected),
                      "sha256": hashlib.sha256(expected).hexdigest(), "comparisons": results,
                      "full_vm_continuation_restore_accepted": False}, sort_keys=True))


if __name__ == "__main__":
    main()
