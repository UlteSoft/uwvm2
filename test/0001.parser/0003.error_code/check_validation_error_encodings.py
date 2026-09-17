#!/usr/bin/env python3
"""Partition the production diagnostic dispatch to bound compiler memory.

Every selected case keeps its original production body and test payload. Only
unselected case instantiations are discarded in a temporary header overlay.
This checks formatter output, not the compilation cost of the unsplit formatter.
Compare each product/encoding with an independently run whole char formatter.
"""
import argparse
import concurrent.futures
import json
import os
from pathlib import Path
import re
import resource
import subprocess
import tempfile


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--full-src', type=Path, required=True)
    p.add_argument('--ros-src', type=Path, required=True)
    p.add_argument('--baseline', type=Path, required=True)
    p.add_argument('--cxx', default=os.environ.get('CXX', 'clang++'))
    p.add_argument('--out', type=Path)
    p.add_argument('--jobs', type=int, default=1)
    p.add_argument('--chunk', type=int, default=8)
    p.add_argument('--first-code', type=int, default=0)
    p.add_argument('--end-code', type=int)
    a = p.parse_args()
    if a.jobs < 1 or a.chunk < 1:
        p.error('jobs and chunk must be positive')
    a.full_src, a.ros_src = a.full_src.resolve(), a.ros_src.resolve()
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    out = a.out or Path(tempfile.mkdtemp(prefix='uwvm-error-encodings-', dir='/tmp'))
    out = out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    baseline = a.baseline.read_text()
    jobs = []
    for product, tree in [('full', a.full_src), ('ros', a.ros_src)]:
        enum_header = (tree/'src/uwvm2/validation/error/error.h').read_text(encoding='utf-8-sig')
        enum_body = re.search(r'enum class code_validation_error_code[^\{]*\{(.*?)\};', enum_header, re.S).group(1)
        entries = [part.strip() for part in enum_body.split(',') if part.strip()]
        assert entries[0] == 'ok = 0u' and all(re.fullmatch(r'\w+', item) for item in entries[1:])
        names = [item.split()[0] for item in entries]
        assert len(baseline.splitlines()) == len(names), 'whole char baseline does not cover every error code'
        header = (tree/'src/uwvm2/validation/error/error_code_output.h').read_text(encoding='utf-8-sig')
        fixture = (tree/'test/0001.parser/0003.error_code/print_all_validation_error_code.cc').read_text(encoding='utf-8-sig')
        assert names[-1] in fixture, 'update the main fixture to cover the newest error code'
        pattern = re.compile(r'(case ::uwvm2::validation::error::code_validation_error_code::(\w+):\s*\{\s*)(#include "error_code_outputs/[^\"]+")')
        cases = pattern.findall(header)
        assert sorted(case[1] for case in cases) == sorted(names)
        end_code = len(names) if a.end_code is None else a.end_code
        assert 0 <= a.first_code < end_code <= len(names)
        for first in range(a.first_code, end_code, a.chunk):
            last = min(first+a.chunk, end_code)
            directory = out/product/f'{first:02}-{last:02}'
            include = directory/'include/uwvm2/validation/error'
            include.mkdir(parents=True, exist_ok=True)
            def selected(match):
                return match[1]+f'if constexpr ({first} <= {names.index(match[2])} && {names.index(match[2])} < {last}) {{\n'+match[3]+'\n}'
            overlay = pattern.sub(selected, header)
            overlay = overlay.replace('"error.h"', '<uwvm2/validation/error/error.h>')
            overlay = overlay.replace('"error_code_outputs/', '"uwvm2/validation/error/error_code_outputs/')
            (include/'error_code_output.h').write_text(overlay)
            marker = 'errout.err.err_curr = module_bytes +'
            assert fixture.count(marker) == 1
            source = fixture.replace(marker, f'if(i < {first}u || i >= {last}u) {{ continue; }}\n        '+marker)
            (directory/'fixture.cpp').write_text(source)
            for encoding, suffix, codec in [(2, 'wc', 'utf-32-le'), (4, 'u16c', 'utf-16-le'), (5, 'u32c', 'utf-32-le')]:
                jobs.append((product, tree, directory, first, last, encoding, suffix, codec))

    def run(job):
        product, tree, directory, first, last, encoding, suffix, codec = job
        work = directory/str(encoding)
        work.mkdir(exist_ok=True)
        command = [a.cxx, '-std=c++26', '-O1', '-DUWVM=2', '-DUWVM_TEST=2',
                   '-DUWVM_DISABLE_INT', '-DUWVM_DISABLE_JIT', '-DUWVM_USE_THREAD_LOCAL',
                   '-DUWVM_TEST_ERROR_DIRECT_IO', f'-DUWVM_TEST_ERROR_CHAR={encoding}',
                   '-I'+str(directory/'include'), '-I'+str(tree/'src'),
                   '-I'+str(tree/'test/0001.parser/0003.error_code')]
        command += ['-I'+str(tree/path) for path in ['third-parties/fast_io/include', 'third-parties/bizwen/include', 'third-parties/boost_unordered/include']]
        command += [str(directory/'fixture.cpp'), '-o', str(work/'printer')]
        with (work/'compile.log').open('wb') as log:
            compiled = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=600)
        result = dict(product=product, first=first, last=last, encoding=encoding, compile=compiled.returncode)
        if compiled.returncode == 0:
            with (work/'run.log').open('wb') as log:
                proc = subprocess.run([str(work/'printer')], cwd=work, stdout=log, stderr=subprocess.STDOUT, timeout=30)
            result['run'] = proc.returncode
            if proc.returncode == 0:
                actual = (work/f'validation_error_code_test_{suffix}.log').read_bytes().decode(codec)
                expected = ''.join(baseline.splitlines(keepends=True)[first:last])
                result['matches'] = actual == expected
                (work/'decoded.txt').write_text(actual)
        (work/'result.json').write_text(json.dumps(result, indent=2))
        print(json.dumps(result), flush=True)
        return result

    with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:
        results = list(pool.map(run, jobs))
    (out/'results.json').write_text(json.dumps(results, indent=2))
    return not all(record.get('matches') for record in results)


if __name__ == '__main__':
    raise SystemExit(main())
