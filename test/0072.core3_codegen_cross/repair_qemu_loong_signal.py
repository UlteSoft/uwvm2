#!/usr/bin/env python3
"""Repair only the pinned QEMU 10.2.1 embedded LoongArch signal CIE.

The output is a private test provider. Never replace a shared QEMU binary.
The source equivalent is documents/toolchain/patches/qemu-loongarch-signal-cfi.patch.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

ORIGINAL_SHA256 = '81f4c16209845ee604f00615f39d0d782a896c04d86c90850ccf966635c3227e'
OLD = bytes.fromhex('017a525300017840011b0c02b002054000')
NEW = bytes.fromhex('017a525300017800011b0c03b002050000')


def digest(blob):
    return hashlib.sha256(blob).hexdigest()


def repair(source, output):
    original = source.read_bytes()
    assert digest(original) == ORIGINAL_SHA256, 'Unqualified QEMU provider'
    candidates, cursor = [], 0
    while True:
        cursor = original.find(b'\x7fELF', cursor)
        if cursor < 0:
            break
        if (original[cursor + 4:cursor + 6] == b'\x02\x01' and
                original[cursor + 16:cursor + 20] == b'\x03\x00\x02\x01'):
            candidates.append(cursor)
        cursor += 4
    assert len(candidates) == 1
    base = candidates[0]
    shoff = int.from_bytes(original[base + 40:base + 48], 'little')
    stride, count, name_index = struct.unpack_from('<HHH', original, base + 58)
    assert stride == 64 and 0 < name_index < count <= 64
    assert base + shoff + count * stride <= len(original)
    headers = [struct.unpack_from('<IIQQQQIIQQ', original, base + shoff + i * stride)
               for i in range(count)]
    name_header = headers[name_index]
    assert base + name_header[4] + name_header[5] <= len(original)
    names = original[base + name_header[4]:base + name_header[4] + name_header[5]]
    sections = {}
    for header in headers:
        assert header[0] < len(names)
        name = names[header[0]:].split(b'\0', 1)[0].decode('ascii')
        assert name not in sections and base + header[4] + header[5] <= len(original)
        sections[name] = header
    eh = sections['.eh_frame']
    begin, end = base + eh[4], base + eh[4] + eh[5]
    entries, cursor = [], begin
    while cursor < end:
        length, cie_pointer = struct.unpack_from('<II', original, cursor)
        assert 8 <= length and cursor + 4 + length <= end
        if cie_pointer == 0:
            entries.append(cursor)
        cursor += 4 + length
    assert cursor == end
    matches = [p + 8 for p in entries if original[p + 8:p + 8 + len(OLD)] == OLD]
    assert len(matches) == 1
    offset = matches[0]
    changed = bytearray(original)
    changed[offset:offset + len(OLD)] = NEW
    deltas = [i for i, (a, b) in enumerate(zip(original, changed)) if a != b]
    assert deltas == [offset + 7, offset + 11, offset + 15]
    assert len(changed) == len(original)
    # No guest instruction, ELF extent, relocation or native emulator code changes.
    text = sections['.text']
    assert original[base + text[4]:base + text[4] + text[5]] == changed[base + text[4]:base + text[4] + text[5]]
    assert not output.exists()
    with output.open('xb') as stream:
        stream.write(changed)
    output.chmod(source.stat().st_mode & 0o777)
    assert digest(output.read_bytes()) == digest(changed)
    receipt = dict(passed=True, source=str(source.resolve()), output=str(output.resolve()),
                   original_sha256=digest(original), repaired_sha256=digest(changed),
                   embedded_elf_offset=base, signal_cie_payload_offset=offset,
                   changed_byte_offsets=deltas, old_payload=OLD.hex(), new_payload=NEW.hex(),
                   guest_text_sha256=digest(original[base + text[4]:base + text[4] + text[5]]),
                   scope='Exact three-byte signal-CIE provider repair; execution is qualified separately',
                   source_reference='https://raw.githubusercontent.com/qemu/qemu/v10.2.1/linux-user/loongarch64/vdso.S',
                   abi_reference='https://github.com/torvalds/linux/blob/master/arch/loongarch/include/asm/linkage.h')
    output.with_suffix('.repair.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('PASS private QEMU signal CIE repair', receipt['repaired_sha256'], flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    repair(args.source, args.output)
