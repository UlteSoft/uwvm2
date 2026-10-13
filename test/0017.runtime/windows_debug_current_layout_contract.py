"""Pure actual Win64 main/runtime layout and official COFF formatter contracts."""
from pathlib import Path
import re
import stage_windows_debug_current_vm as stage

require = stage.require
IDENTITY_MACROS = frozenset(('UWVM_BUILD_SOURCE_ID', 'UWVM_SOURCE_ID', 'UWVM_GIT_COMMIT_HASH', 'UWVM2_BUILD_SOURCE_ID'))


def layout_contract(argv: list[str], cwd: Path) -> tuple:
    require(type(argv) is list and bool(argv) and all(type(value) is str and '\0' not in value for value in argv), 'actual direct argv required')
    require(not any(value.startswith('@') or value.startswith('-Wl,') and '@' in value for value in argv), 'unexpanded response not a layout contract')
    macros, searches, forced, targets, sysroots, flags = {}, [], [], [], [], []
    index = 1
    while index < len(argv):
        value = argv[index]
        if value in ('-D', '-U'):
            require(index + 1 < len(argv), 'truncated actual macro')
            kind, token = value, argv[index + 1]; index += 1
        elif value.startswith(('-D', '-U')):
            kind, token = value[:2], value[2:]
        else:
            kind = token = None
        if kind:
            name = token.partition('=')[0]
            require(bool(name), 'empty actual macro')
            if name not in IDENTITY_MACROS:
                require(name not in macros, 'duplicate/conflicting actual macro')
                # Clang's bare -D gate means value 1, as emitted by top-level
                # xmake add_defines. Canonicalize only this qualification gate;
                # all other actual macro tokens and duplicate rejection remain.
                if kind == '-D' and token == 'UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT':
                    token += '=1'
                macros[name] = (kind, token)
        if value in ('-I', '-isystem', '-iquote', '-idirafter', '-include', '-imacros', '--target', '-target', '--sysroot', '-isysroot'):
            require(index + 1 < len(argv), 'truncated actual include/target/sysroot')
            token = argv[index + 1]; index += 1
            if value in ('--target', '-target'):
                targets.append(token)
            else:
                path = str(stage.resolved_argument(token, cwd))
                (forced if value in ('-include', '-imacros') else sysroots if value in ('--sysroot', '-isysroot') else searches).append((value, path))
        elif value.startswith('--target='):
            targets.append(value.partition('=')[2])
        elif value.startswith('--sysroot='):
            sysroots.append(('--sysroot', str(stage.resolved_argument(value.partition('=')[2], cwd))))
        elif value.startswith('-I') and value != '-I':
            searches.append(('-I', str(stage.resolved_argument(value[2:], cwd))))
        if value in ('-fexceptions', '-fno-exceptions', '-fcxx-exceptions', '-fno-cxx-exceptions', '-funwind-tables',
                     '-fno-unwind-tables', '-fasynchronous-unwind-tables', '-fno-asynchronous-unwind-tables',
                     '-frtti', '-fno-rtti', '-fshort-enums', '-fno-short-enums', '-fshort-wchar', '-fno-short-wchar',
                     '-mms-bitfields', '-mno-ms-bitfields', '-fms-extensions', '-fno-ms-extensions', '-m32', '-m64') or value.startswith(
                         ('-stdlib=', '-std=', '-march=', '-mcpu=', '-mabi=', '-fpack-struct', '-fclang-abi-compat=')):
            flags.append(value)
        index += 1
    require(targets == ['x86_64-w64-windows-gnu'], 'one exact actual Win64 GNU target required')
    require(macros.get('UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT') ==
            ('-D', 'UWVM2_ENABLE_DEBUG_NATIVE_STEP_WINDOWS_PRODUCT=1'),
            'actual current qualification build must explicitly enable Windows native stepping in BOTH TUs')
    require(len(sysroots) == 1, 'one actual SDK sysroot required')
    require('-fexceptions' in flags and not any(value in flags for value in ('-fno-exceptions', '-fno-cxx-exceptions')) and
            any(value in flags for value in ('-funwind-tables', '-fasynchronous-unwind-tables')) and
            not any(value in flags for value in ('-fno-unwind-tables', '-fno-asynchronous-unwind-tables')), 'actual native EH/unwind flags required')
    require('-stdlib=libc++' in flags and '-std=c++26' in flags, 'actual common C++26/libc++ ABI required')
    return tuple(sorted(macros.items())), tuple(searches), tuple(forced), tuple(targets), tuple(sysroots), tuple(flags)


def main_runtime_contracts(root: Path, build: dict) -> tuple:
    sources = {root / 'src/uwvm2/runtime/lib/uwvm_runtime.default.cpp', root / 'src/uwvm2/uwvm/main.default.cpp'}
    contracts = {}
    for row in build.get('product_compiles', []):
        output, argv, cwd = stage.verify_command(row)
        require('-c' in argv, 'actual compile record required')
        consumed = stage.file_arguments(argv, cwd)
        for source in sources & consumed:
            require(source not in contracts, 'duplicate actual main/runtime compile')
            contracts[source] = layout_contract(argv, cwd)
    require(contracts.keys() == sources and len(set(contracts.values())) == 1, 'actual fresh main/runtime layout/header/SDK/EH contracts differ')
    return next(iter(contracts.values()))


def coff_amd64_headers(text: str) -> int:
    # This formatter oracle is only SDK/object qualification, never a VM
    # source-position credential or a host/guest address reader.
    formats = re.findall(r'^Format:\s*(.+?)\s*$', text, re.MULTILINE)
    arches = re.findall(r'^Arch:\s*(.+?)\s*$', text, re.MULTILINE)
    widths = re.findall(r'^AddressSize:\s*(.+?)\s*$', text, re.MULTILINE)
    machines = re.findall(r'^\s*Machine:\s*(.+?)\s*$', text, re.MULTILINE)
    require(bool(formats) and len(formats) == len(arches) == len(widths) == len(machines) and
            all(value == 'COFF-x86-64' for value in formats) and all(value == 'x86_64' for value in arches) and
            all(value == '64bit' for value in widths) and all(value == 'IMAGE_FILE_MACHINE_AMD64 (0x8664)' for value in machines),
            'actual SDK archive contains missing/non-AMD64/non-COFF headers')
    return len(formats)
