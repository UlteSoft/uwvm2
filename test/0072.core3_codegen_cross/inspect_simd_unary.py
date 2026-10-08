#!/usr/bin/env python3
"""Record instructions in authenticated, executed SIMD unary function extents.

The cache reader proves fresh-process execution and bounds every ELF function.
These observations expose vector use, scalar fallbacks and helper calls; they
do not measure hardware throughput or assert that a lowering is optimal.
"""
import json
from pathlib import Path
import re
import sys

from inspect_authenticated_cache import main as inspect_authenticated
from run_matrix import digest, save, verify


def main():
    inspect_authenticated()
    config_path = Path(sys.argv[1])
    config = json.loads(config_path.read_text())
    output = Path(config['output'])
    qualification_path = output / 'summary.json'
    qualification = json.loads(qualification_path.read_text())
    assert qualification['passed']
    rows = []
    for row in qualification['rows']:
        match = re.fullmatch(r'(f32x4|f64x2)-(abs|neg|ceil|floor|trunc)-special-bits', row['fixture'])
        assert match and len(row['functions']) == 1
        function = row['functions'][0]
        assert function['function'].endswith('_func_0')
        assert function['disassembly_extent'] == 'bounded-ELF-function'
        assembly_path = output / (row['profile'] + '--' + row['fixture']) / 'function-0.asm'
        assert digest(assembly_path) == function['assembly_sha256']
        instructions = []
        histogram = {}
        for line in assembly_path.read_text().splitlines():
            instruction = re.match(r'^\s*[0-9a-f]+:\s+([A-Za-z][A-Za-z0-9_.]*)\b(.*)$', line)
            if instruction and not instruction[1].startswith('R_'):
                instructions.append(line)
                opcode = instruction[1]
                histogram[opcode] = histogram.get(opcode, 0) + 1
        assert len(instructions) == function['instruction_lines']
        rows.append(dict(profile=row['profile'], fixture=row['fixture'],
            lane_type=match[1], operation=match[2],
            function=function['function'], section=function['section'],
            start=function['start'], size=function['size'],
            object_sha256=function['object_sha256'],
            assembly_path=str(assembly_path), assembly_sha256=digest(assembly_path),
            instruction_lines=len(instructions),
            link_setting_calls=function['link_setting_calls'],
            opcode_histogram=histogram, instructions=instructions))
    verify(config['pins'])
    save(output / 'simd-unary-instruction-observations.json', dict(passed=True,
        arch=config['arch'], rows=rows, completed=len(rows),
        source_identities=qualification['source_identities'],
        product_sha256=qualification['product_sha256'],
        authenticated_assembly_qualification_sha256=digest(qualification_path),
        configuration_sha256=digest(config_path), code_sha256=digest(__file__),
        hardware_efficiency_qualified=False,
        scope='Complete exported SIMD unary function 0 in actual authenticated executed objects; instruction observations only.'))
    print('PASS bounded SIMD unary instruction observations', config['arch'], len(rows), flush=True)


if __name__ == '__main__':
    main()
