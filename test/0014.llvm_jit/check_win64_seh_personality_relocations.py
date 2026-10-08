#!/usr/bin/env python3
"""Check the GNU Win64 COFF personality relocation contract in a small object.

Run only inside the authorized Linux test cgroup. CLANG points to the selected
GNU x64-capable Clang driver; output stays in a caller-provided scratch folder.
This complements, and does not replace, the Win11 VM execution test.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess


def run(*argv: str) -> str:
    return subprocess.check_output(argv, text=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--clang', required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--objdump', default='objdump')
    args = parser.parse_args()
    assert run('uname', '-s').strip() == 'Linux'
    assert Path('/sys/fs/cgroup/memory.max').read_text().strip() == '68719476736'
    assert Path('/sys/fs/cgroup/memory.swap.max').read_text().strip() == '0'
    assert Path('/sys/fs/cgroup/cpuset.cpus.effective').read_text().strip() == '0,2,4,6,16-31'
    fixture = Path(__file__).with_name('fixtures') / 'win64_seh_personality_relocation.ll'
    args.out.mkdir(parents=True, exist_ok=True)
    obj = args.out / 'win64_seh_personality_relocation.o'
    subprocess.run([args.clang, '--target=x86_64-w64-windows-gnu', '-O0', '-x', 'ir',
                    '-c', str(fixture), '-o', str(obj)], check=True)
    relocations = run(args.objdump, '-r', str(obj))
    symbols = run(args.objdump, '-t', str(obj))
    sections = run(args.objdump, '-h', str(obj))
    (args.out / 'relocations.txt').write_text(relocations)
    (args.out / 'symbols.txt').write_text(symbols)
    (args.out / 'sections.txt').write_text(sections)
    assert re.search(r'RELOCATION RECORDS FOR \[\.xdata\]:[\s\S]*?IMAGE_REL_AMD64_ADDR32NB\s+__gxx_personality_seh0', relocations)
    assert re.search(r'RELOCATION RECORDS FOR \[\.xdata\]:[\s\S]*?IMAGE_REL_AMD64_ADDR64\s+uwvm_guest_exception_typeinfo_v1', relocations)
    assert re.search(r'RELOCATION RECORDS FOR \[\.text\$__gxx_personality_seh0\]:[\s\S]*?IMAGE_REL_AMD64_REL32\s+uwvm_guest_seh_host_personality_v1', relocations)
    assert re.search(r'\(sec\s+[1-9][0-9]*\).*?__gxx_personality_seh0', symbols)
    assert re.search(r'\.text\$__gxx_personality_seh0[^\n]*\n[^\n]*\bCODE\b', sections)
    assert re.search(r'\.xdata[^\n]*\n[^\n]*\bDATA\b', sections)
    print(f'PASS GNU Win64 SEH personality relocations: {obj}')


if __name__ == '__main__':
    main()
