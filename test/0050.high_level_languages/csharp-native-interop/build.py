#!/usr/bin/env python3
"""Build a private Mono WASI native-link proof; never install or patch an SDK."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import sys

sys.dont_write_bytecode = True
from harness import Session, SUITE, digest, durable_json, require, unchanged_admission
from inspect_wasm import inspect

SOURCE = Path(__file__).resolve().parent
ASSEMBLY = 'hll-native-interop'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dotnet-root', type=Path, required=True)
    parser.add_argument('--wasi-sdk', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--native-cc', type=Path, default=Path('/usr/bin/cc'))
    parser.add_argument('--nuget-packages', type=Path)
    parser.add_argument('--cpu', type=int, default=8)
    args = parser.parse_args()
    require(args.out.is_absolute() and not args.out.resolve().is_relative_to(SOURCE), 'new absolute output outside source required')
    args.out.mkdir(parents=True, exist_ok=False)
    record = {'passed': False, 'scope': 'build, native oracle, registered and linked C ABI; no WASI execution yet',
              'wasm_execution_qualified': False, 'ros_qualification': False,
              'pinned_dotnet_sdk': '8.0.408', 'pinned_wasi_runtime_pack': '8.0.15', 'pinned_wasi_sdk': '20.0',
              'source_sha256': {str(p): digest(p) for p in sorted(SOURCE.iterdir()) if p.is_file()}}
    session = None
    try:
        snapshot = args.out / 'sources'
        snapshot.mkdir()
        for path, checksum in record['source_sha256'].items():
            copy = snapshot / Path(path).name
            shutil.copy2(path, copy)
            require(digest(copy) == checksum, 'source changed while saving the attempt snapshot')
        session = Session(args.out, args.cpu)
        record.update(admission_before=session.before, boot=session.boot, cpu=args.cpu, commands=session.commands)
        dotnet_root, wasi = args.dotnet_root.resolve(strict=True), args.wasi_sdk.resolve(strict=True)
        require(wasi.name in ('wasi-sdk-20.0', 'wasi-sdk-20'), 'reuse the pinned wasi-sdk-20.0 installation')
        dotnet, clang = dotnet_root / 'dotnet', wasi / 'bin/clang'
        retain = SUITE / 'csharp/retain_symbols.py'
        tool_paths = [dotnet, clang, args.native_cc, retain]
        record['tool_sha256'] = {str(p): digest(p) for p in tool_paths}
        candidates = [p for p in (dotnet_root / 'packs').rglob('WasiApp.Native.targets') if '8.0.15' in p.parts]
        require(len(candidates) == 1, 'exact installed WASI 8.0.15 target required')
        target = candidates[0]
        text = target.read_text()
        require('<_WasmPInvokeModules Include="%(_WasmNativeFileForLinking.FileName)"' in text
                and "'%(_WasmNativeFileForLinking.ScanForPInvokes)' != 'false'" in text
                and '<_WasiFilePathForFixup Include="@(_WasmNativeFileForLinking)"' in text,
                'pinned private SDK item contract changed')
        record['sdk_targets'] = {'path': str(target), 'sha256': digest(target)}
        packages = (args.nuget_packages or Path(os.environ.get('NUGET_PACKAGES', str(Path.home() / '.nuget/packages')))).resolve()
        require(packages.is_dir(), 'reuse the existing pinned NuGet package cache; no downloads are performed')
        record['nuget_packages'] = str(packages)
        env = dict(os.environ, DOTNET_ROOT=str(dotnet_root), PATH=str(dotnet_root) + ':' + os.environ['PATH'],
                   DOTNET_CLI_HOME=str(args.out / 'dotnet-home'), DOTNET_CLI_TELEMETRY_OPTOUT='1',
                   DOTNET_SKIP_FIRST_TIME_EXPERIENCE='1', DOTNET_SYSTEM_GLOBALIZATION_INVARIANT='1',
                   DOTNET_CLI_WORKLOAD_UPDATE_NOTIFY_DISABLE='true', DOTNET_CLI_USE_MSBUILD_SERVER='0',
                   MSBUILDDISABLENODEREUSE='1', WASI_SDK_PATH=str(wasi),
                   NUGET_PACKAGES=str(packages))
        offline = args.out / 'empty-offline-nuget-feed'
        offline.mkdir()
        row = session.run([dotnet, '--version'], 'dotnet-version', cwd=SOURCE, env=env, timeout=30)
        require(Path(row['stdout']).read_text().strip() == '8.0.408', 'wrong .NET SDK version')
        row = session.run([clang, '--version'], 'wasi-clang-version', env=env, timeout=30)
        require(re.search(r'clang version 16\.', Path(row['stdout']).read_text()), 'wasi-sdk20 LLVM16 compiler required')
        bridge = args.out / 'bridge/hll_native_bridge.o'
        bridge.parent.mkdir()
        session.run([clang, '--target=wasm32-wasi', '--sysroot=' + str(wasi / 'share/wasi-sysroot'),
                     '-std=c11', '-O2', '-g', '-Wall', '-Wextra', '-Werror', '-c', SOURCE / 'hll_native_bridge.c',
                     '-o', bridge], 'compile-wasm-bridge', env=env)
        evidence = args.out / 'link-evidence'
        wasi_out = args.out / 'wasi'

        def properties(destination):
            return ['-p:BaseIntermediateOutputPath=' + str(destination / 'obj') + '/',
                    '-p:MSBuildProjectExtensionsPath=' + str(destination / 'obj') + '/',
                    '-p:BaseOutputPath=' + str(destination / 'bin') + '/', '-p:UseSharedCompilation=false']

        props = properties(wasi_out) + ['-p:WasmAppDir=' + str(wasi_out / 'AppBundle') + '/',
            '-p:HllNativeBridgeObject=' + str(bridge), '-p:HllInteropEvidenceDir=' + str(evidence),
            '-p:WasmNativeStrip=false', '-p:WasmNativeDebugSymbols=true']
        for name, project, destination, extra in [
                ('wasi', SOURCE / 'HllNativeInterop.csproj', wasi_out, props),
                ('native', SOURCE / 'HllNativeInteropNative.csproj', args.out / 'native', properties(args.out / 'native'))]:
            session.run([dotnet, 'restore', project, '--source', offline, '--ignore-failed-sources',
                         '-p:Configuration=Release', *extra], 'restore-' + name, cwd=SOURCE, env=env)
            session.run([dotnet, 'publish' if name == 'wasi' else 'build', project, '--no-restore',
                         '--disable-build-servers', '-c', 'Release', '-m:1', '-nr:false', *extra],
                        'build-' + name, cwd=SOURCE, env=env)
        response = wasi_out / 'obj/Release/net8.0/wasi-wasm/wasm/for-publish/clang-compile.rsp'
        table = response.with_name('pinvoke-table.h')
        arguments = shlex.split(response.read_text())
        require(arguments.count(str(bridge)) == 1, 'the exact bridge object must occur once in real link rsp')
        header = table.read_text()
        block = re.search(r'static\s+PinvokeImport\s+hll_native_bridge_imports\s*\[\s*\]\s*=\s*\{(.*?)\n\};', header, re.S)
        require(block is not None and '"hll_native_bridge"' in header, 'native PInvoke module was not registered')
        for function in ('hll_bridge_copy', 'hll_roundtrip_u64'):
            require(re.search(r'\{\s*"' + function + r'"\s*,\s*' + function + r'\s*\}', block[1]),
                    'registered PInvoke function missing: ' + function)
        require(re.search(r'uint64_t\s+hll_bridge_copy\s*\(\s*int\s*,\s*uint64_t\s*,\s*uint64_t\s*,\s*uint64_t\s*,\s*uint64_t\s*\)', header),
                'generated bridge prototype is not int + four uint64_t -> uint64_t')
        require(re.search(r'uint64_t\s+hll_roundtrip_u64\s*\(\s*uint64_t\s*\)', header), 'uint64 roundtrip prototype missing')
        injection = (evidence / 'injected-native-items.txt').read_text().splitlines()
        require(str(bridge) + '|hll_native_bridge|true' in injection, 'private hook did not preserve module filename/scan metadata')
        for path in (table, response):
            shutil.copy2(path, evidence / path.name)
        session.run([sys.executable, retain, wasi_out, clang, ASSEMBLY], 'retain-exact-symbols', cwd=SOURCE, env=env)
        relink = json.loads((wasi_out / 'symbol-link-command.json').read_text())
        expected = list(arguments)
        require(expected.count('-Wl,-s') == 1, 'unexpected original strip recipe')
        expected.remove('-Wl,-s')
        expected.insert(0, '-g')
        wasm = wasi_out / 'AppBundle' / (ASSEMBLY + '.wasm')
        expected[expected.index('-o') + 1] = str(wasm)
        require(relink == [str(clang), *expected], 'symbol relink did not reuse the exact original inputs')
        shutil.copy2(wasi_out / 'symbol-link-command.json', evidence / 'symbol-link-command.json')
        module = inspect(wasm)
        require(module['imports'] and all(row['module'] == 'wasi_snapshot_preview1' for row in module['imports']),
                'unexpected non-WASIp1 import; this proof has no external ROS/custom host imports')
        require(any(row['name'] == '_start' and row['kind'] == 0 for row in module['exports']), 'WASI command _start missing')
        require({'name', '.debug_info', '.debug_line'} <= set(module['custom']), 'retained native symbol/DWARF sections missing')
        require({'hll_bridge_copy', 'hll_roundtrip_u64'} <= set(module['functions'].values()), 'linked native function names missing')
        durable_json(evidence / 'inspect-wasm.json', module)
        library = args.out / 'native/libhll_native_bridge.so'
        session.run([args.native_cc, '-std=c11', '-O2', '-g', '-Wall', '-Wextra', '-Werror', '-fPIC', '-shared',
                     SOURCE / 'hll_native_bridge.c', '-o', library], 'compile-native-bridge', env=env)
        dll = args.out / 'native/bin/Release/net8.0/hll-native-interop-native.dll'
        native_env = dict(env, LD_LIBRARY_PATH=str(library.parent) + (':' + env['LD_LIBRARY_PATH'] if env.get('LD_LIBRARY_PATH') else ''))
        oracle = session.run([dotnet, '--fx-version', '8.0.15', dll], 'native-oracle', env=native_env, timeout=120)
        require(Path(oracle['stdout']).read_bytes() == (SOURCE / 'expected-native.stdout').read_bytes(), 'native oracle exact stdout mismatch')
        require(not Path(oracle['stderr']).read_bytes(), 'native oracle unexpected stderr')
        record.update(wasm=str(wasm), wasm_sha256=digest(wasm), bridge_object=str(bridge), bridge_sha256=digest(bridge),
                      native_dll=str(dll), native_dll_sha256=digest(dll), native_library=str(library),
                      native_library_sha256=digest(library), dotnet=str(dotnet), native_oracle_passed=True,
                      native_oracle_scope='same C source/managed Program; native 64-bit pointers; Wasm bounds explicitly not applicable',
                      link_evidence={str(p): digest(p) for p in sorted(evidence.iterdir()) if p.is_file()})
        durable_json(args.out / 'cases.json', [{'name': 'csharp-native-interop', 'language': 'csharp',
            'target': 'mono-wasm-private-linked-c-shim', 'wasm': str(wasm), 'wasm_sha256': digest(wasm),
            'expected_stdout': (SOURCE / 'expected-wasi.stdout').read_text(), 'skip_debug': True,
            'ordinary_timeout': 900, 'generated_source': str(SOURCE / 'Program.cs'),
            'generated_source_sha256': digest(SOURCE / 'Program.cs'),
            'oracle': str(args.out / 'commands/native-oracle/receipt.json'),
            'oracle_sha256': digest(args.out / 'commands/native-oracle/receipt.json'),
            'scope': 'generic C ABI only; no ROS or C# source-debugger claim'}])
        require(all(digest(p) == value for p, value in record['source_sha256'].items()), 'project source changed during build')
        require(all(digest(p) == value for p, value in record['tool_sha256'].items()), 'pinned tool changed during build')
        require(digest(target) == record['sdk_targets']['sha256'], 'SDK targets changed')
        record['admission_after'] = unchanged_admission(session.before)
        record['passed'] = True
    except BaseException as error:
        record['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        if session is not None:
            session.close()
        durable_json(args.out / 'receipt.json', record)


if __name__ == '__main__':
    main()
