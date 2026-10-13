"""Pure direct-argv and official Mach-O formatter checks for the fixed Mac lane."""
from pathlib import Path
import re
import stage_windows_debug_current_vm as build_helpers

require = build_helpers.require
sha = build_helpers.sha


IDENTITY_MACROS = frozenset(('UWVM_BUILD_SOURCE_ID', 'UWVM_SOURCE_ID', 'UWVM_GIT_COMMIT_HASH', 'UWVM2_BUILD_SOURCE_ID'))


def layout_contract(argv: list[str], cwd: Path) -> tuple:
    require(type(argv) is list and bool(argv) and all(type(value) is str and '\0' not in value for value in argv), 'actual direct argv required')
    require(not any(value.startswith('@') or value.startswith('-Wl,') and '@' in value for value in argv), 'unexpanded response not a layout contract')
    require(not any(value in ('-Xclang', '-Xpreprocessor') or value.startswith('-fmodule-file') for value in argv),
            'opaque frontend/preprocessor/module arguments need separate actual layout qualification')
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
                macros[name] = (kind, token)
        if value in ('-I', '-isystem', '-iquote', '-idirafter', '-iframework', '-F', '-include', '-include-pch', '-imacros', '--target', '-target', '--sysroot', '-isysroot'):
            require(index + 1 < len(argv), 'truncated actual include/target/sysroot')
            token = argv[index + 1]; index += 1
            if value in ('--target', '-target'):
                targets.append(token)
            else:
                path = str(build_helpers.resolved_argument(token, cwd))
                (forced if value in ('-include', '-include-pch', '-imacros') else sysroots if value in ('--sysroot', '-isysroot') else searches).append((value, path))
        elif value.startswith('--target='):
            targets.append(value.partition('=')[2])
        elif value.startswith('--sysroot='):
            sysroots.append(('--sysroot', str(build_helpers.resolved_argument(value.partition('=')[2], cwd))))
        else:
            # Resolve joined searches against EACH original TU's actual cwd.
            for option in ('-iframework', '-idirafter', '-isystem', '-iquote', '-I', '-F'):
                if value.startswith(option) and value != option:
                    searches.append((option, str(build_helpers.resolved_argument(value[len(option):], cwd))))
                    break
        if value in ('-fexceptions', '-fno-exceptions', '-fcxx-exceptions', '-fno-cxx-exceptions', '-funwind-tables',
                     '-fno-unwind-tables', '-fasynchronous-unwind-tables', '-fno-asynchronous-unwind-tables',
                     '-frtti', '-fno-rtti', '-fshort-enums', '-fno-short-enums', '-fshort-wchar', '-fno-short-wchar',
                     '-mms-bitfields', '-mno-ms-bitfields', '-fms-extensions', '-fno-ms-extensions', '-fsigned-char', '-funsigned-char', '-m32', '-m64') or value.startswith(
                         ('-stdlib=', '-std=', '-march=', '-mcpu=', '-mabi=', '-fpack-struct', '-fclang-abi-compat=')):
            flags.append(value)
        index += 1
    require(len(targets) == 1 and re.fullmatch(r'(?:arm64|aarch64)-apple-(?:macosx?|darwin)(?:[0-9]+(?:\.[0-9]+){0,2})?', targets[0]) is not None,
            'one exact actual arm64 Apple macOS/Darwin target required')
    require(len(sysroots) == 1, 'one actual selected Mac SDK sysroot required')
    require('-fexceptions' in flags and not any(value in flags for value in ('-fno-exceptions', '-fno-cxx-exceptions')) and
            any(value in flags for value in ('-funwind-tables', '-fasynchronous-unwind-tables')) and
            not any(value in flags for value in ('-fno-unwind-tables', '-fno-asynchronous-unwind-tables')), 'actual native EH/unwind flags required')
    require('-stdlib=libc++' in flags and '-std=c++26' in flags, 'actual common C++26/libc++ ABI required')
    return tuple(sorted(macros.items())), tuple(searches), tuple(forced), tuple(targets), tuple(sysroots), tuple(flags)


def option_values(argv: list[str], names: tuple[str, ...]) -> list[str]:
    result = []
    for index, value in enumerate(argv):
        if value in names:
            require(index + 1 < len(argv), 'truncated actual search/sysroot')
            result.append(argv[index + 1])
        else:
            for name in names:
                if value.startswith(name + '='):
                    result.append(value[len(name) + 1:])
                elif name == '-F' and value.startswith('-F') and value != '-F':
                    result.append(value[2:])
    return result


def oracle(row: dict, product: Path, option: str) -> tuple[str, set[Path]]:
    argv = row.get('argv')
    require(row.get('returncode') == 0 and type(argv) is list and len(argv) == 4 and
            argv[1:3] == ['--macho', option] and all(type(value) is str and '\0' not in value for value in argv), 'actual direct Mach-O oracle argv')
    cwd = Path(row['cwd']).resolve(strict=True)
    tool = build_helpers.resolved_argument(argv[0], cwd)
    log = Path(row['log']).resolve(strict=True)
    require(re.fullmatch(r'llvm-objdump(?:-[0-9]+(?:\.[0-9]+)*)?', tool.name) is not None and
            build_helpers.resolved_argument(argv[3], cwd) == product and sha(product) == row.get('input_sha256'), 'actual official oracle input/tool')
    require(log.stat().st_size <= 8 << 20 and sha(tool) == row.get('tool_sha256') and sha(log) == row.get('log_sha256'), 'actual original oracle tool/log SHA')
    return log.read_text(), {tool, log}


def system_loads(text: str, product: Path, *, rpaths: bool = False) -> list[str]:
    rows = [row.strip() for row in text.splitlines() if row.strip()]
    if rows and rows[0] in (str(product) + ':', product.name + ':'):
        rows.pop(0)
    if rpaths:
        require(not rows, 'minimal current closure admits no LC_RPATH')
        return []
    paths = []
    for row in rows:
        match = re.fullmatch(r'([^ ]+) \(compatibility version [0-9]+\.[0-9]+\.[0-9]+, current version [0-9]+\.[0-9]+\.[0-9]+(?:, (?:weak|reexport|upward|lazy))*\)', row)
        require(match is not None, 'unrecognized actual LLVM Mach-O dylib row')
        path = match[1]
        require((path.startswith('/usr/lib/') and path.endswith('.dylib') or path.startswith('/System/Library/Frameworks/')) and
                '..' not in Path(path).parts and '@' not in path, 'non-system load needs separate current arm64 closure; unavailable in minimal lane')
        paths.append(path)
    require(bool(paths) and len(paths) == len(set(paths)) and '/usr/lib/libSystem.B.dylib' in paths and
            any('/Security.framework/' in path for path in paths) and any('/CoreFoundation.framework/' in path for path in paths),
            'actual system/Security/CoreFoundation load closure missing')
    return paths


def system_paths(paths: list[str]) -> list[str]:
    require(type(paths) is list and bool(paths) and all(type(path) is str for path in paths),
            'actual parsed system dependency paths required')
    require(all((path.startswith('/usr/lib/') and path.endswith('.dylib') or path.startswith('/System/Library/Frameworks/')) and
                '..' not in Path(path).parts and '@' not in path and '\0' not in path for path in paths),
            'unqualified external or ambiguous actual dependency')
    require(len(paths) == len(set(paths)) and '/usr/lib/libSystem.B.dylib' in paths and
            any('/Security.framework/' in path for path in paths) and any('/CoreFoundation.framework/' in path for path in paths),
            'actual system/Security/CoreFoundation dependencies missing')
    return paths


def qualify(build: dict, root: Path, product: Path, argv: list[str], cwd: Path, sdk: Path, frameworks: set[Path]) -> tuple[dict, set[Path]]:
    sources = (root / 'src/uwvm2/runtime/lib/uwvm_runtime.default.cpp', root / 'src/uwvm2/uwvm/main.default.cpp')
    contracts = {}
    for row in build.get('product_compiles', []):
        output, flags, directory = build_helpers.verify_command(row)
        consumed = build_helpers.file_arguments(flags, directory)
        for source in sources:
            if source in consumed:
                require(source not in contracts, 'ambiguous duplicate current C++ TU')
                contracts[source] = layout_contract(flags, directory)
    require(len(contracts) == 2 and len(set(contracts.values())) == 1, 'actual main/runtime macro/header/SDK/target/EH/ABI contracts differ')
    for value in contracts.values():
        require(Path(value[4][0][1]) == sdk, 'actual compile SDK differs from the selected MIG/link SDK')
        require(all(Path(path).is_relative_to(sdk) for kind, path in value[1] if kind in ('-F', '-iframework')),
                'actual compile framework search escapes selected SDK')
    roots = option_values(argv, ('-isysroot', '--sysroot', '-syslibroot'))
    require(len(roots) == 1 and build_helpers.resolved_argument(roots[0], cwd) == sdk, 'actual selected link SDK sysroot differs')
    searches = {sdk / 'System/Library/Frameworks'}
    for value in option_values(argv, ('-F',)):
        directory = build_helpers.resolved_argument(value, cwd)
        require(directory.is_relative_to(sdk), 'external framework search refused')
        searches.add(directory)
    trace = Path(build['product_link']['log']).read_text()
    require('-Wl,-t' in argv or '-t' in argv, 'actual original framework link trace required')
    require(all(path.parent.parent in searches and str(path) in trace.splitlines() for path in frameworks),
            'actual original trace lacks selected SDK framework stubs')
    linker = build.get('actual_linker', {})
    linker_path = Path(linker.get('path', '')).resolve(strict=True)
    require(linker_path.is_file() and re.fullmatch(r'ld64\.lld(?:-[0-9]+(?:\.[0-9]+)*)?', linker_path.name) is not None and
            sha(linker_path) == linker.get('sha256') and '-fuse-ld=' + str(linker_path) in argv,
            'actual original selected Mach-O linker tool SHA')
    loads, inputs = oracle(build['macho_dylibs'], product, '--dylibs-used')
    rpaths, more_inputs = oracle(build['macho_rpaths'], product, '--rpaths')
    system_loads(rpaths, product, rpaths=True)
    return {'actual_main_runtime_layout_contract': contracts[sources[0]], 'actual_sdk_sysroot': str(sdk),
            'actual_linker': linker, 'actual_system_loads': system_loads(loads, product), 'rpaths': [],
            'macho_dylibs': build['macho_dylibs'], 'macho_rpaths': build['macho_rpaths']}, inputs | more_inputs | {linker_path}
