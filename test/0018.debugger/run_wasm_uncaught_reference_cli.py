#!/usr/bin/env python3
"""Inspect real cross-module GC/exception reference graphs, Linux cgroup only."""
import re
from pathlib import Path
import run_wasm_uncaught_import_cli as base
from wasm_uncaught_reference_cases import reference_uncaught_examples


def bind(bindings, role, module):
    if role not in bindings:
        assert module not in bindings.values(), (role, module, bindings)
        bindings[role] = module
    assert bindings[role] == module, (role, module, bindings)


def objects(packet):
    found = {}
    current = None
    for line in packet.decode().splitlines():
        match = re.fullmatch(r'object #([1-9][0-9]*) (.+) members=([0-9]+) first=([0-9]+) next=([0-9]+) more=(yes|no)', line)
        if match:
            identifier = int(match[1]); assert identifier not in found, packet
            current = dict(identifier=identifier, identity=match[2], total=int(match[3]),
                           first=int(match[4]), next=int(match[5]), more=match[6], values=[])
            found[identifier] = current
        elif current is not None:
            member = re.fullmatch(r'  ([0-9]+) (.*)', line)
            if member:
                current['values'].append((int(member[1]), member[2]))
    return found


def value(actual, wanted, bindings, aliases):
    if isinstance(wanted, str):
        assert actual == wanted, (actual, wanted)
        return
    suffix = wanted.get('suffix', '')
    assert not suffix or actual.endswith(suffix), (actual, wanted)
    if suffix:
        actual = actual[:-len(suffix)]
    heap = wanted['heap']
    type_text = ('null ' if wanted['nullable'] else '') + ('type-index=' + str(wanted['index']) if heap == 'type' else heap)
    if heap == 'type':
        match = re.fullmatch(r'\(ref ' + re.escape(type_text) + r' module=([0-9]+)\) = (.*)', actual)
        assert match, (actual, wanted)
        bind(bindings, wanted['role'], int(match[1])); payload = match[2]
    else:
        prefix = '(ref ' + type_text + ') = '
        assert actual.startswith(prefix), (actual, wanted)
        payload = actual[len(prefix):]
    kind = wanted['kind']
    if kind == 'null':
        assert payload == 'null', actual
    elif kind == 'i31':
        assert payload == 'i31 signed=-123 unsigned=2147483525', actual
    elif kind == 'function':
        match = re.fullmatch(r'function module=([0-9]+) index=([0-9]+)', payload)
        assert match and int(match[2]) == wanted['function'], (actual, wanted)
        bind(bindings, wanted['role'], int(match[1]))
    else:
        match = re.fullmatch(re.escape(kind) + r' #([1-9][0-9]*)', payload)
        assert match and wanted['symbol'], (actual, wanted)
        symbol = wanted['symbol']; identifier = int(match[1])
        if symbol in aliases:
            assert aliases[symbol] == identifier, ('lost query-local alias', symbol, aliases, actual)
        else:
            assert identifier not in aliases.values(), ('distinct objects were aliased', aliases, actual)
            aliases[symbol] = identifier


def identity(node, expected, bindings):
    kind = expected['kind']
    if kind in ('struct', 'array'):
        match = re.fullmatch(kind + r' module=([0-9]+) type=([0-9]+)', node['identity'])
        assert match and int(match[2]) == expected['type'], (node, expected)
        bind(bindings, expected['role'], int(match[1]))
    elif kind == 'exception':
        match = re.fullmatch(r'exception tag-module=([0-9]+) tag=([0-9]+)', node['identity'])
        assert match and int(match[2]) == expected['tag'], (node, expected)
        bind(bindings, expected['role'], int(match[1]))
    else:
        assert node['identity'] == kind, (node, expected)
    assert node['total'] == len(expected['values']), (node, expected)


def graph(packet, expected, bindings, aliases, selected=None, first=0, count=64, ancestors=()):
    parsed = objects(packet)
    for symbol in ancestors:
        if symbol in aliases:
            continue
        candidates = []
        for identifier, node in parsed.items():
            try:
                trial = dict(bindings)
                identity(node, expected[symbol], trial)
            except AssertionError:
                continue
            if identifier not in aliases.values():
                candidates.append(identifier)
        assert len(candidates) == 1, (symbol, candidates, parsed)
        aliases[symbol] = candidates[0]
    if selected is not None:
        symbol, identifier = selected
        aliases[symbol] = identifier
    visited = set()
    while any(symbol not in visited for symbol in aliases):
        symbol = next(symbol for symbol in aliases if symbol not in visited)
        visited.add(symbol); identifier = aliases[symbol]
        assert identifier in parsed and symbol in expected, (aliases, parsed, expected)
        node = parsed[identifier]; oracle = expected[symbol]
        identity(node, oracle, bindings)
        # Member queries intentionally return metadata for other objects;
        # their members must be expanded through the original root/path.
        if selected is not None and identifier != selected[1]:
            assert node['values'] == [], node
            continue
        start = first if selected else 0
        end = min(node['total'], start + count) if selected else node['total']
        assert (node['first'], node['next'], node['more']) == (start, end, 'yes' if end < node['total'] else 'no'), node
        assert [n for n, _ in node['values']] == list(range(start, end)), node
        for (_, actual), wanted in zip(node['values'], oracle['values'][start:end]):
            value(actual, wanted, bindings, aliases)
    assert set(parsed) == set(aliases.values()), ('unexpected or orphan graph object', parsed, aliases)
    return list(parsed.values())


def inspect(console, case, bindings, view):
    stop, _ = base.status(console); thread = stop['thread']; count = len(case['frames'])
    reply = console.send(f'frames wasm {thread} {stop["stop_id"]} 0 16')
    head = re.search(rb'^wasm-frames stop=([0-9]+) thread=([0-9]+) selected=0 count=([0-9]+) total=([0-9]+) first=0 physical=0$', reply, re.M)
    assert head and tuple(map(int, head.groups())) == (stop['stop_id'], thread, count, count), reply
    rows = re.findall(rb'^  frame ([0-9]+) kind=(physical|caller) module=([0-9]+) function=([0-9]+) generation=([0-9]+) runtime-epoch=([0-9]+) ', reply, re.M)
    assert len(rows) == count, reply
    for ordinal, (row, oracle) in enumerate(zip(rows, case['frames'])):
        assert int(row[0]) == ordinal and row[1] == (b'physical' if ordinal == 0 else b'caller'), reply
        bind(bindings, oracle['role'], int(row[2]))
        assert int(row[3]) == oracle['function'] and int(row[4]) > 0 and int(row[5]) == stop['runtime_epoch'], reply
    frames = []
    for ordinal, (row, oracle) in enumerate(zip(rows, case['frames'])):
        module, function, generation, epoch = map(int, row[2:])
        page = dict(ordinal=ordinal, role=oracle['role'], module=module, function=function,
                    generation=generation, runtime_epoch=epoch, member_pages=[], graphs={})
        for selection, label in (('operands', 'operand'), ('locals', 'local')):
            command = f'operands {thread} {ordinal} 0 64' if selection == 'operands' else f'locals wasm {thread} {ordinal} 0 64'
            packet = console.send(command)
            header = re.search(rb'^Wasm state thread=([0-9]+) module=([0-9]+) epoch=([0-9]+) first=0 total=([0-9]+)$', packet, re.M)
            wanted = oracle['values'] if selection == 'operands' else oracle['locals']
            assert header and tuple(map(int, header.groups())) == (thread, module, epoch, len(wanted)), packet
            actual = re.findall(rb'^' + label.encode() + rb' ([0-9]+) (.*)$', packet, re.M)
            assert [int(n) for n, _ in actual] == list(range(len(wanted))), packet
            aliases = {}
            for (_, rendered), expected in zip(actual, wanted):
                value(rendered.decode(), expected, bindings, aliases)
            page[selection] = [rendered.decode() for _, rendered in actual]
            page['graphs'][selection] = graph(packet, oracle['objects'], bindings, aliases)
            if selection == 'operands' and view == 'after-actual-throw':
                assert b'Note: Uncaught Wasm snapshot; stack has not unwound.' in packet, packet
        for member in oracle['members']:
            total = len(oracle['objects'][member['symbol']]['values'])
            for first, requested in ((0, 64), (total - 1, 1), (total, 1)):
                path = member['path']
                command = (f'members {member["selection"]} {thread} {module} {ordinal} 0 {member["root"]} {first} {requested}'
                           + ''.join(f' {edge}' for edge in path))
                packet = console.send(command)
                assert b'error:' not in packet and b'Wasm state unavailable' not in packet, packet
                header = re.search(rb'^Wasm state thread=([0-9]+) module=([0-9]+) epoch=([0-9]+) first=([0-9]+) total=([0-9]+)$', packet, re.M)
                original_values = oracle['values'] if member['selection'] == 'operands' else oracle['locals']
                assert header and tuple(map(int, header.groups())) == (thread, module, epoch, member['root'], len(original_values)), packet
                selected = re.search(rb'^Wasm members object=([1-9][0-9]*) first=([0-9]+) count=([0-9]+) path=(.*)$', packet, re.M)
                assert selected and (int(selected[2]), int(selected[3])) == (first, requested), packet
                assert selected[4].decode() == (','.join(map(str, path)) or '-'), packet
                aliases = {}
                root_expected = (oracle['values'] if member['selection'] == 'operands' else oracle['locals'])[member['root']]
                root_row = re.search(rb'^(?:operand|local) ([0-9]+) (.*)$', packet, re.M)
                assert root_row and int(root_row[1]) == member['root'], packet
                value(root_row[2].decode(), root_expected, bindings, aliases)
                ancestors = [root_expected['symbol']]
                for edge in path:
                    ancestors.append(oracle['objects'][ancestors[-1]]['values'][edge]['symbol'])
                assert ancestors[-1] == member['symbol'], (member, ancestors)
                copied = graph(packet, oracle['objects'], bindings, aliases,
                               (member['symbol'], int(selected[1])), first, requested, ancestors)
                page['member_pages'].append(dict(command=command, selection=member['selection'],
                    root=member['root'], path=path, symbol=member['symbol'], first=first,
                    count=requested, selected_object=int(selected[1]), objects=copied))
        frames.append(page)
    assert base.status(console)[0] == stop, 'read-only reference queries changed the stop'
    return dict(kind=view, **stop, frames=frames)


if __name__ == '__main__':
    base.FEATURES += ('gc', 'bulk-memory')
    base.CASE_FILE = Path(__file__).with_name('wasm_uncaught_reference_cases.py')
    base.__file__ = __file__
    base.imported_uncaught_examples = reference_uncaught_examples
    base.inspect = inspect
    base.main()
