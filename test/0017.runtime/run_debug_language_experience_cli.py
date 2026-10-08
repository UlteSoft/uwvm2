#!/usr/bin/env python3
"""Actual compiler-produced C/C++/Rust language experience, keeper Linux only.

Required missing capabilities are UNAVAILABLE and make this recipe fail. A
source literal, a hand-built controller reply or a finite metadata component
cannot satisfy any case. Reuse existing real-console/official-line helpers;
this adds no product I/O or independent Wasm/decimal parser.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess
import sys
import run_debug_source_inline_metadata_cli as metadata_cli
import run_debug_source_step_cli as step_cli

require = step_cli.require
sha = step_cli.sha
CASES = ('aggregate', 'lexical', 'inline-vars', 'next', 'finish', 'frame-selection', 'inline-frame-selection', 'scalar-constants')
MARKERS = ('LANG_OBJECT_READY', 'LANG_BEFORE_INLINE', 'LANG_INLINE_INNER', 'LANG_INLINE_MIDDLE',
           'LANG_INNER_SHADOW', 'LANG_PHYSICAL_CALL', 'LANG_AFTER_CALL', 'LANG_OUTER_SHADOW', 'LANG_LEAF_READY', 'LANG_CONSTANT_VALUES')


class Unavailable(RuntimeError):
    """A required capability is explicitly absent; never a successful test."""


def clean(reply: bytes) -> bytes:
    return re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]', b'', reply)


def query(session: step_cli.Session, command: str) -> bytes:
    """Permit an expected rejected read without weakening stepping checks."""
    reply = session.console.send(command)
    session.actions.append(command)
    require(len(session.console.transcript) <= 8 * 1024 * 1024, 'bounded actual console transcript')
    return clean(reply)


def own_seek(session: step_cli.Session, predicate, description: str, count: int = 4096) -> dict:
    """Only an actual own-file, officially mapped PC can become an origin.

    Rust may expose inlined core::ptr rows between fixture statements. Those
    genuine foreign rows are skipped by real Wasm stepping, never relabelled.
    """
    for _ in range(count):
        reply = clean(session.send(f'bt {session.thread}'))
        source = re.findall(rb'(?m)^  source (.+):(\d+):(\d+)\r?$', reply)
        if len(source) == 1 and Path(source[0][0].decode()).name == session.source.name:
            position = session.position()
            if predicate(position):
                return position
        result = session.send(f'step wasm {session.thread}')
        require(b'guest exited:' not in result, 'fixture exited before actual required origin', description)
    raise AssertionError(('bounded real origin search exhausted', description))


def command_value(session: step_cli.Session, position: dict, expression: str) -> bytes:
    command = f'print {session.thread} {position["stop_id"]} {expression}'
    reply = query(session, command)
    repeat = query(session, command)
    require(reply == repeat, 'same-stop authenticated value response changed', (expression, reply, repeat))
    if b', unavailable=' in reply or b'control request rejected' in reply or b'error:' in reply:
        raise Unavailable(('actual source value unavailable', expression, reply.decode(errors='replace')))
    return reply


def scalar(session: step_cli.Session, position: dict, expression: str, expected: int, enumerator: bytes | None = None, frame: int | None = None) -> bytes:
    if frame is None:
        reply = command_value(session, position, expression)
    else:
        spelling = f'print {session.thread} {position["stop_id"]} {frame} {expression}'
        reply = query(session, spelling)
        require(reply == query(session, spelling), 'same-stop atomic frame value changed', reply)
        if b'error:' in reply or b', unavailable=' in reply:
            raise Unavailable(('real selected frame value unavailable', expression, frame, reply.decode(errors='replace')))
    if b'source-value stop=' in reply:
        actual = re.findall(rb', value=(-?\d+)(?: \([^\r\n]*\))?\r?$', reply, re.M)
        require(len(actual) == 1, 'selected actual scalar must have exactly one rendered numeric value', reply)
    else:
        actual = re.findall(rb'(?m)^source (?:local|parameter) ' + re.escape(expression.encode()) + rb' type=[^\r\n]* = [iu]\d+=(-?\d+)\r?$', reply)
        require(len(actual) == 1, 'actual scoped numeric reply missing', reply)
    require(int(actual[0]) == expected, 'actual copied/constant scalar differs from workload', (expression, reply))
    if enumerator is not None:
        require(b'(' + enumerator + b')' in reply, 'actual enumeration name missing', reply)
    return reply


def frame_list(session: step_cli.Session, position: dict) -> tuple[list[dict], int]:
    reply = query(session, f'frames {session.thread} {position["stop_id"]}')
    if b'error:' in reply:
        raise Unavailable(('actual frame commands unavailable', reply.decode(errors='replace')))
    header = re.search(rb'(?m)^source-frames stop=(\d+) thread=(\d+) selected=(\d+) count=(\d+)\r?$', reply)
    require(header is not None and int(header.group(1)) == position['stop_id'] and int(header.group(2)) == session.thread,
            'frame display differs from actual owned paused participant', reply)
    found = re.findall(rb'(?m)^  frame (\d+) kind=(inline|physical|caller) module=(\d+) function=(\d+) generation=(\d+) runtime-epoch=(\d+) scope-unit=(\d+) scope-offset=(\d+) variables=(current|caller-unavailable) name=([^\r\n]*)\r?$', reply)
    require(len(found) == int(header.group(4)) and 0 < len(found) <= 128 and b'source-frames end' in reply,
            'bounded complete actual frame display required', reply)
    rows = []
    for fields in found:
        ordinal, kind, module, function, generation, epoch, unit, offset, availability, name = fields
        require(int(ordinal) == len(rows) and int(generation) > 0 and int(epoch) > 0, 'actual frame ordering/generation invalid', fields)
        rows.append({'ordinal': int(ordinal), 'kind': kind.decode(), 'module': int(module), 'function': int(function),
                     'generation': int(generation), 'epoch': int(epoch), 'scope': (int(unit), int(offset)),
                     'current': availability == b'current', 'name': name.decode()})
    return rows, int(header.group(3))


def fixture_statement_rows(oracle: str, source: Path) -> list[dict]:
    """Official per-table file indices; never map a foreign row by line alone."""
    result = []
    for table in re.split(r'(?m)^debug_line\[', oracle)[1:]:
        files = {int(index): Path(name).name for index, name in
                 re.findall(r'file_names\[\s*(\d+)\]:\s*\n\s*name: "([^"\n]+)"', table)}
        if source.name not in files.values():
            continue
        for sequence in step_cli.line_sequences(table):
            result.extend(row for row in sequence if files.get(row['file']) == source.name and
                          row['is_statement'] and not row['end_sequence'])
    require(bool(result), 'official fixture statement rows absent', source)
    return result


def qualify(prefix: list[str], wasm: Path, source: Path, sequences: list[list[dict]], markers: dict[str, int],
            language: str, kind: str, log: Path, oracle: str) -> dict:
    exports = {name: metadata_cli.function(wasm, 'language_' + name + '_' + language)[0] for name in ('outer', 'leaf')}
    expressions = step_cli.code_expressions(wasm)
    own_rows = fixture_statement_rows(oracle, source)
    session = None
    row = {'case': kind, 'language': language, 'status': 'FAIL', 'passed': False,
           'source_sha256': sha(source), 'wasm_sha256': sha(wasm), 'log': str(log),
           'actual_product_source_value_qualified': False, 'selected_caller_values_qualified': False}
    try:
        session = step_cli.Session([*prefix, '--run', str(wasm)], log, step_cli.code_expressions(wasm), sequences, source)
        row.update(actual_argv=session.console.command, actions=session.actions, positions=session.positions)
        origin = exports['leaf'] if kind in ('finish', 'frame-selection', 'scalar-constants') else exports['outer']
        session.begin(origin)

        def available(marker: str) -> None:
            start = expressions[origin]
            end = min((value for value in expressions.values() if value > start), default=1 << 64)
            if not any(start <= row['address'] < end and row['line'] == markers[marker] for row in own_rows):
                raise Unavailable(('original compiler omitted required own-file statement row', marker,
                                   markers[marker], origin, start, end))

        def at(marker: str, inline: bool | None = None) -> dict:
            available(marker)
            return own_seek(session, lambda p: p['function'] == origin and p['line'] == markers[marker] and p['is_statement'] and
                            (inline is None or bool(p['inline']) == inline), marker)

        if kind == 'aggregate':
            position = at('LANG_OBJECT_READY')
            reply = query(session, f'ptype {session.thread} {position["stop_id"]} packet')
            if b'source-type stop=' not in reply:
                raise Unavailable(('actual producer object type unavailable', reply.decode(errors='replace')))
            require(b'name=grid' in reply, 'real packet array member metadata missing', reply)
            scalar(session, position, 'packet.grid[1][2]', 15)
            if language == 'cpp':
                scalar(session, position, 'packet.base', 17)
                scalar(session, position, 'packet.small', -3)
                scalar(session, position, 'packet.flags', 42)
                scalar(session, position, 'packet.mode', -1, b'cold')
            elif language == 'rust':
                explicit = command_value(session, position, 'packet.choice')
                require(re.search(rb'(?m)^\s*Negative: [^\r\n]*, active-variant', explicit) is not None and
                        re.search(rb'(?m)^\s*payload: [^\r\n]*, value=-9\r?$', explicit) is not None,
                        'real signed Rust variant/payload not selected from authenticated bytes', explicit)
                niche = command_value(session, position, 'packet.niche')
                require(re.search(rb'(?m)^\s*Some: [^\r\n]*, active-variant', niche) is not None and
                        re.search(rb', value=5\r?$', niche, re.M) is not None,
                        'real niche default variant or NonZero value unavailable', niche)
            row['actual_product_source_value_qualified'] = True
        elif kind == 'lexical':
            inside = at('LANG_INNER_SHADOW')
            scalar(session, inside, 'shadow', 23)
            outside = at('LANG_OUTER_SHADOW')
            require(outside['stop_id'] > inside['stop_id'], 'lexical scope query reused an old pause')
            scalar(session, outside, 'shadow', 7)
            stale = query(session, f'print {session.thread} {inside["stop_id"]} shadow')
            require(b'source-value stop=' not in stale and re.search(rb'(?m)^source (?:local|parameter) shadow ', stale) is None,
                    'stale stop label returned an actual source variable', stale)
            row['actual_product_source_value_qualified'] = True
        elif kind == 'inline-vars':
            available('LANG_INLINE_INNER')
            require('DW_TAG_inlined_subroutine' in oracle and 'language_inner_' + language in oracle and 'language_middle_' + language in oracle,
                    'actual producer failed to retain required inline DIEs')
            position = own_seek(session, lambda p: p['function'] == origin and p['line'] == markers['LANG_INLINE_INNER'] and
                p['is_statement'] and any('language_inner_' + language in label for label in p['inline']) and
                any('language_middle_' + language in label for label in p['inline']), 'actual nested concrete inline variable scope')
            scalar(session, position, 'inner_cookie', 4)
            row['actual_product_source_value_qualified'] = True
        elif kind == 'next':
            available('LANG_AFTER_CALL')
            at('LANG_PHYSICAL_CALL', False)
            for _ in range(24):
                position = session.source_step('over')
                require(position['function'] == exports['outer'] and not position['inline'], 'source next entered a physical/inline child', position)
                if position['line'] == markers['LANG_AFTER_CALL']:
                    break
            else:
                raise AssertionError('source next failed to reach actual after-call statement')
            scalar(session, position, 'child', 16)
            scalar(session, position, 'shadow', 23)
            row['actual_product_source_value_qualified'] = True
        elif kind == 'finish':
            before = at('LANG_LEAF_READY', False)
            require(exports['outer'] in before['physical_functions'], 'actual caller activation missing from leaf backtrace', before)
            position = session.source_step('out')
            require(position['function'] == exports['outer'] and not position['inline'], 'source finish failed to return to actual caller', position)
            leaked = query(session, f'print {session.thread} {position["stop_id"]} leaf_cookie')
            require(b'source-value stop=' not in leaked and re.search(rb'(?m)^source (?:local|parameter) leaf_cookie ', leaked) is None,
                    'returned callee local leaked into caller scope', leaked)
            row['gdb_finish_return_value_qualified'] = False
            row['finish_return_value_gap'] = 'out changes source activation; GDB finish return-value capture is a separate missing capability'
        elif kind == 'scalar-constants':
            position = at('LANG_LEAF_READY', False)
            negative_reply = scalar(session, position, 'leaf_negative', -31)
            positive_reply = scalar(session, position, 'leaf_positive', 23)
            reply = command_value(session, position, 'leaf_boolean')
            if b'source-value stop=' in reply:
                require(len(re.findall(rb', value=true\r?$', reply, re.M)) == 1, 'actual object bool is not canonical true', reply)
            else:
                require(len(re.findall(rb'(?m)^source (?:local|parameter) leaf_boolean type=[^\r\n]* = bool=true\r?$', reply)) == 1,
                        'actual scoped bool constant/carrier is not canonical true', reply)
            float_replies = []
            for name, width, bits in (('leaf_fraction', 32, 0x3fa00000),
                                      ('leaf_double', 64, 0xc004000000000000),
                                      ('leaf_zero', 32, 0x80000000)):
                actual_reply = command_value(session, position, name)
                # Match the renderer's complete target bit pattern independently
                # of host float formatting; original DIE origin follows below.
                require(re.search(rb'f' + str(width).encode() + rb' bits=0x0*' +
                                  format(bits, 'x').encode() + rb'(?:\r?$|[ \r\n])', actual_reply, re.M),
                        'actual floating constant bits differ from workload', actual_reply)
                float_replies.append((name, actual_reply))
            row['actual_product_source_value_qualified'] = True
            # Official DIE evidence is recorded separately below. A readable
            # variable may instead use a location expression or real guest copy.
            # Source literals therefore cannot qualify the direct constant path.
            receipts = []
            for name, actual_reply in (('leaf_negative', negative_reply), ('leaf_positive', positive_reply), ('leaf_boolean', reply), *float_replies):
                receipt = direct_constant_receipt(oracle, actual_reply, position, session.thread, name)
                if receipt is not None:
                    receipts.append(receipt)
            row['direct_const_attribute_names'] = list(scalar_constant_names(oracle))
            row['direct_const_receipts'] = receipts
            row['direct_const_path_exercised'] = bool(receipts)
        elif kind == 'frame-selection':
            before = at('LANG_LEAF_READY', False)
            require(exports['outer'] in before['physical_functions'], 'actual caller activation missing from backtrace', before)
            rows, _ = frame_list(session, before)
            require(rows[0]['kind'] == 'physical' and rows[0]['function'] == exports['leaf'] and
                    any(row['kind'] == 'caller' and row['function'] == exports['outer'] for row in rows),
                    'actual physical/caller frame identities missing', rows)
            selected = query(session, f'frame {session.thread} {before["stop_id"]} 0')
            require(b'selected=0' in selected, 'explicit current frame selection failed', selected)
            scalar(session, before, 'leaf_cookie', 16)
            upper = query(session, f'up {session.thread} {before["stop_id"]} 1')
            require(b'selected=1' in upper and b'kind=caller' in upper, 'up did not select actual caller identity', upper)
            scalar(session, before, 'shadow', 23)
            # A caller read must use its genuine saved source PC and packet.
            # The caller has no callee leaf_cookie, even though its frame is active.
            unavailable = query(session, f'print {session.thread} {before["stop_id"]} leaf_cookie')
            require(b'error:' in unavailable and b'source-value stop=' not in unavailable,
                    'caller ordinal incorrectly borrowed callee/local bytes', unavailable)
            lower = query(session, f'down {session.thread} {before["stop_id"]} 1')
            require(b'selected=0' in lower, 'down did not restore actual current frame', lower)
            scalar(session, before, 'leaf_cookie', 16)
            rejected = query(session, f'frame {session.thread} {before["stop_id"]} 18446744073709551615')
            require(b'outside the current authenticated frame list; selection unchanged' in rejected, 'large ordinal not rejected before access', rejected)
            _, still_selected = frame_list(session, before)
            require(still_selected == 0, 'failed selection mutated selected frame')
            row['selected_caller_values_qualified'] = True
            row['caller_cross_scope_guard_qualified'] = True
            row['actual_frame_commands_qualified'] = True
            row['actual_product_source_value_qualified'] = True
        elif kind == 'inline-frame-selection':
            available('LANG_INLINE_INNER')
            before = own_seek(session, lambda p: p['function'] == origin and p['line'] == markers['LANG_INLINE_INNER'] and
                p['is_statement'] and any('language_inner_' + language in name for name in p['inline']) and
                any('language_middle_' + language in name for name in p['inline']), 'genuine nested producer inline scopes')
            rows, _ = frame_list(session, before)
            matching = {name: [row['ordinal'] for row in rows if row['current'] and ('language_' + name + '_' + language) in row['name']]
                        for name in ('inner', 'middle', 'outer')}
            require(all(len(indices) == 1 for indices in matching.values()), 'one concrete inline/physical frame per fixture name', matching)
            inner, middle, outer = (matching[name][0] for name in ('inner', 'middle', 'outer'))
            require(inner < middle < outer, 'real source frame order does not follow concrete inline path', rows)
            scalar(session, before, 'shadow', 104, frame=inner)
            scalar(session, before, 'shadow', 204, frame=middle)
            scalar(session, before, 'shadow', 7, frame=outer)
            reply = query(session, f'frame {session.thread} {before["stop_id"]} {inner}')
            require(f'selected={inner}'.encode() in reply, 'inner frame not selected', reply)
            reply = query(session, f'up {session.thread} {before["stop_id"]} {middle - inner}')
            require(f'selected={middle}'.encode() in reply, 'up did not select outer concrete inline frame', reply)
            scalar(session, before, 'shadow', 204)
            # GDB step/next execute the innermost activation regardless of the
            # inspection cursor. Only finish uses the selected frame as a target.
            after = session.source_step('over')
            require(after['stop_id'] > before['stop_id'] and after['function'] == origin and
                    any('language_' + scope + '_' + language in name for name in after['inline'] for scope in ('inner', 'middle')),
                    'next from a selected outer frame did not execute the genuine innermost activation', after)
            _, selected = frame_list(session, after)
            require(selected == 0, 'a resumed stop retained the old inspection cursor', selected)
            stale = query(session, f'frame {session.thread} {before["stop_id"]} {middle}')
            require(b'error:' in stale and b'source-frames stop=' not in stale, 'retired stop reused an inline selection', stale)
            row['actual_frame_commands_qualified'] = True
            row['actual_selected_outer_next_qualified'] = True
            row['actual_product_source_value_qualified'] = True
        else:
            raise AssertionError('unknown independent recipe case')
        session.finish_guest()
        row.update(status='PASS', passed=True)
    except Unavailable as error:
        row.update(status='UNAVAILABLE', error=repr(error))
    except BaseException as error:
        row.update(status='FAIL', error=repr(error))
    finally:
        if session is not None:
            try:
                session.console.finish()
            except BaseException as error:
                row.update(status='FAIL', passed=False, finish_error=repr(error))
        if log.is_file():
            row['log_sha256'] = sha(log)
        log.with_suffix('.case.json').write_text(json.dumps(row, indent=2) + '\n')
    return row


def official_dies(oracle: str) -> dict[tuple[int, int], dict]:
    """Read the official LLVM dump's concrete identity tree, never DWARF bytes.

    Reference spellings are LLVM's actual absolute DIE offsets in this one
    .debug_info image. No supplementary/host symbol/file lookup is permitted.
    """
    records = {}
    unit = None
    current = None
    stack = []
    for line in oracle.splitlines():
        if match := re.match(r'^0x([0-9a-fA-F]+):\s+Compile Unit:', line):
            unit = int(match.group(1), 16)
            current = None
            stack = []
            continue
        if match := re.match(r'^0x([0-9a-fA-F]+):(\s+)(DW_TAG_[A-Za-z_0-9]+|NULL)\b', line):
            require(unit is not None, 'official DIE missing actual CU header', line)
            offset, indent, tag = int(match.group(1), 16), len(match.group(2)), match.group(3)
            while stack and stack[-1][0] >= indent:
                stack.pop()
            current = None
            if tag == 'NULL':
                continue
            key = (unit, offset)
            require(key not in records and len(records) < 262144, 'duplicate or excessive official DIE identity', key)
            current = {'key': key, 'tag': tag, 'parent': stack[-1][1] if stack else None, 'attributes': {}}
            records[key] = current
            stack.append((indent, key))
            continue
        if current is not None and (match := re.match(r'^\s+(DW_AT_[A-Za-z_0-9]+)(?:\s+\[[^]\r\n]+\])?\s+(.*)$', line)):
            name, value = match.groups()
            require(name not in current['attributes'], 'duplicate official attribute', (current['key'], name))
            current['attributes'][name] = value
    return records


def official_reference(records: dict, value: str) -> tuple[int, int] | None:
    match = re.match(r'^\(0x([0-9a-fA-F]+)\b', value)
    if match is None:
        return None
    offset = int(match.group(1), 16)
    found = [key for key in records if key[1] == offset]
    require(len(found) == 1, 'official reference must identify exactly one owned DIE', value)
    return found[0]


def official_inherited(records: dict, key: tuple[int, int], attribute: str, ancestors: tuple = ()) -> str | None:
    require(key in records and key not in ancestors and len(ancestors) < 16, 'official inherited metadata cycle/bound', key)
    fields = records[key]['attributes']
    if attribute in fields:
        return fields[attribute]
    for link in ('DW_AT_specification', 'DW_AT_abstract_origin'):
        if link in fields:
            target = official_reference(records, fields[link])
            require(target is not None, 'official metadata reference has no concrete target', fields[link])
            value = official_inherited(records, target, attribute, ancestors + (key,))
            if value is not None:
                return value
    return None


def scalar_constant_names(oracle: str) -> tuple[str, ...]:
    # Inventory only. It cannot qualify a direct constant product path because
    # a different same-name DIE/carrier could supply the actual displayed value.
    records = official_dies(oracle)
    found = set()
    for key, record in records.items():
        if record['tag'] not in ('DW_TAG_variable', 'DW_TAG_formal_parameter') or 'DW_AT_const_value' not in record['attributes']:
            continue
        name = official_inherited(records, key, 'DW_AT_name')
        for expected in ('leaf_negative', 'leaf_positive', 'leaf_boolean', 'leaf_fraction', 'leaf_double', 'leaf_zero'):
            if name is not None and re.fullmatch(r'\("' + re.escape(expected) + r'"\)', name):
                found.add(expected)
    return tuple(sorted(found))


def direct_constant_receipt(oracle: str, reply: bytes, position: dict, thread: int, expected_name: str) -> dict | None:
    lines = [line for line in clean(reply).splitlines() if line.startswith(b'source-origin ')]
    if not lines:
        return None  # Real copied locals/DW_OP constants remain other paths.
    require(len(lines) == 1, 'one exact scalar origin receipt required', lines)
    pattern = (rb'source-origin stop=(\d{1,20}) thread=(\d{1,20}) code-offset=(\d{1,20}) '
               rb'variable-unit=(\d{1,20}) variable-offset=(\d{1,20}) scope-unit=(\d{1,20}) scope-offset=(\d{1,20}) '
               rb'type-unit=(\d{1,20}) type-offset=(\d{1,20}) kind=DW_AT_const_value')
    match = re.fullmatch(pattern, lines[0])
    require(match is not None, 'complete direct constant origin receipt required', lines)
    values = list(map(int, match.groups()))
    require(all(value < 1 << 64 for value in values), 'origin identity outside actual width', values)
    stop, participant, pc = values[:3]
    variable, scope, type_key = tuple(values[3:5]), tuple(values[5:7]), tuple(values[7:9])
    require((stop, participant, pc) == (position['stop_id'], thread, position['code_offset']),
            'direct constant provenance differs from actual stopped activation/Code PC', (values, position))
    records = official_dies(oracle)
    require(variable in records and scope in records and type_key in records, 'product origin absent from official actual DWARF', values)
    record = records[variable]
    require(record['tag'] in ('DW_TAG_variable', 'DW_TAG_formal_parameter') and 'DW_AT_const_value' in record['attributes'] and
            'DW_AT_location' not in record['attributes'], 'direct constant path lacks unique direct storage attribute', record)
    require(official_inherited(records, variable, 'DW_AT_name') == '("' + expected_name + '")',
            'actual concrete constant DIE name differs from requested variable', record)
    type_value = official_inherited(records, variable, 'DW_AT_type')
    require(type_value is not None and official_reference(records, type_value) == type_key,
            'actual product constant type identity differs from official reference', (type_key, type_value))
    parent = record['parent']
    scopes = {'DW_TAG_compile_unit', 'DW_TAG_subprogram', 'DW_TAG_inlined_subroutine', 'DW_TAG_lexical_block', 'DW_TAG_try_block', 'DW_TAG_catch_block'}
    depth = 0
    while parent is not None and records[parent]['tag'] not in scopes:
        require(depth < 64, 'official scoped identity traversal bound', parent)
        parent = records[parent]['parent']
        depth += 1
    require(parent == scope, 'constant origin scope differs from actual concrete DIE parent', (parent, scope))
    return {'name': expected_name, 'stop': stop, 'thread': participant, 'code_offset': pc,
            'variable': list(variable), 'scope': list(scope), 'type': list(type_key), 'kind': 'DW_AT_const_value'}


def source_fingerprint(root: Path) -> dict[str, str]:
    files = sorted(p for p in (root / 'src').rglob('*') if p.is_file() and p.name != '.DS_Store' and not p.name.startswith('._'))
    return {str(p.relative_to(root)): sha(p) for p in files}


def build_proof(binary: Path, receipt_path: Path, source_before: dict[str, str]) -> tuple[dict, list[Path]]:
    receipt = json.loads(receipt_path.read_text())
    require(Path(receipt['binary_path']).resolve(strict=True) == binary and receipt['binary_sha256'] == sha(binary), 'product build receipt/hash mismatch')
    argv = receipt['link_argv']
    require(receipt['link_returncode'] == 0 and isinstance(argv, list) and argv and all(isinstance(v, str) for v in argv), 'successful actual product link argv required')
    outputs = [argv[i + 1] for i, value in enumerate(argv[:-1]) if value == '-o']
    require(len(outputs) == 1, 'one actual link output required')
    output = Path(outputs[0])
    if not output.is_absolute():
        output = Path(receipt['link_cwd']).resolve(strict=True) / output
    require(output.resolve(strict=True) == binary, 'receipt link output/cwd differs from actual passed product')
    before, after = (Path(receipt[name]).resolve(strict=True) for name in ('source_before_file', 'source_after_file'))
    require(sha(before) == sha(after), 'actual product build changed its input closure')
    fingerprint = json.loads(before.read_text())
    entries = fingerprint['files']
    require(entries and len({row['path'] for row in entries}) == len(entries), 'unique actual dependency fingerprint required')
    canonical = json.dumps(entries, sort_keys=True, separators=(',', ':')).encode()
    require(fingerprint['source_id'] == 'sha256:' + hashlib.sha256(canonical).hexdigest(), 'actual build source_id mismatch')
    files = {row['path']: row['sha256'] for row in entries}
    require({name for name in files if name.startswith('src/')} == set(source_before) and
            all(files[name] == value for name, value in source_before.items()), 'product is stale relative to actual complete src tree')
    return fingerprint, [before, after]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source-root', 'out', 'uwvm', 'build-receipt', 'wasm-clang', 'wasm-ld', 'rustc', 'wasm-tools', 'llvm-dwarfdump'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--ros', action='store_true')
    parser.add_argument('--language', action='append', choices=('c', 'cpp', 'rust'))
    parser.add_argument('--dwarf-version', type=int, action='append', choices=(4, 5))
    parser.add_argument('--optimization', type=int, action='append', choices=(0, 1, 2))
    parser.add_argument('--diagnostic-policy', action='append', choices=('instruction', 'unwind'))
    parser.add_argument('--case', action='append', choices=CASES)
    args = parser.parse_args()
    for value in (args.language, args.dwarf_version, args.optimization, args.diagnostic_policy, args.case):
        require(value is None or len(value) == len(set(value)), 'duplicate requested tuple would overwrite a fresh receipt')
    require(sys.platform == 'linux', 'only keeper controlled Linux may compile/run this recipe')
    root = args.source_root.resolve(strict=True)
    helpers = (Path(__file__).resolve(strict=True), Path(metadata_cli.__file__).resolve(strict=True), Path(step_cli.__file__).resolve(strict=True))
    expected = tuple(root / 'test/0017.runtime' / name for name in ('run_debug_language_experience_cli.py', 'run_debug_source_inline_metadata_cli.py', 'run_debug_source_step_cli.py'))
    require(helpers == expected, 'actual runner and helper imports must match exact source root')
    guard = root / 'tools/ci/require_wasm3_test_cgroup.sh'
    subprocess.run(['bash', str(guard)], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    binary, receipt_path = args.uwvm.resolve(strict=True), args.build_receipt.resolve(strict=True)
    source_before = source_fingerprint(root)
    fingerprint, fingerprints = build_proof(binary, receipt_path, source_before)
    # Preserve driver spelling: resolving wasm-ld to lld changes its flavor.
    # Hashing opens the actual referent; strict resolution separately proves existence.
    tools = {name: getattr(args, name).absolute() for name in ('wasm_clang', 'wasm_ld', 'rustc', 'wasm_tools', 'llvm_dwarfdump')}
    for tool in tools.values():
        tool.resolve(strict=True)
    sources = {name: root / 'test/0017.runtime/fixtures' / ('debug_language_experience_' + name + extension)
               for name, extension in (('c', '.c'), ('cpp', '.cc'), ('rust', '.rs'))}
    immutable = [binary, receipt_path, *fingerprints, guard, *helpers, *sources.values(), *tools.values()]
    inputs_before = {str(path): sha(path) for path in immutable}
    summary = {'passed': False, 'qualification': 'actual language values/scopes/stepping, missing capabilities fail; no GDB/LLDB parity',
               'build_source_id': fingerprint['source_id'], 'build_receipt_sha256': sha(receipt_path), 'product_sha256': sha(binary),
               'source_before': source_before, 'inputs_before': inputs_before, 'tools': {}, 'commands': [], 'fixtures': [], 'cases': [],
               'host_expression_execution': False, 'selected_caller_values_qualified': False, 'candidate_code_imported_by_product': True}

    def run(argv: list[str], name: str) -> str:
        log = out / (name + '.log')
        command = {'argv': argv, 'cwd': str(root), 'log': str(log), 'returncode': None}
        summary['commands'].append(command)
        with log.open('wb') as stream:
            completed = subprocess.run(argv, cwd=root, stdout=stream, stderr=subprocess.STDOUT, timeout=180, check=False)
        command.update(returncode=completed.returncode, log_sha256=sha(log))
        require(completed.returncode == 0, 'actual official tool failed', command)
        require(log.stat().st_size <= 16 * 1024 * 1024, 'official tool output budget exceeded')
        return log.read_text(errors='strict')

    try:
        languages = args.language or ('c', 'cpp', 'rust')
        requested = args.case or CASES
        for name, tool in tools.items():
            summary['tools'][name] = {'path': str(tool), 'sha256': sha(tool), 'version': run([str(tool), '--version'], 'version-' + name)}
        if 'rust' in languages:
            libdir = Path(run([str(tools['rustc']), '--print', 'target-libdir', '--target', 'wasm32-unknown-unknown'], 'rust-target-libdir').strip()).resolve(strict=True)
            require(len(list(libdir.glob('libcore-*.rlib'))) == 1, 'one actual installed Wasm Rust core required')
            libraries = sorted({p.resolve(strict=True) for pattern in ('libcore-*.rlib', 'libcompiler_builtins-*.rlib') for p in libdir.glob(pattern)})
            immutable.extend(libraries)
            inputs_before.update({str(p): sha(p) for p in libraries})
        for language in languages:
            source = sources[language]
            markers = {}
            for marker in MARKERS:
                lines = [i for i, line in enumerate(source.read_text().splitlines(), 1) if marker in line]
                require(len(lines) == 1, 'one exact producer fixture marker required', marker)
                markers[marker] = lines[0]
            for version in args.dwarf_version or (4, 5):
                for optimization in args.optimization or (0, 1):
                    stem = f'{language}-dwarf{version}-O{optimization}'
                    wasm = out / (stem + '.wasm')
                    if language == 'rust':
                        run([str(tools['rustc']), '--edition=2024', '--crate-name', 'debug_language_experience', '--target', 'wasm32-unknown-unknown',
                             '-C', 'panic=abort', '-C', 'debuginfo=2', '-C', f'dwarf-version={version}', '-C', 'split-debuginfo=off',
                             '-C', f'opt-level={optimization}', '-C', 'codegen-units=1', '-C', 'target-feature=+bulk-memory',
                             '-C', 'linker=' + str(tools['wasm_ld']), '-C', 'link-arg=--no-entry', '-C', 'link-arg=-O0', str(source), '-o', str(wasm)], 'compile-' + stem)
                    else:
                        obj = out / (stem + '.o')
                        argv = [str(tools['wasm_clang']), '--target=wasm32-unknown-unknown', '-g', f'-gdwarf-{version}', f'-O{optimization}',
                                '-std=c++23' if language == 'cpp' else '-std=c17', '-ffreestanding', '-nostdlib', '-mbulk-memory']
                        if language == 'cpp':
                            argv.extend(('-fno-exceptions', '-fno-rtti'))
                        run([*argv, '-c', str(source), '-o', str(obj)], 'compile-' + stem)
                        run([str(tools['wasm_ld']), '-O0', '--no-entry', '--export-all', str(obj), '-o', str(wasm)], 'link-' + stem)
                    immutable.append(wasm)
                    emitted_sha = sha(wasm)
                    inputs_before[str(wasm)] = emitted_sha
                    run([str(tools['wasm_tools']), 'validate', str(wasm)], 'wasm-validate-' + stem)
                    try:
                        run([str(tools['llvm_dwarfdump']), '--verify', str(wasm)], 'dwarf-verify-' + stem)
                    except AssertionError as error:
                        diagnostic = summary['commands'][-1]
                        require(diagnostic['returncode'] is not None and diagnostic['returncode'] != 0,
                                'only actual official verification rejection can skip execution', diagnostic)
                        summary['fixtures'].append({'name': stem, 'source_sha256': sha(source),
                            'wasm_sha256': emitted_sha, 'official_dwarf_verified': False,
                            'official_verification': diagnostic})
                        for policy in args.diagnostic_policy or ('instruction', 'unwind'):
                            for kind in requested:
                                summary['cases'].append({'case': kind, 'language': language, 'dwarf_version': version,
                                    'optimization': optimization, 'diagnostic_policy': policy, 'status': 'UNAVAILABLE',
                                    'passed': False, 'executed': False, 'reason': 'original compiler DWARF rejected by official verifier',
                                    'official_verification': diagnostic, 'error': repr(error), 'wasm_sha256': emitted_sha})
                        continue
                    oracle = run([str(tools['llvm_dwarfdump']), '--show-form', '--debug-info', '--debug-line', '--debug-ranges', '--debug-rnglists', str(wasm)], 'dwarf-oracle-' + stem)
                    require(sha(wasm) == emitted_sha, 'compiler module changed during official tool validation')
                    require('DW_AT_name' in oracle and 'LanguagePacket' in oracle and 'grid' in oracle, 'real producer packet metadata missing')
                    sequences = step_cli.line_sequences(oracle)
                    step_cli.code_expressions(wasm)
                    summary['fixtures'].append({'name': stem, 'source_sha256': sha(source), 'wasm_sha256': emitted_sha, 'markers': markers,
                        'const_value_attribute_count': oracle.count('DW_AT_const_value'), 'direct_const_scalar_names': list(scalar_constant_names(oracle)), 'variant_part_count': oracle.count('DW_TAG_variant_part'),
                        'inline_die_count': oracle.count('DW_TAG_inlined_subroutine'), 'linker_size_optimization': 0,
                        'actual_import_policy': 'all actual imports rejected'})
                    for policy in args.diagnostic_policy or ('instruction', 'unwind'):
                        mode = ['-Raot'] if args.ros else ['-Rcc', 'jit', '-Rcm', 'full']
                        prefix = [str(binary), '-m', 'debug-jit', *mode, '-Rct', '0', '-Rllvm-call-stack', policy,
                                  '-Rllvm-exception-dispatch', 'native-unwind', '-Rllvm-cache-path', 'disable']
                        for kind in requested:
                            require(sha(wasm) == emitted_sha, 'product must consume exact officially verified producer module')
                            row = qualify(prefix, wasm, source, sequences, markers, language, kind, out / f'{stem}-{policy}-{kind}.log', oracle)
                            row.update(dwarf_version=version, optimization=optimization, diagnostic_policy=policy)
                            summary['cases'].append(row)
                            require(sha(wasm) == emitted_sha, 'producer module changed during actual product query')
        subprocess.run(['bash', str(guard)], check=True)
        summary['inputs_after'] = {str(path): sha(path) for path in immutable}
        summary['source_after'] = source_fingerprint(root)
        require(summary['inputs_after'] == inputs_before and summary['source_after'] == source_before, 'source/tool/product/fixture changed during qualification')
        summary['passed'] = bool(summary['cases']) and all(row['passed'] for row in summary['cases'])
        summary['all_required_capabilities_passed'] = summary['passed'] and set(requested) == set(CASES)
        summary['complete_requested_matrix'] = (set(languages) == {'c', 'cpp', 'rust'} and set(args.dwarf_version or (4, 5)) == {4, 5} and
            set(args.optimization or (0, 1)) == {0, 1} and set(args.diagnostic_policy or ('instruction', 'unwind')) == {'instruction', 'unwind'} and
            set(requested) == set(CASES))
        summary['selected_caller_values_qualified'] = any(row.get('selected_caller_values_qualified', False) for row in summary['cases'])
        summary['requested_cases'] = list(requested)
        summary['actual_direct_scalar_constants_qualified'] = (any(row.get('direct_const_path_exercised', False) for row in summary['cases']) and
            all(row['passed'] for row in summary['cases'] if row.get('direct_const_path_exercised', False)))
    except BaseException as error:
        summary['error'] = repr(error)
        raise
    finally:
        (out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(('PASS' if summary['passed'] else 'FAIL') + f' actual language experience requested subset; {len(summary["cases"])} cases; {out / "summary.json"}')
    return 0 if summary['passed'] else 2


if __name__ == '__main__':
    raise SystemExit(main())
