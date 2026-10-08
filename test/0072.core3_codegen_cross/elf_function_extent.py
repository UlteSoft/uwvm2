#!/usr/bin/env python3
"""Inspect complete, bounded ELF function extents; local labels never end a function.

This reader accepts ordinary ELF32/ELF64 relocatable objects in either byte
order. Extended section indices are rejected. It does not authenticate or
execute objects; the VM owns signed-cache validation.
"""
import struct

def function_extent(data, wanted):
    assert data[:4] == b'\x7fELF' and data[4] in (1, 2) and data[5] in (1, 2)
    endian = '<' if data[5] == 1 else '>'
    def extent(offset, size):
        assert 0 <= offset <= len(data) and 0 <= size <= len(data) - offset
        return data[offset:offset + size]
    header = struct.unpack(endian + ('HHIQQQIHHHHHH' if data[4] == 2 else 'HHIIIIIHHHHHH'), extent(16, 48 if data[4] == 2 else 36))
    assert header[0] == 1
    section_format = endian + ('IIQQQQIIQQ' if data[4] == 2 else 'IIIIIIIIII')
    stride = struct.calcsize(section_format)
    assert header[10] == stride and 1 < header[11] < 65535 and header[12] < header[11]
    sections = [struct.unpack(section_format, extent(header[5] + i * stride, stride)) for i in range(header[11])]
    def section_data(section):
        return extent(section[4], section[5])
    def name(table, offset):
        end = table.find(b'\0', offset)
        assert 0 <= offset < len(table) and end >= offset
        return table[offset:end].decode('utf-8')
    section_names = section_data(sections[header[12]])
    found = []
    for table in sections:
        if table[1] != 2:
            continue
        fmt = endian + ('IBBHQQ' if data[4] == 2 else 'IIIBBH')
        step = struct.calcsize(fmt)
        assert table[9] == step and table[5] % step == 0 and table[6] < len(sections)
        names = section_data(sections[table[6]])
        symbols = section_data(table)
        for offset in range(0, len(symbols), step):
            fields = struct.unpack_from(fmt, symbols, offset)
            n, info, _, section, value, size = fields if data[4] == 2 else (fields[0], fields[3], fields[4], fields[5], fields[1], fields[2])
            if name(names, n) != wanted:
                continue
            assert info & 15 == 2 and 0 < section < len(sections)
            target = sections[section]
            assert target[2] & 4 and 0 < size <= target[5] and value <= target[5] - size
            found.append((name(section_names, target[0]), target[3] + value, size))
    assert len(found) == 1, (wanted, found)
    return found[0]

