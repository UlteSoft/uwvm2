#!/usr/bin/env python3
"""Read function-body/local shape without decoding or executing guest code."""
import argparse
import json
from pathlib import Path
import sys

from linux_control import digest, guard

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from inspect_wasm import Reader, inspect


def shape(path):
    module = inspect(path)
    imports = sum(entry['kind'] == 0 for entry in module['imports'])
    reader = Reader(Path(path).read_bytes())
    reader.pos = 8
    functions = []
    while reader.pos < len(reader.data):
        tag, size = reader.byte(), reader.leb()
        end = reader.pos + size
        if tag == 10:
            for number in range(reader.leb()):
                body_bytes = reader.leb()
                body_end = reader.pos + body_bytes
                groups = []
                for _ in range(reader.leb()):
                    groups.append({'count': reader.leb(), 'type': reader.value_type()})
                index = imports + number
                functions.append({'index': index, 'name': module['functions'].get(index),
                                  'body_bytes': body_bytes,
                                  'locals': sum(g['count'] for g in groups),
                                  'local_groups': groups,
                                  'instruction_bytes': body_end - reader.pos})
                reader.pos = body_end
        reader.pos = end
    return {'wasm': str(path), 'sha256': digest(path), 'function_count': len(functions),
            'declared_locals_total': sum(f['locals'] for f in functions),
            'top_locals': sorted(functions, key=lambda f: f['locals'], reverse=True)[:20],
            'top_body_bytes': sorted(functions, key=lambda f: f['body_bytes'], reverse=True)[:20],
            'eval_frame': [f for f in functions if 'EvalFrameDefault' in (f['name'] or '')]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('wasm', type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    guard(args.out.parent)
    args.out.write_text(json.dumps(shape(args.wasm), indent=2) + '\n')


if __name__ == '__main__':
    main()
