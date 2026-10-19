#!/usr/bin/env python3
"""Stream official flat WAT and count lexical pre-opcode control depth.

No guest executes. This deliberately does not estimate reachability, actual
retained safepoints, vector capacities, allocator overhead or peak live bytes.
"""
import argparse
import csv
import json
from pathlib import Path
import re
import resource
import subprocess
import sys
import time

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from build_products import admission, digest
from inspect_wasm import Reader, inspect
from owned_process import remember_process, terminate_group, owned_process_record
from vtune_reports import durable_json, require


def bodies(path, imports):
    data = Path(path).read_bytes()
    reader = Reader(data)
    require(reader.data[:8] == b'\0asm\x01\0\0\0', 'not a core module')
    reader.pos = 8
    result = []
    while reader.pos < len(data):
        tag, size = reader.byte(), reader.leb()
        end = reader.pos + size
        require(end <= len(data), 'section extends past module')
        if tag == 10:
            count = reader.leb()
            for ordinal in range(count):
                size = reader.leb()
                start, body_end = reader.pos, reader.pos + size
                groups, locals_count = reader.leb(), 0
                for _ in range(groups):
                    locals_count += reader.leb()
                    reader.value_type()
                require(start < reader.pos <= body_end <= end and data[body_end - 1] == 0x0b,
                        'bad body/local declaration extent')
                result.append({'index': imports + ordinal, 'body_start': start, 'body_end': body_end,
                               'body_bytes': size, 'code_start': reader.pos, 'declared_locals': locals_count})
                reader.pos = body_end
            require(reader.pos == end, 'code body count/section extent mismatch')
        reader.pos = end
    require(result, 'missing code bodies')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wasm', type=Path, required=True)
    parser.add_argument('--tool', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--expected-bodies', type=int, default=8841)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    before = admission()
    boot = Path('/proc/sys/kernel/random/boot_id').read_text().strip()
    shape = inspect(args.wasm)
    imports = sum(item['kind'] == 0 for item in shape['imports'])
    raw_bodies = bodies(args.wasm, imports)
    require(len(raw_bodies) == shape['code_bodies'] == args.expected_bodies, 'defined function count differs')
    args.out.mkdir(parents=True, exist_ok=False)
    command = [str(args.tool), 'print', '-p', '--indent', '0', str(args.wasm)]
    report = {'passed': False, 'purpose': 'static lexical control-layout scale diagnostic',
              'executes_guest': False, 'command': command, 'wasm': str(args.wasm),
              'wasm_sha256': digest(args.wasm), 'tool_sha256': digest(args.tool),
              'helper_sha256': digest(__file__), 'boot': boot, 'admission_before': before,
              'defined_functions_expected': len(raw_bodies), 'imported_functions': imports,
              'rows': [], 'unknown_lines': [],
              'counting_rule': 'one implicit function control; depth immediately before each printed opcode, including explicit end/else and the binary function end represented by WAT closing parenthesis',
              'estimate_rule': '104 * sum_pre_control_depth; illustrative x86_64 control_layout object size only, not measured allocation, retained sites, capacities, tuple storage or RSS',
              'limitations': 'all lexical opcode sites include unreachable code; fused validator can skip unreachable sites or hit quotas; vectors/slots and temporary copies are not modeled'}
    process, current, stack, previous_offset = None, None, None, None
    started, last_observe = time.monotonic(), 0
    offset_line = re.compile(r'^\(;@([0-9a-fA-F]+)\s*;\)(.*)$')
    function = re.compile(r'^\(func(?:\s|\))')
    opcode = re.compile(r'^([a-z][a-z0-9_.]*)(?:\s|$)')

    def site(offset, op):
        nonlocal previous_offset
        require(current['code_start'] <= offset < current['body_end'], 'opcode outside binary body')
        require(previous_offset is None or offset > previous_offset, 'nonmonotonic/duplicate opcode offset')
        if previous_offset is None:
            require(offset == current['code_start'], 'first WAT opcode does not match binary local declarations')
        previous_offset = offset
        current['opcode_sites'] += 1
        current['sum_pre_control_depth'] += len(stack)
        current['max_control_depth'] = max(current['max_control_depth'], len(stack))
        if op in ('block', 'loop', 'if'):
            stack.append(op)
            current[op + '_sites'] += 1
            current['max_control_depth'] = max(current['max_control_depth'], len(stack))
        elif op == 'else':
            require(stack[-1] == 'if', 'else without a live if')
            stack[-1] = 'else'
        elif op == 'end':
            require(len(stack) > 1, 'explicit WAT end popped the implicit function')
            stack.pop()
        else:
            require(op not in ('try', 'try_table', 'catch', 'catch_all', 'delegate', 'resume', 'resume_throw', 'switch'),
                    'unsupported control opcode: ' + op)

    try:
        with (args.out / 'parser.stderr.log').open('wb') as error, (args.out / 'format-prefix.log').open('w') as prefix:
            process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=error, text=True,
                                       encoding='utf-8', errors='strict', start_new_session=True)
            remember_process(process)
            for line_number, raw in enumerate(process.stdout, 1):
                if line_number <= 64:
                    prefix.write(raw)
                if time.monotonic() - last_observe >= .5:
                    remember_process(process)
                    require(time.monotonic() - started < 300, 'parser/analyzer exceeded 300 seconds')
                    last_observe = time.monotonic()
                match = offset_line.fullmatch(raw.rstrip('\n'))
                if not match:
                    require(current is None, 'unoffset line inside a function')
                    continue
                offset, text = int(match[1], 16), match[2].strip()
                if function.match(text):
                    require(current is None and len(report['rows']) < len(raw_bodies), 'nested/excess function')
                    body = raw_bodies[len(report['rows'])]
                    require(offset == body['body_start'], 'WAT function order/body offset differs')
                    index_match = re.search(r'\(;([0-9]+);\)', text)
                    require(index_match and int(index_match[1]) == body['index'], 'WAT/binary function index differs')
                    current = dict(body, name=shape['functions'].get(body['index']), opcode_sites=0,
                                   sum_pre_control_depth=0, max_control_depth=1,
                                   block_sites=0, loop_sites=0, if_sites=0)
                    stack, previous_offset = ['function'], None
                elif current is not None:
                    if text == ')':
                        require(offset == current['body_end'] - 1 and stack == ['function'], 'function closes with unbalanced controls/offset')
                        site(offset, 'function-end')
                        current['estimated_control_objects_bytes_104'] = 104 * current['sum_pre_control_depth']
                        report['rows'].append(current)
                        current, stack, previous_offset = None, None, None
                    elif text.startswith('(local '):
                        require(offset < current['code_start'] and previous_offset is None, 'local declaration after opcode')
                    else:
                        instruction = opcode.match(text)
                        if instruction is None:
                            report['unknown_lines'].append({'line': line_number, 'offset': offset, 'text': text[:300]})
                            raise ValueError('unrecognized WAT instruction line')
                        site(offset, instruction[1])
            report['parser_exit'] = process.wait(timeout=10)
            require(report['parser_exit'] == 0, 'official parser failed; see stderr')
        require(current is None and len(report['rows']) == len(raw_bodies), 'incomplete function/body count')
        report['totals'] = {name: sum(row[name] for row in report['rows']) for name in
                            ('body_bytes', 'opcode_sites', 'sum_pre_control_depth', 'estimated_control_objects_bytes_104')}
        report['max_control_depth'] = max(row['max_control_depth'] for row in report['rows'])
        report['largest_20_by_lexical_control_sum'] = sorted(report['rows'], key=lambda row: row['sum_pre_control_depth'], reverse=True)[:20]
        with (args.out / 'functions.csv').open('w', newline='') as stream:
            writer = csv.DictWriter(stream, fieldnames=list(report['rows'][0]))
            writer.writeheader()
            writer.writerows(report['rows'])
        require(digest(args.wasm) == report['wasm_sha256'] and digest(args.tool) == report['tool_sha256'], 'input/tool changed')
        report['passed'] = True
    except BaseException as error:
        report['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        terminate_group(process)
        report['ownership'] = owned_process_record(process)
        report['admission_after'] = admission()
        report['elapsed_seconds'] = time.monotonic() - started
        if (boot != Path('/proc/sys/kernel/random/boot_id').read_text().strip()
                or report['admission_after']['membership'] != before['membership']
                or report['admission_after']['memory_events'] != before['memory_events']
                or not report['ownership']['closed']):
            report['passed'] = False
            report['final_guard_failure'] = True
        durable_json(args.out / 'receipt.json', report)
    print(json.dumps({key: report[key] for key in ('passed', 'totals', 'max_control_depth', 'elapsed_seconds')}), flush=True)


if __name__ == '__main__':
    main()
