#!/usr/bin/env python3
"""Read current table pages and GC members after real Wasm mutations (Linux cgroup)."""
import re
from pathlib import Path
import run_wasm_operand_preview_cli as preview
from wasm_live_container_cases import live_container_examples


_cases = {case['name']: case for case in live_container_examples()}
_operands = preview.operands


class ContainerConsole(preview.OperandConsole):
    def __init__(self, argv, log):
        super().__init__(argv, log)
        self.case = _cases[Path(argv[-1]).stem]
        self.point = 0


def header(reply, thread, first, total):
    assert b'Wasm state unavailable' not in reply and b'error:' not in reply, reply
    match = re.search(rb'^Wasm state thread=(\d+) module=0 epoch=(\d+) first=(\d+) total=(\d+)$', reply, re.M)
    assert match and (int(match[1]), int(match[3]), int(match[4])) == (thread, first, total), reply


def status_id(console):
    reply = console.send('status')
    match = re.search(rb'^stop-id (\d+)$', reply, re.M)
    assert b'stopped:' in reply and match, reply
    return int(match[1])


def inspect(console, thread, frame, expected, observations):
    _operands(console, thread, frame, expected, observations)
    assert frame == 0
    point = console.case['expected'][console.point]
    assert point['values'] == expected
    stop = status_id(console)
    pages = []
    for table in point['tables']:
        values = table['values']; total = len(values); index = table['index']
        windows = ((0, 64), (1, 2), (total - 1, 64), (total, 1)) if total else ((0, 64), (0, 1))
        for first, count in windows:
            command = f'table {thread} 0 {index} {first} {count}'
            reply = console.send(command)
            header(reply, thread, first, total)
            rows = re.findall(rb'^table (\d+) element (\d+) (.*)$', reply, re.M)
            want = values[first:first + count]
            assert len(rows) == len(want), (command, reply)
            for offset, ((table_id, element, value), pattern) in enumerate(zip(rows, want)):
                assert (int(table_id), int(element)) == (index, first + offset), reply
                assert re.fullmatch(pattern, value.decode()), (pattern, reply)
            pages.append(dict(command=command, kind='table', first=first, total=total,
                              values=[value.decode() for _, _, value in rows]))
    for member in point['members']:
        total = len(member['values'])
        windows = ((0, 64), (total - 1, 1), (total, 1)) if total else ((0, 64), (0, 1))
        for first, count in windows:
            command = (f"members {member['selection']} {thread} 0 0 {member['table']} "
                       f"{member['root']} {first} {count}" + ''.join(f' {edge}' for edge in member['path']))
            reply = console.send(command)
            assert b'Wasm state unavailable' not in reply and b'error:' not in reply, reply
            selected = re.search(rb'^Wasm members object=(\d+) first=(\d+) count=(\d+) path=(.*)$', reply, re.M)
            assert selected and (int(selected[2]), int(selected[3])) == (first, count), reply
            assert selected[4].decode() == (','.join(map(str, member['path'])) or '-'), reply
            object_id = int(selected[1]); end = min(total, first + count)
            label = (f"object #{object_id} {member['kind']} module=0 type={member['type_index']} "
                     f"members={total} first={first} next={end} more={'yes' if end < total else 'no'}")
            lines = reply.decode().splitlines(); at = lines.index(label)
            actual = []
            for index, pattern in enumerate(member['values'][first:end], first):
                line = lines[at + 1 + index - first]
                assert line.startswith(f'  {index} '), reply
                value = line[len(f'  {index} '):]
                assert re.fullmatch(pattern, value), (pattern, reply)
                actual.append(value)
            assert at + 1 + len(actual) == len(lines) or not re.match(r'^  \d+ ', lines[at + 1 + len(actual)]), reply
            pages.append(dict(command=command, kind='members', first=first, total=total,
                              path=member['path'], values=actual))
    assert status_id(console) == stop, 'read-only queries changed the genuine stop'
    observations[0]['container_pages'] = pages
    observations[0]['stop_id'] = stop
    console.point += 1


if __name__ == '__main__':
    preview.examples = live_container_examples
    preview.OperandConsole = ContainerConsole
    preview.operands = inspect
    preview.main()
