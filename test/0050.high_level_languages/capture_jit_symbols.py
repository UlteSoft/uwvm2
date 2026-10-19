#!/usr/bin/env python3
"""Optional read-only MCJIT metadata for a caller-owned, READY-gated target.

No ptrace, signals, GDB, inferior calls, or target writes. This captures only
registered ELF objects and validated function ranges, not arbitrary heap data.
It does not collect samples or claim source-line/managed-method attribution.
"""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import sys
import tempfile

from build_products import admission, digest
from inspect_wasm import Reader, inspect


MEMBERSHIP = ('0::/system.slice/docker-'
              '4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope')
MAX_ENTRIES = 16384
MAX_OBJECT = 128 << 20
MAX_TOTAL = 512 << 20
MAX_TABLE = 256 << 20
U64_LIMIT = 1 << 64
WASM_SYMBOL = re.compile(r'^(uwvm_m_(?:addr_)?[0-9a-fA-Fx]+)_(raw_)?func_([0-9]+)([._].*)?$')


class SafetyError(RuntimeError):
    """Identity/admission failures must prevent the caller from releasing G."""


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def require_safe(condition, message):
    if not condition:
        raise SafetyError(message)


def safety_call(function, *args, **kwargs):
    try:
        return function(*args, **kwargs)
    except Exception as error:
        raise SafetyError(f'{function.__name__} safety verification failed: {error}') from error


def sha_bytes(data):
    return hashlib.sha256(data).hexdigest()


def fd_hash(fd):
    before = os.fstat(fd)
    checksum, offset = hashlib.sha256(), 0
    while offset < before.st_size:
        block = os.pread(fd, min(1 << 20, before.st_size - offset), offset)
        require(block, 'short mapped ELF file read while hashing')
        checksum.update(block)
        offset += len(block)
    after = os.fstat(fd)
    require((before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns)
            == (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns),
            'opened mapped ELF file changed while hashing')
    return checksum.hexdigest()


def durable_json(path, data):
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix=path.name + '.', delete=False) as stream:
            temporary = Path(stream.name)
            stream.write((json.dumps(data, indent=2, allow_nan=False) + '\n').encode())
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        fd = os.open(path.parent, os.O_DIRECTORY | os.O_RDONLY)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


class ELF:
    """Bounded ELF64-LE reader, backed by an opened file or registered bytes."""
    def __init__(self, *, fd=None, data=None):
        require((fd is None) != (data is None), 'one ELF backing is required')
        self.fd, self.data = fd, data
        self.size = os.fstat(fd).st_size if fd is not None else len(data)
        header = struct.unpack('<16sHHIQQQIHHHHHH', self.read(0, 64))
        ident = header[0]
        require(ident[:7] == b'\x7fELF\x02\x01\x01', 'only ELF64 little endian version1 is supported')
        self.kind, self.machine = header[1:3]
        require(header[3] == 1 and self.machine == 62, 'only x86-64 ELF version1 is supported')
        phoff, shoff = header[5:7]
        ehsize, phentsize, phnum, shentsize, shnum, _ = header[8:14]
        require(ehsize == 64, 'unexpected ELF header size')
        section0 = None
        if shoff:
            require(shentsize == 64, 'unexpected section-header size')
            section0 = self.section(struct.unpack('<IIQQQQIIQQ', self.read(shoff, 64)))
            if shnum == 0:
                shnum = section0['size']
            if phnum == 65535:
                phnum = section0['info']
        require(0 <= shnum <= 65536 and 0 <= phnum <= 65536, 'ELF header count exceeds bound')
        require(not shnum or shoff, 'section table missing')
        require(not phnum or (phoff and phentsize == 56), 'program table missing/unsupported')
        self.sections = [self.section(struct.unpack('<IIQQQQIIQQ', self.read(shoff + i * 64, 64)))
                         for i in range(shnum)]
        self.loads = []
        for i in range(phnum):
            typ, flags, offset, va, _, filesz, memsz, align = struct.unpack('<IIQQQQQQ', self.read(phoff + i * 56, 56))
            if typ == 1:
                require(filesz <= memsz and offset + filesz <= self.size and va + memsz < U64_LIMIT,
                        'invalid PT_LOAD extent')
                self.loads.append({'flags': flags, 'offset': offset, 'vaddr': va,
                                   'filesz': filesz, 'memsz': memsz, 'align': align})

    @staticmethod
    def section(cells):
        return dict(zip(['name', 'type', 'flags', 'addr', 'offset', 'size', 'link', 'info', 'align', 'entsize'], cells))

    def read(self, offset, size):
        require(0 <= offset <= self.size and 0 <= size <= MAX_TABLE and offset + size <= self.size,
                'ELF read outside bounded file')
        data = os.pread(self.fd, size, offset) if self.fd is not None else self.data[offset:offset + size]
        require(len(data) == size, 'short ELF read')
        return data

    def symbols(self):
        for table in self.sections:
            if table['type'] not in (2, 11):
                continue
            require(table['entsize'] == 24 and table['size'] % 24 == 0 and table['size'] <= MAX_TABLE,
                    'invalid or oversized ELF symbol table')
            require(0 <= table['link'] < len(self.sections), 'symbol string table missing')
            strings = self.sections[table['link']]
            require(strings['type'] == 3, 'symbol string table type mismatch')
            text = self.read(strings['offset'], strings['size'])
            raw = self.read(table['offset'], table['size'])
            for name, info, other, section, value, size in struct.iter_unpack('<IBBHQQ', raw):
                require(name < len(text), 'symbol name outside string table')
                end = text.find(b'\0', name)
                require(end >= 0, 'unterminated symbol name')
                yield {'name': text[name:end].decode('utf-8', errors='replace'), 'type': info & 15,
                       'binding': info >> 4, 'section': section, 'value': value, 'size': size}


def read_maps(pid):
    text = Path(f'/proc/{pid}/maps').read_text()
    rows = []
    for line in text.splitlines():
        cells = line.split(None, 5)
        require(len(cells) >= 5, 'malformed process maps')
        low, high = [int(v, 16) for v in cells[0].split('-')]
        major, minor = [int(v, 16) for v in cells[3].split(':')]
        name = cells[5] if len(cells) == 6 else ''
        name = re.sub(r'\\([0-7]{3})', lambda m: chr(int(m[1], 8)), name)
        require(0 <= low < high < U64_LIMIT and (not rows or low >= rows[-1]['end']), 'invalid map range/order')
        rows.append({'start': low, 'end': high, 'perms': cells[1], 'offset': int(cells[2], 16),
                     'major': major, 'minor': minor, 'inode': int(cells[4]), 'path': name})
    return text, rows


def mapped_range(maps, address, size, permission):
    if size <= 0 or not (0 <= address < address + size < U64_LIMIT):
        return False
    cursor, end = address, address + size
    for row in maps:
        if row['end'] <= cursor:
            continue
        if row['start'] > cursor or permission not in row['perms']:
            return False
        cursor = min(end, row['end'])
        if cursor == end:
            return True
    return False


def memory_read(fd, maps, address, size):
    require(0 < size <= MAX_OBJECT and mapped_range(maps, address, size, 'r'), 'target read outside readable mapped range')
    data = os.pread(fd, size, address)
    require(len(data) == size, 'short target memory read')
    return data


def target_identity(pid):
    root = Path('/proc') / str(pid)
    membership = (root / 'cgroup').read_text().strip()
    require(membership == MEMBERSHIP, 'target escaped the admitted cgroup')
    stat_text = (root / 'stat').read_text()
    fields = stat_text.rsplit(')', 1)[1].split()
    tasks = {}
    for path in sorted((root / 'task').iterdir()):
        try:
            status = (path / 'status').read_text()
            caps = {key: int(value, 16) for key, value in
                    re.findall(r'^(Cap(?:Inh|Prm|Eff|Bnd|Amb)):\s*([0-9a-f]+)$', status, re.M)}
            tracer = re.search(r'^TracerPid:\s*([0-9]+)$', status, re.M)
            require(len(caps) == 5 and not any(caps.values()), 'target capabilities retained')
            require(tracer and tracer[1] == '0', 'target has a tracer')
            require(re.search(r'^NoNewPrivs:\s*1$', status, re.M), 'target no-new-privileges missing')
            require((path / 'cgroup').read_text().strip() == MEMBERSHIP, 'target thread cgroup mismatch')
            tasks[path.name] = {'capabilities': caps, 'tracer_pid': 0,
                                'affinity': sorted(os.sched_getaffinity(int(path.name)))}
        except (FileNotFoundError, ProcessLookupError):
            raise RuntimeError('target task set changed during identity capture')
    require(str(pid) in tasks, 'main target thread unavailable')
    argv = [os.fsdecode(v) for v in (root / 'cmdline').read_bytes().split(b'\0') if v]
    return {'pid': pid, 'parent_pid': int(fields[1]), 'start_time_ticks': int(fields[19]),
            'cgroup': membership, 'exe': os.readlink(root / 'exe'), 'argv': argv, 'tasks': tasks}


def critical_snapshot(pid, binary, wasm, binary_sha256, wasm_sha256):
    """Independent pre/post safety check, including on a metadata-only failure."""
    guard = safety_call(admission)
    require_safe(guard['membership'] == MEMBERSHIP, 'original 64-GiB cgroup required')
    identity = safety_call(target_identity, pid)
    actual, supplied = safety_call(os.stat, f'/proc/{pid}/exe'), safety_call(binary.stat)
    require_safe((actual.st_dev, actual.st_ino) == (supplied.st_dev, supplied.st_ino),
                 'actual executable is not the supplied product inode')
    require_safe(safety_call(digest, f'/proc/{pid}/exe') == binary_sha256
                 and safety_call(digest, binary) == binary_sha256, 'actual product image hash mismatch')
    require_safe(safety_call(digest, wasm) == wasm_sha256, 'actual Wasm hash mismatch')
    argv = identity['argv']
    require_safe(argv.count('--run') == 1 and argv.index('--run') + 1 < len(argv), 'one explicit --run target required')
    run_path = Path(argv[argv.index('--run') + 1])
    if not run_path.is_absolute():
        run_path = Path(safety_call(os.readlink, f'/proc/{pid}/cwd')) / run_path
    require_safe(safety_call(run_path.resolve, strict=True) == wasm, 'target --run file differs from supplied Wasm')
    return {'admission': guard, 'target': identity,
            'boot': Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
            'binary_sha256': binary_sha256, 'wasm_sha256': wasm_sha256}


def file_maps(maps, metadata):
    return [row for row in maps if row['inode'] == metadata.st_ino
            and (row['major'], row['minor']) == (os.major(metadata.st_dev), os.minor(metadata.st_dev))]


def load_bias(elf, rows):
    require(elf.kind in (2, 3) and rows and elf.loads, 'mapped executable/shared ELF required')
    page = os.sysconf('SC_PAGE_SIZE')
    down = lambda n: n // page * page
    up = lambda n: (n + page - 1) // page * page
    def corresponds(row, segment, bias):
        base_off, base_va = down(segment['offset']), down(segment['vaddr'])
        return (base_off <= row['offset'] < up(segment['offset'] + segment['filesz'])
                and row['start'] == bias + base_va + row['offset'] - base_off
                and row['end'] <= bias + up(segment['vaddr'] + segment['memsz'])
                and all(letter not in row['perms'] or segment['flags'] & bit
                        for letter, bit in [('r', 4), ('w', 2), ('x', 1)]))
    candidates = {row['start'] - down(segment['vaddr']) - (row['offset'] - down(segment['offset']))
                  for row in rows for segment in elf.loads
                  if segment['filesz'] and down(segment['offset']) <= row['offset'] < up(segment['offset'] + segment['filesz'])}
    valid = [bias for bias in candidates if bias >= 0 and bias % page == 0
             and (elf.kind != 2 or bias == 0)
             and all(any(corresponds(row, segment, bias) for segment in elf.loads) for row in rows)
             and any('x' in row['perms'] and corresponds(row, segment, bias)
                     for row in rows for segment in elf.loads if segment['flags'] & 1)]
    require(len(valid) == 1, 'PT_LOAD/maps load bias is missing or ambiguous')
    return valid[0]


def registry(fd, maps, address):
    raw = memory_read(fd, maps, address, 24)
    version, action, relevant, first = struct.unpack('<IIQQ', raw)
    require(version == 1 and action in (0, 1, 2), 'unsupported JIT descriptor ABI')
    entries, seen, previous, current = [], set(), 0, first
    while current:
        require(current not in seen and len(entries) < MAX_ENTRIES, 'JIT entry cycle/count bound exceeded')
        seen.add(current)
        next_entry, prev_entry, elf_address, size = struct.unpack('<QQQQ', memory_read(fd, maps, current, 32))
        require(prev_entry == previous, 'JIT linked-list prev pointer inconsistent')
        require(0 < size <= MAX_OBJECT, 'registered ELF size exceeds bound')
        require(mapped_range(maps, elf_address, size, 'r'), 'registered ELF outside readable mappings')
        entries.append({'entry': current, 'next': next_entry, 'prev': prev_entry,
                        'elf_address': elf_address, 'elf_size': size})
        previous, current = current, next_entry
    return {'address': address, 'version': version, 'action': action, 'relevant_entry': relevant,
            'first_entry': first, 'entries': entries}


def module_binding(wasm, identity):
    argv = identity['argv']
    require_safe(argv.count('--run') == 1 and argv.index('--run') + 1 < len(argv), 'one explicit --run target required')
    run_name = argv[argv.index('--run') + 1]
    run_path = Path(run_name)
    if not run_path.is_absolute():
        run_path = Path(os.readlink(f"/proc/{identity['pid']}/cwd")) / run_path
    require_safe(safety_call(run_path.resolve, strict=True) == wasm, 'target --run module differs from --wasm')
    require(wasm.stat().st_size <= MAX_OBJECT, 'Wasm exceeds metadata reader bound')
    raw = wasm.read_bytes()
    reader, custom_names = Reader(raw), []
    reader.pos = 8
    while reader.pos < len(raw):
        tag, size = reader.byte(), reader.leb()
        end = reader.pos + size
        require(end <= len(raw), 'Wasm section outside file')
        if tag == 0:
            section = Reader(raw[reader.pos:end])
            if section.name() == 'name':
                while section.pos < len(section.data):
                    subtag, subsize = section.byte(), section.leb()
                    subend = section.pos + subsize
                    require(subend <= len(section.data), 'Wasm name subsection outside file')
                    if subtag == 0:
                        custom_names.append(section.name())
                    section.pos = subend
        reader.pos = end
    require(len(custom_names) <= 1, 'ambiguous Wasm module-name custom subsections')
    name = custom_names[0] if custom_names and custom_names[0] else run_name
    # Use the installed standard XXH3 API only in this inspector. Never call
    # any function inside the target or load a library into the target.
    libraries = [Path('/usr/lib/x86_64-linux-gnu/libxxhash.so.0'), Path('/lib/x86_64-linux-gnu/libxxhash.so.0')]
    library_path = next((p.resolve() for p in libraries if p.is_file()), None)
    require(library_path is not None, 'installed libxxhash.so.0 unavailable; module-prefix binding unsupported')
    library = ctypes.CDLL(str(library_path))
    function = library.XXH3_64bits
    function.argtypes, function.restype = [ctypes.c_void_p, ctypes.c_size_t], ctypes.c_uint64
    encoded = name.encode('utf-8')
    buffer = ctypes.create_string_buffer(encoded)
    value = function(buffer, len(encoded))
    return {'module_name': name, 'module_name_source': 'name custom section' if custom_names and custom_names[0] else 'exact --run argv',
            'native_prefix': f'uwvm_m_{value:016x}', 'hash_algorithm': 'XXH3_64bits default seed0',
            'inspector_xxhash_library': str(library_path), 'inspector_xxhash_library_sha256': digest(library_path)}


def capture(pid, binary, wasm, out, expected_binary_sha256=None, expected_wasm_sha256=None):
    """Return a receipt; a metadata failure is explicit and does not sample.

    Caller must own this target and have observed READY without sending G.
    CLI expected hashes are optional; callers should pass their manifest/product
    identities and independently match this receipt before using attribution.
    """
    out = Path(out)
    out.mkdir(parents=True, exist_ok=False)
    receipt_path = out / 'receipt.json'
    data = {'passed': False, 'purpose': 'optional-readonly-jit-symbol-metadata', 'pid': pid,
            'caller_pid': os.getpid(), 'caller_parent_pid': os.getppid(),
            'gate_precondition': 'caller-owned ordinary target held at READY; caller must not release G during capture',
            'gate_not_observed_by_this_helper': True, 'target_writes': False, 'ptrace': False, 'gdb': False,
            'source_line_or_managed_method_attribution': False, 'sample_ip_join_status': 'not attempted',
            'safety_checks_passed': False, 'fatal_safety_failure': False,
            'limits': {'entries_per_descriptor': MAX_ENTRIES, 'registered_elf_bytes_each': MAX_OBJECT,
                       'registered_elf_bytes_total': MAX_TOTAL}, 'objects': [], 'ranges': [], 'skipped_symbols': []}
    fds, interrupt = [], None
    try:
        durable_json(receipt_path, data)
        before = safety_call(admission)
        require_safe(before['membership'] == MEMBERSHIP, 'only original existing 64-GiB container is admitted')
        data['admission_before'] = before
        data['boot'] = Path('/proc/sys/kernel/random/boot_id').read_text().strip()
        require_safe(isinstance(pid, int) and pid > 1 and pid != os.getpid(), 'invalid target PID')
        binary, wasm = safety_call(Path(binary).resolve, strict=True), safety_call(Path(wasm).resolve, strict=True)
        data.update(binary=str(binary), wasm=str(wasm), binary_sha256=safety_call(digest, binary), wasm_sha256=safety_call(digest, wasm),
                    expected_binary_sha256=expected_binary_sha256, expected_wasm_sha256=expected_wasm_sha256,
                    helper_sha256=digest(__file__), inspect_wasm_sha256=digest(Path(__file__).with_name('inspect_wasm.py')))
        for actual, expected in [(data['binary_sha256'], expected_binary_sha256), (data['wasm_sha256'], expected_wasm_sha256)]:
            require_safe(expected is None or re.fullmatch('[0-9a-f]{64}', expected) and actual == expected,
                         'caller-provided identity mismatch')
        data['safety_before'] = critical_snapshot(pid, binary, wasm, data['binary_sha256'], data['wasm_sha256'])
        identity = data['safety_before']['target']
        data['target_before'] = identity
        require(digest(f'/proc/{pid}/exe') == data['binary_sha256'], 'actual /proc/PID/exe differs from product')
        actual_exe = os.stat(f'/proc/{pid}/exe')
        require((actual_exe.st_dev, actual_exe.st_ino) == (binary.stat().st_dev, binary.stat().st_ino),
                'actual executable is not the supplied product inode')
        maps_text, maps = read_maps(pid)
        data['maps_before'] = maps_text
        data['maps_before_sha256'] = sha_bytes(maps_text.encode())
        names = inspect(wasm)
        data['wasm_functions'] = names['functions']
        binding = module_binding(wasm, identity)
        data['wasm_module_binding'] = binding
        mem = os.open(f'/proc/{pid}/mem', os.O_RDONLY)
        fds.append(mem)
        files = [binary]
        for row in maps:
            name = Path(row['path']).name
            if row['path'].startswith('/') and name.startswith('libLLVM') and '.so' in name:
                require(not row['path'].endswith(' (deleted)'), 'mapped LLVM library was deleted')
                files.append(Path(f'/proc/{pid}/root') / row['path'].lstrip('/'))
        descriptors, mapped_files, file_seen = {}, [], set()
        for file in files:
            fd = os.open(file, os.O_RDONLY)
            fds.append(fd)
            metadata = os.fstat(fd)
            key = metadata.st_dev, metadata.st_ino
            if key in file_seen:
                continue
            file_seen.add(key)
            rows = file_maps(maps, metadata)
            require(rows, 'product/LLVM file identity has no matching target mappings')
            elf = ELF(fd=fd)
            bias = load_bias(elf, rows)
            found = [symbol for symbol in elf.symbols()
                     if symbol['name'] == '__jit_debug_descriptor' and symbol['section'] != 0]
            for symbol in found:
                require(symbol['type'] in (0, 1) and symbol['section'] < len(elf.sections),
                        'descriptor symbol has unsupported type/section')
                section = elf.sections[symbol['section']]
                require(section['flags'] & 2 and section['addr'] <= symbol['value']
                        and symbol['value'] + 24 <= section['addr'] + section['size'],
                        'descriptor symbol outside its allocated ELF section')
            values = {symbol['value'] for symbol in found}
            require(len(values) <= 1, 'ambiguous __jit_debug_descriptor symbol definitions')
            record = {'path': str(file), 'device': metadata.st_dev, 'inode': metadata.st_ino,
                      'sha256': fd_hash(fd), 'load_bias': bias, 'elf_type': elf.kind, 'maps': rows,
                      'descriptor_addresses': []}
            require((file.stat().st_dev, file.stat().st_ino) == key and digest(file) == record['sha256'],
                    'mapped ELF path no longer identifies its opened mapping file')
            for value in values:
                address = bias + value
                require(mapped_range(maps, address, 24, 'r'), 'descriptor outside readable target mapping')
                record['descriptor_addresses'].append(address)
                descriptors[address] = registry(mem, maps, address)
            mapped_files.append(record)
        require(descriptors, 'no mapped product/libLLVM JIT descriptor symbol')
        data['mapped_elf_files'] = mapped_files
        data['descriptors_before'] = list(descriptors.values())
        entries = {}
        for descriptor in descriptors.values():
            for entry in descriptor['entries']:
                entries[(entry['elf_address'], entry['elf_size'])] = entry
        require(entries and len(entries) <= MAX_ENTRIES, 'no registered ELF objects or count exceeds bound')
        require(sum(size for address, size in entries) <= MAX_TOTAL, 'registered ELF aggregate exceeds bound')
        seen_ranges = set()
        for index, ((address, size), entry) in enumerate(sorted(entries.items())):
            raw = memory_read(mem, maps, address, size)
            checksum = sha_bytes(raw)
            file = out / f'registered-{index:05d}.elf'
            require(shutil.disk_usage(out).free >= len(raw) + (512 << 20),
                    'insufficient free space to save registered ELF with 512-MiB reserve')
            with file.open('xb') as stream:
                stream.write(raw)
                stream.flush()
                os.fsync(stream.fileno())
            obj = {'file': str(file), 'sha256': checksum, 'bytes': size, **entry}
            data['objects'].append(obj)
            elf = ELF(data=raw)
            require(elf.kind == 1, 'registered object is not ET_REL; address rule unsupported')
            for symbol in elf.symbols():
                if symbol['type'] != 2 or symbol['section'] == 0:
                    continue
                section_index = symbol['section']
                if symbol['size'] == 0 or section_index >= len(elf.sections):
                    data['skipped_symbols'].append({'object': index, 'symbol': symbol['name'], 'reason': 'zero size or unsupported section index'})
                    continue
                section = elf.sections[section_index]
                if not section['addr'] or not section['flags'] & 4:
                    data['skipped_symbols'].append({'object': index, 'symbol': symbol['name'], 'reason': 'unloaded or non-executable section'})
                    continue
                require(symbol['value'] + symbol['size'] <= section['size'], 'function exceeds its ET_REL section')
                start = section['addr'] + symbol['value']
                require(mapped_range(maps, start, symbol['size'], 'x'), 'function outside actual executable target mappings')
                key = start, symbol['size'], symbol['name']
                if key in seen_ranges:
                    continue
                seen_ranges.add(key)
                row = {'start': start, 'size': symbol['size'], 'end': start + symbol['size'],
                       'native_symbol': symbol['name'], 'registered_object': index,
                       'address_rule': 'ET_REL section.sh_addr + symbol.st_value', 'wasm_function_index': None,
                       'wasm_name': None, 'wasm_module_bound': False}
                match = WASM_SYMBOL.fullmatch(symbol['name'])
                if match:
                    function_index = int(match[3])
                    row.update(wasm_function_index=function_index, native_module_prefix=match[1],
                               native_abi='raw wrapper' if match[2] else 'typed entry', native_suffix=match[4] or '')
                    if match[1] == binding['native_prefix']:
                        require(function_index < sum(item['kind'] == 0 for item in names['imports']) + names['code_bodies'],
                                'bound native function index outside supplied Wasm index space')
                        row.update(wasm_module_bound=True, wasm_name=names['functions'].get(function_index),
                                   wasm_sha256=data['wasm_sha256'],
                                   wasm_exports=[item['name'] for item in names['exports']
                                                 if item['kind'] == 0 and item['index'] == function_index])
                data['ranges'].append(row)
        require(data['ranges'], 'no nonempty executable function ranges')
        require(any(row['wasm_module_bound'] for row in data['ranges']), 'no symbol matches the exact supplied Wasm module prefix')
        data['ranges'].sort(key=lambda row: (row['start'], row['size'], row['native_symbol']))
        maps_after, _ = read_maps(pid)
        require(maps_after == maps_text, 'target mappings changed while READY should be blocked')
        after_registry = {address: registry(mem, maps, address) for address in descriptors}
        require(after_registry == descriptors, 'JIT descriptor/list changed during capture')
        for obj in data['objects']:
            require(sha_bytes(memory_read(mem, maps, obj['elf_address'], obj['elf_size'])) == obj['sha256'],
                    'registered ELF bytes changed during capture')
        identity_after = safety_call(target_identity, pid)
        require_safe(identity_after == identity, 'target PID/start time/tasks/capabilities changed during capture')
        require_safe(safety_call(digest, f'/proc/{pid}/exe') == safety_call(digest, binary) == data['binary_sha256'], 'product image changed during capture')
        require_safe(safety_call(digest, wasm) == data['wasm_sha256'], 'Wasm changed during capture')
        require_safe(all(safety_call(digest, item['path']) == item['sha256'] for item in mapped_files), 'mapped LLVM/product file changed during capture')
        require(digest(binding['inspector_xxhash_library']) == binding['inspector_xxhash_library_sha256'],
                'inspector xxhash library changed during capture')
        require(digest(__file__) == data['helper_sha256']
                and digest(Path(__file__).with_name('inspect_wasm.py')) == data['inspect_wasm_sha256'],
                'inspector helpers changed during capture')
        after = safety_call(admission)
        require_safe(after == before and Path('/proc/sys/kernel/random/boot_id').read_text().strip() == data['boot'],
                     'admission/boot changed during capture')
        require(read_maps(pid)[0] == maps_text
                and {address: registry(mem, maps, address) for address in descriptors} == descriptors,
                'target maps/registry changed before snapshot publication')
        data.update(passed=True, quiescent_snapshot_verified=True, target_after=identity_after,
                    admission_after=after, maps_after_sha256=sha_bytes(maps_after.encode()),
                    descriptors_after=list(after_registry.values()),
                    attribution_scope='native function range to original Wasm function index/name only; no source-line or managed-bytecode-method claim')
    except BaseException as error:
        data.update(passed=False, error=repr(error), attribution_scope='unavailable; retain original raw hardware qualification separately')
        if isinstance(error, SafetyError):
            data['fatal_safety_failure'] = True
        if not isinstance(error, Exception):
            interrupt = error
    finally:
        if 'safety_before' in data:
            try:
                data['safety_after'] = critical_snapshot(pid, binary, wasm, data['binary_sha256'], data['wasm_sha256'])
                require_safe(data['safety_after'] == data['safety_before'], 'critical target/admission identity changed')
                data['safety_checks_passed'] = True
            except Exception as error:
                data.update(passed=False, fatal_safety_failure=True, safety_error=repr(error))
        else:
            data['fatal_safety_failure'] = True
        data['failure_class'] = ('safety' if data['fatal_safety_failure'] else 'none' if data['passed'] else 'metadata')
        for fd in reversed(fds):
            os.close(fd)
        durable_json(receipt_path, data)
    if interrupt is not None:
        raise interrupt
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid', type=int, required=True)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--wasm', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--expected-binary-sha256')
    parser.add_argument('--expected-wasm-sha256')
    args = parser.parse_args()
    data = capture(args.pid, args.binary, args.wasm, args.out,
                   args.expected_binary_sha256, args.expected_wasm_sha256)
    print(json.dumps({'passed': data['passed'], 'receipt': str(args.out / 'receipt.json'),
                      'objects': len(data['objects']), 'ranges': len(data['ranges']), 'error': data.get('error')}), flush=True)
    raise SystemExit(0 if data['passed'] else 1)


if __name__ == '__main__':
    main()
