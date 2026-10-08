#!/usr/bin/env python3
"""Actual until/advance product qualification; requires the original guarded Linux cgroup.

Consumes a completed fresh main/runtime/host API build record, pins its complete
source closure and dependencies, and compares genuine guest stops against the
official compiler DWARF line tables. Component fixtures cannot satisfy this run.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, resource, subprocess, sys
sys.dont_write_bytecode = True

def sha(p):
    with Path(p).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def require(value, message, detail=None):
    if not value:
        raise AssertionError((message, detail))

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source-root', type=Path, required=True)
    ap.add_argument('--build-record', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--wasm-clang', type=Path, required=True)
    ap.add_argument('--wasm-ld', type=Path, required=True)
    ap.add_argument('--rustc', type=Path, required=True)
    ap.add_argument('--wasm-tools', type=Path, required=True)
    ap.add_argument('--llvm-dwarfdump', type=Path, required=True)
    ap.add_argument('--language', choices=['c', 'cpp', 'objc', 'rust'], action='append')
    ap.add_argument('--dwarf-version', choices=[4, 5], type=int, action='append')
    ap.add_argument('--policy', choices=['instruction', 'unwind'], action='append')
    ap.add_argument('--case', choices=['until-call', 'advance-call', 'until-recursive', 'advance-recursive', 'return-bound', 'refusals', 'breakpoint', 'replacement'], action='append')
    ap.add_argument('--ros', action='store_true')
    args = ap.parse_args()
    root = args.source_root.resolve(strict=True)
    subprocess.run(['bash', str(root/'tools/ci/require_wasm3_test_cgroup.sh')], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=False)
    sys.path.insert(0, str(root/'test/0017.runtime'))
    import run_debug_source_step_cli as step
    import run_debug_language_experience_cli as language_oracle
    require(Path(step.__file__).resolve() == root/'test/0017.runtime/run_debug_source_step_cli.py', 'exact step oracle import')
    build_path = args.build_record.resolve(strict=True)
    build = json.loads(build_path.read_text())
    require(build['passed'] and build['all_inputs_after_unchanged'] and build['new_objects_only'], 'fresh completed full build required')
    require(Path(build['source_root']).resolve() == root, 'actual product source root')
    binary = Path(build['binary']).resolve(strict=True)
    require(sha(binary) == build['binary_sha256'], 'linked product hash')
    require(len(build['rows']) == 4 and all(row['returncode'] == 0 for row in build['rows']), 'actual main/runtime/host/link provenance')
    pins = dict(build['inputs'])
    for p, h in pins.items():
        require(sha(p) == h, 'actual build closure changed', p)
    manifest_path = root.parent/(root.name+'-current-manifest.json')
    manifest = json.loads(manifest_path.read_text())
    require(manifest['source_id'] == build['source_id'], 'current product source cut')
    expected = {v['path']: v['sha256'] for v in manifest['files']}
    current = {str(p.relative_to(root)): sha(p) for p in (root/'src').rglob('*')
               if p.is_file() and not p.name.startswith('._') and p.name != '.DS_Store'}
    require(current == {p:h for p,h in expected.items() if p.startswith('src/')}, 'complete current src membership/bytes')
    # Multi-call lld selects wasm-ld mode from argv[0]; preserve its symlink name.
    tools = {name:Path(os.path.abspath(getattr(args, name))) for name in ('wasm_clang','wasm_ld','rustc','wasm_tools','llvm_dwarfdump')}
    require(all(p.is_file() for p in tools.values()), 'actual tool files')
    sources = {'c':root/'test/0017.runtime/fixtures/debug_source_step_c.c',
               'cpp':root/'test/0017.runtime/fixtures/debug_source_step_cpp.cc',
               'objc':root/'test/0017.runtime/fixtures/debug_source_step_c.c',
               'rust':root/'test/0017.runtime/fixtures/debug_source_run_to_rust.rs'}
    for p in [binary,build_path,manifest_path,Path(__file__),Path(step.__file__),Path(step.metadata_cli.__file__),Path(language_oracle.__file__),
              root/'tools/ci/require_wasm3_test_cgroup.sh',*tools.values(),*sources.values()]:
        pins[str(p)] = sha(p)
    summary = {'passed':False,'scope':'actual ordinary/ROS product source run-to statements; no native language or non-x86_64 full-product parity claim',
               'source_id':build['source_id'],'product_sha256':sha(binary),'inputs':pins,'cgroup':Path('/proc/self/cgroup').read_text(),
               'linked_debug_string_merging':False,'tools':{},'commands':[],'cases':[],'fixtures':[]}
    def publish():
        (out/'results.json').write_text(json.dumps(summary,indent=2)+'\n')
    def run(argv, name):
        log = out/(name+'.log')
        with log.open('wb') as stream:
            result = subprocess.run(list(map(str,argv)),cwd=root,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
        require(log.stat().st_size <= 16*1024*1024, 'bounded tool log')
        summary['commands'].append({'argv':list(map(str,argv)),'returncode':result.returncode,'log':str(log),'log_sha256':sha(log)})
        publish(); require(result.returncode == 0, 'actual producer/tool failure', log.read_text()[-2000:])
        return log.read_text()
    try:
        for name, tool in tools.items():
            summary['tools'][name] = {'sha256':sha(tool),'version':run([tool,'--version'],'version-'+name)}
        if 'rust' in (args.language or sources):
            libdir = Path(run([tools['rustc'],'--print','target-libdir','--target','wasm32-unknown-unknown'],'rust-target-libdir').strip())
            libraries = sorted({p for pattern in ('libcore-*.rlib','libcompiler_builtins-*.rlib') for p in libdir.glob(pattern)})
            require(len(list(libdir.glob('libcore-*.rlib'))) == 1 and libraries, 'real installed target Rust libraries')
            pins.update({str(p):sha(p) for p in libraries})
        for language in args.language or sources:
            source = sources[language]
            markers = {}
            for name in ('STEP_PHYSICAL_CALL','STEP_AFTER_CALL','STEP_LEAF_ENTRY','STEP_RECURSIVE_ENTRY','STEP_RECURSIVE_RETURN'):
                lines = [i for i,line in enumerate(source.read_text().splitlines(),1) if name in line]
                require(len(lines) == 1, 'exact marker', name); markers[name] = lines[0]
            for version in args.dwarf_version or (4,5):
                stem = f'{language}-dwarf{version}-O1'; wasm = out/(stem+'.wasm')
                if language == 'rust':
                    run([tools['rustc'],'--edition=2021','--crate-name','source_run_to_rust','--target','wasm32-unknown-unknown',
                         '-C','panic=abort','-C','debuginfo=2','-C',f'dwarf-version={version}','-C','split-debuginfo=off',
                         '-C','opt-level=1','-C','codegen-units=1','-C','linker='+str(tools['wasm_ld']),
                         '-C','link-arg=--no-entry','-C','link-arg=-O0',source,'-o',wasm],'compile-'+stem)
                else:
                    obj = out/(stem+'.o')
                    run([tools['wasm_clang'],'--target=wasm32-unknown-unknown','-g',f'-gdwarf-{version}','-O1',
                         '-x',{'c':'c','cpp':'c++','objc':'objective-c'}[language],'-std=c++23' if language=='cpp' else '-std=c17',
                         '-nostdlib','-c',source,'-o',obj],'compile-'+stem)
                    run([tools['wasm_ld'],'-O0','--no-entry','--export-all',obj,'-o',wasm],'link-'+stem)
                pins[str(wasm)] = sha(wasm)
                run([tools['wasm_tools'],'validate',wasm],'wasm-validate-'+stem)
                run([tools['llvm_dwarfdump'],'--verify',wasm],'dwarf-verify-'+stem)
                oracle = run([tools['llvm_dwarfdump'],'--debug-line',wasm],'lines-'+stem)
                sequences = step.line_sequences(oracle); expressions = step.code_expressions(wasm)
                own_rows = language_oracle.fixture_statement_rows(oracle,source)
                require(all(any(row['line']==line for row in own_rows) for line in markers.values()),
                        'each run-to marker must be an actual own-file statement; no source row is fabricated',markers)
                indices = {name:step.metadata_cli.function(wasm,'source_step_'+name)[0] for name in ('outer','leaf','recursive')}
                summary['fixtures'].append({'name':stem,'wasm_sha256':sha(wasm),'source_sha256':sha(source),'markers':markers})
                for policy in args.policy or ('instruction','unwind'):
                    for kind in args.case or ('until-call','advance-call','until-recursive','advance-recursive','return-bound','refusals','breakpoint','replacement'):
                        require(sha(wasm) == pins[str(wasm)], 'unchanged actual validated module')
                        mode = ['-Raot'] if args.ros else ['-Rcc','jit','-Rcm','full']
                        argv = [str(binary),'-Rdbg',*mode,'-Rct','0','-Rllvm-call-stack',policy,
                                '-Rllvm-exception-dispatch','native-unwind','-Rllvm-cache-path','disable','--run',str(wasm)]
                        session = step.Session(argv,out/(stem+'-'+policy+'-'+kind+'.log'),expressions,sequences,source)
                        row = {'language':language,'dwarf_version':version,'diagnostic_policy':policy,'case':kind,'passed':False,
                               'argv':argv,'positions':session.positions,'actions':session.actions}
                        summary['cases'].append(row); publish()
                        try:
                            origin = indices['recursive'] if 'recursive' in kind else indices['leaf'] if kind=='return-bound' else indices['outer']
                            session.begin(origin)
                            marker = 'STEP_RECURSIVE_ENTRY' if 'recursive' in kind else 'STEP_LEAF_ENTRY' if kind=='return-bound' else 'STEP_PHYSICAL_CALL'
                            before = language_oracle.own_seek(session,lambda p:p['function']==origin and p['line']==markers[marker] and p['is_statement'], 'real run-to origin')
                            depth = len(before['physical_functions']); require(depth > 0,'real complete guest backtrace')
                            if kind == 'replacement':
                                code = next(payload for tag,payload in step.metadata_cli.sections(wasm) if tag==10)
                                count,at = step.metadata_cli.u32(code,0,len(code));body = None
                                for index in range(count):
                                    size,at = step.metadata_cli.u32(code,at,len(code))
                                    require(size<=len(code)-at,'original body extent')
                                    if index==indices['leaf']:body = code[at:at+size]
                                    at += size
                                require(at==len(code) and body,'unchanged producer leaf body; focused fixture has no imports')
                                replacement = out/(stem+'-'+policy+'-original-leaf.bin')
                                replacement.write_bytes(body);pins[str(replacement)] = sha(replacement)
                                row['replacement_body_sha256'] = sha(replacement)
                                reply = session.send(f'replace 0 {indices["leaf"]} 1 {replacement}')
                                require(b'function replaced; generation 2' in reply,'real inactive leaf hot replacement',reply)
                                current = session.position()
                                require(current['function']==before['function'] and current['offset']==before['offset'],
                                        'successful replacement moved the actual paused outer instruction',(before,current))
                                before = current
                                command = f'advance {session.thread} {before["stop_id"]} {before["file"]}:{markers["STEP_LEAF_ENTRY"]}'
                                reply = session.console.send(command);session.actions.append(command)
                                require(reply.startswith(b'error: ') and b'source target has no emitted statement in the permitted module/frame' in reply,
                                        'replacement retired original leaf source mapping',reply)
                                require(session.position()['stop_id']==before['stop_id'],'rejected replacement source target resumed the guest')
                                kind_to_run = 'until-call'
                            elif kind == 'refusals':
                                for command in (f'until {session.thread} {before["stop_id"]-1} {before["file"]}:{markers["STEP_AFTER_CALL"]}',
                                                f'advance {session.thread} {before["stop_id"]} /absent-source.c:1',
                                                f'until {session.thread} {before["stop_id"]} {before["file"]}:999999',
                                                f'until {session.thread} {before["stop_id"]} {before["file"]}:{markers["STEP_LEAF_ENTRY"]}'):
                                    reply = session.console.send(command); session.actions.append(command)
                                    expected_error = b'command or thread is not valid in the current execution state' if command.startswith(f'until {session.thread} {before["stop_id"]-1} ') else b'source target has no emitted statement in the permitted module/frame'
                                    require(reply.startswith(b'error: ') and expected_error in reply, 'wrong/stale target must fail before resume',reply)
                                    require(session.position()['stop_id'] == before['stop_id'],'rejected target changed genuine stop')
                                kind_to_run = 'until-call'
                            else:
                                kind_to_run = kind
                            target = markers['STEP_RECURSIVE_RETURN'] if 'recursive' in kind_to_run else markers['STEP_LEAF_ENTRY'] if kind_to_run=='advance-call' else markers['STEP_RECURSIVE_ENTRY'] if kind_to_run=='return-bound' else markers['STEP_AFTER_CALL']
                            command = 'advance' if kind_to_run in ('advance-call','advance-recursive','return-bound') else 'until'
                            breakpoint_id = None
                            if kind_to_run == 'breakpoint':
                                reply = session.send(f'break 0 {indices["leaf"]} 0'); match = re.search(rb'breakpoint (\d+)',reply)
                                require(match is not None,'real interrupting breakpoint'); breakpoint_id = int(match[1])
                            # Exercise both native-style aliases and explicit stop-bound grammar.
                            text = f'{command} {session.thread} {before["stop_id"]} {before["file"]}:{target}' if version==4 else (f'{command} {target}' if kind_to_run in ('until-call','advance-call','until-recursive','advance-recursive') else f'{command} {before["file"]}:{target}')
                            reply = session.send(text)
                            if kind_to_run == 'breakpoint':
                                require(b'stopped: breakpoint' in reply,'real breakpoint must preempt run-to',reply)
                                session.send(f'delete {breakpoint_id}')
                            else:
                                require(b'stopped: selected participant step' in reply,'actual run-to pause',reply)
                                after = session.position(); require(after['stop_id'] > before['stop_id'] and after['is_statement'],'fresh genuine statement stop')
                                if kind_to_run == 'return-bound':
                                    require(after['function']==indices['outer'] and len(after['physical_functions'])==depth-1,'return boundary instead of later target',after)
                                else:
                                    require(after['line']==target,'exact requested line',after)
                                    if kind_to_run in ('until-call','until-recursive'):
                                        require(after['function']==origin and len(after['physical_functions'])==depth,'until skipped genuine recursive/callee activations',after)
                                    else:
                                        require(len(after['physical_functions'])>depth,'advance entered actual child activation',after)
                            session.finish_guest(); row['passed'] = True
                        finally:
                            session.console.finish(); row['quit_returncode'] = session.console.child.returncode
                            row['managed_shutdown_complete'] = b'managed shutdown complete' in session.console.transcript
                            row['log_sha256'] = sha(session.console.logpath)
                            require(row['quit_returncode']==0 and row['managed_shutdown_complete'],'actual managed shutdown')
                            publish()
        summary['inputs_after_unchanged'] = all(sha(p)==h for p,h in pins.items())
        require(summary['inputs_after_unchanged'],'actual source/tool/build/input changed')
        summary['passed'] = True
    except BaseException as error:
        summary['error'] = repr(error); raise
    finally:
        publish()
    print('PASS real until/advance statements:',len(summary['cases']))

if __name__ == '__main__':
    main()
