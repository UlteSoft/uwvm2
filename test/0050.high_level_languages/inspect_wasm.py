#!/usr/bin/env python3
"""Read core module import/export identities without rewriting producer output."""
import json
from pathlib import Path
import sys


class Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def byte(self):
        value = self.data[self.pos]
        self.pos += 1
        return value

    def leb(self):
        value = shift = 0
        for _ in range(10):
            byte = self.byte()
            value |= (byte & 127) << shift
            if not byte & 128:
                return value
            shift += 7
        raise ValueError('overlong LEB')

    def name(self):
        size = self.leb()
        value = self.data[self.pos:self.pos + size].decode('utf-8')
        self.pos += size
        return value

    def sleb(self):
        value = shift = 0
        for _ in range(5):
            byte = self.byte()
            value |= (byte & 127) << shift
            shift += 7
            if not byte & 128:
                return value - (1 << shift) if byte & 64 else value
        raise ValueError('overlong signed heap type')

    def limits(self):
        flags = self.leb()
        minimum = self.leb()
        return {'flags': flags, 'min': minimum, 'max': self.leb() if flags & 1 else None}

    def value_type(self):
        value = self.byte()
        if value in (0x63, 0x64):
            return [value, self.sleb()]
        return value


def inspect(path):
    data = Path(path).read_bytes()
    if data[:8] != b'\0asm\x01\0\0\0':
        raise ValueError('expected a core Wasm module, not a component')
    reader = Reader(data)
    reader.pos = 8
    result = {'imports': [], 'exports': [], 'custom': [], 'functions': {}, 'code_bodies': 0}
    while reader.pos < len(data):
        tag, size = reader.byte(), reader.leb()
        end = reader.pos + size
        section = Reader(data[reader.pos:end])
        if tag == 0:
            name = section.name()
            result['custom'].append(name)
            if name == 'name':
                while section.pos < len(section.data):
                    subtag, subsize = section.byte(), section.leb()
                    subend = section.pos + subsize
                    if subtag == 1:
                        for _ in range(section.leb()):
                            index, function_name = section.leb(), section.name()
                            result['functions'][index] = function_name
                    section.pos = subend
        elif tag == 2:
            for _ in range(section.leb()):
                module, name, kind = section.name(), section.name(), section.byte()
                entry = {'module': module, 'name': name, 'kind': kind}
                if kind == 0:
                    entry['type'] = section.leb()
                elif kind == 1:
                    entry['element'] = section.value_type()
                    entry['limits'] = section.limits()
                elif kind == 2:
                    entry['limits'] = section.limits()
                elif kind == 3:
                    entry['value_type'], entry['mutable'] = section.value_type(), section.byte()
                elif kind == 4:
                    entry['attribute'], entry['type'] = section.byte(), section.leb()
                else:
                    raise ValueError('unknown import kind')
                result['imports'].append(entry)
        elif tag == 7:
            for _ in range(section.leb()):
                result['exports'].append({'name': section.name(), 'kind': section.byte(), 'index': section.leb()})
        elif tag == 10:
            result['code_bodies'] = section.leb()
        reader.pos = end
    return result


if __name__ == '__main__':
    print(json.dumps(inspect(sys.argv[1]), indent=2))
