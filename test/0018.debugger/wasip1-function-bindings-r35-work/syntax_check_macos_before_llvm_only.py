from pathlib import Path
import ast, hashlib, importlib.util, json, os, platform, resource, shlex, signal, subprocess, sys, time
L=Path(__file__).parent
B=L.parents[3]
assert sys.platform=='darwin' and platform.machine()=='arm64'
M=L.parent/'wasip1-prepared-retirement-r34-work/macos_monitor.py'
spec=importlib.util.spec_from_file_location('r35_mac_accounting',M)
m=importlib.util.module_from_spec(spec);sys.modules[spec.name]=m;spec.loader.exec_module(m)
sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
compiler=Path('/Users/liyinan/Documents/MacroModel/tool-chain/tools/aarch64-apple-darwin-llvm/llvm/bin/clang++')
sdk=Path('/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk')
llvm=compiler.parent.parent/'include'
openssl=Path('/opt/homebrew/opt/openssl@3/include')
limit=2<<30;reserve=128<<20
assert compiler.is_file() and (llvm/'llvm/ExecutionEngine/ExecutionEngine.h').is_file()
profile='(version 1)(allow default)(deny process-fork)'
native=m.MacNative()
rows=[]
def stable_rss(pid,birth):
    for attempt in range(40):
        current=native.bsd(pid,missing=True)
        if current is None:return 0
        assert native.birth(current)==birth
        if current.status==5:return 0
        try:return native.rss(pid,current)
        except RuntimeError as e:
            if 'bytes=0, errno=3,' not in str(e) or attempt==39:raise
            time.sleep(.001)
def run(label,argv,timeout=180):
    log=L/(label+'.log');assert not log.exists()
    status=None;birth=None;peak=0;start=time.monotonic()
    with log.open('wb') as output:
        p=subprocess.Popen(['/usr/bin/sandbox-exec','-p',profile,*map(str,argv)],
            stdout=output,stderr=subprocess.STDOUT,start_new_session=True,
            preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_FSIZE,(8<<20,8<<20)))
        try:
            birth=native.birth(native.bsd(p.pid))
            assert birth.uid==os.getuid() and birth.pgid==p.pid
            while status is None:
                current=native.bsd(p.pid);assert native.birth(current)==birth
                assert all(i==p.pid for i in native.members(p.pid))
                peak=max(peak,stable_rss(p.pid,birth))
                assert peak+resource.getrusage(resource.RUSAGE_SELF).ru_maxrss+reserve<limit,'2 GiB aggregate budget'
                waited,raw,usage=os.wait4(p.pid,os.WNOHANG)
                if waited:status=os.waitstatus_to_exitcode(raw);p.returncode=status;break
                assert time.monotonic()-start<timeout and log.stat().st_size<8<<20
                time.sleep(.005)
        finally:
            if status is None:
                current=native.bsd(p.pid,missing=True)
                if current is not None:
                    assert birth is not None and native.birth(current)==birth
                    if current.status!=5:os.kill(p.pid,signal.SIGKILL)
                os.wait4(p.pid,0)
    upper=max(peak,usage.ru_maxrss)+resource.getrusage(resource.RUSAGE_SELF).ru_maxrss+reserve
    assert upper<limit
    row=dict(label=label,argv=list(map(str,argv)),exit=status,log_sha256=sha(log),
        aggregate_peak_upper_bytes=upper,limit_bytes=limit,controller_reserve_bytes=reserve,
        owned_ramdisk_capacity_bytes=0,pid_reaped=True,identity=birth.__dict__,seconds=time.monotonic()-start,
        validation_kind='compiler frontend syntax only; no native fixture execution claimed')
    rows.append(row);(L/'macos-syntax-rows.json').write_text(json.dumps(rows,indent=2)+'\n')
    print(label,status,upper,flush=True)
    return status,log
try:
    status,log=run('syntax-nofork-probe',[sys.executable,'-c','import os; os.fork()'],10)
    assert status!=0 and b'Operation not permitted' in log.read_bytes()
    for repo in ('uwvm2','uwvm2-ros'):
        root=B/repo
        source_inputs={str(p):sha(p) for d in ('src','third-parties') for p in (root/d).rglob('*')
            if p.is_file() and p.suffix in ('.h','.hpp','.hh','.inc','.cpp','.cppm','.def')}
        (L/(repo+'-syntax-source-before.json')).write_text(json.dumps(source_inputs,indent=2)+'\n')
        common=['-std=c++26','-stdlib=libc++','-isysroot',str(sdk),'-pthread','-g0','-O0',
            '-fexceptions','-fno-rtti','-fasynchronous-unwind-tables','-Wno-undefined-inline',
            '-Wno-deprecated-declarations','-fconstexpr-steps=64000000','-DUWVM=2',
            '-DUWVM_USE_LLVM_JIT','-DUWVM_USE_UWVM_INT','-DUWVM_USE_THREAD_LOCAL',
            '-DUWVM_RUNTIME_LLVM_JIT_CACHE_USE_OPENSSL_ED25519',
            '-DUWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2=1',
            '-DUWVM_VERSION_X=2','-DUWVM_VERSION_Y=0','-DUWVM_VERSION_Z=4','-DUWVM_VERSION_S=0',
            '-I'+str(root/'src'),'-I'+str(root/'third-parties/fast_io/include'),
            '-I'+str(root/'third-parties/bizwen/include'),'-I'+str(root/'third-parties/boost_unordered/include'),
            '-I'+str(llvm),'-I'+str(openssl),'-fsyntax-only']
        for label,source in [('runtime','src/uwvm2/runtime/lib/uwvm_runtime.default.cpp'),
            ('core','test/0017.runtime/debug_checkpoint_prepared_retirement_runtime.cc'),
            ('wasip1','test/0017.runtime/debug_wasip1_prepared_retirement_runtime.cc')]:
            driver=subprocess.run([str(compiler),*common,str(root/source),'-###'],capture_output=True,text=True,check=True)
            commands=[shlex.split(line) for line in driver.stderr.splitlines()
                if line.strip().startswith('"') and ' "-cc1" ' in line]
            assert len(commands)==1 and '-cc1' in commands[0] and '-fsyntax-only' in commands[0]
            status,log=run(repo+'-'+label+'-syntax',commands[0])
            assert all(sha(p)==h for p,h in source_inputs.items()),'Concurrent input edit during syntax check'
            if status!=0:print(log.read_text(errors='replace')[-12000:],flush=True);raise SystemExit(1)
    result=dict(passed=True,compiler=str(compiler),compiler_sha256=sha(compiler),monitor_sha256=sha(M),
        controller_sha256=sha(__file__),rows=rows,actual_native_runtime_execution_claimed=False)
    (L/'macos-syntax-qualified.json').write_text(json.dumps(result,indent=2)+'\n')
finally:native.close()
