"""SOURCE: bounded real Linux subprocess pipes and retained-pidfd retirement.

This helper never changes dumpability, constructs Runtime authority, compiles a
product or authenticates cache signatures. Callers must require real CLI store
and verified-hit diagnostics. Linux qualification still requires an actual run.
"""
import hashlib
import builtins
import importlib.machinery
import linecache
import stat
import types
import json
import os
from pathlib import Path
import re
import resource
import selectors
import signal
import subprocess
import sys
import time
from types import SimpleNamespace

OWNER_SHA = '37d9d95d078bfbeb8fbca80dc060e2274d1b569589770ed06895825a3a36d9c9'
RSS_SHA = '8039b352a592386fec73a39f1e2720ed25ddf0be7246ca078a6df1fc0711f874'
SEALED = b'[llvm-jit-cache] process-sealed dumpable=0 explicit-cli-request=1 persistence=subject-to-origin-admission\n'
HIT = re.compile(rb'^\[llvm-jit-cache\] object-cache-hit module="[^"]*" bytes=[1-9][0-9]* signature_verified=1$', re.M)
STORE = re.compile(rb'^\[llvm-jit-cache\] object-cache-store-complete module="[^"]*" status=ok bytes=[1-9][0-9]*$', re.M)


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def save(path, value):
    temp = path.with_suffix(path.suffix + '.tmp')
    temp.write_text(json.dumps(value, indent=2, ensure_ascii=False, allow_nan=False) + '\n')
    temp.replace(path)


def _module(name, path, expected):
    # The exact captured closure is admitted before any owner/RSS/helper executes.
    return __cache_source_context__.load_path(path, expected)


def _integers(path):
    return {key: int(value) for key, value in (line.split() for line in path.read_text().splitlines())}


def require_sealed(raw):
    assert b'\r' not in raw and raw.count(SEALED) == 1, 'actual integrated CLI seal diagnostic required'


def require_cache_phase(raw, phase):
    require_sealed(raw)
    hits = HIT.findall(raw)
    if phase == 'cold':
        assert not hits and STORE.search(raw), 'actual authenticated publication required; cache-off is failure'
    else:
        assert hits, 'actual signature_verified=1 cache hits required; recompilation is failure'
    return len(hits)


def accepts_expected_guest_trap(exit_code, text, expected):
    # Exact existing0072 run_trapping_conversions classifier semantics. This
    # validates Guest fatal intent AFTER lifecycle retirement, never all signals.
    ansi = re.compile(r'\x1b\[[0-?]*[ -/]*[@-~]')
    fatal = re.compile(r'^uwvm: \[fatal\] Runtime crash \(([^\r\n]+)\)$', re.MULTILINE)
    return exit_code in {-signal.SIGILL, -signal.SIGTRAP, -signal.SIGABRT} and fatal.findall(ansi.sub('', text)) == [expected]


class BoundedCli:
    """One process at a time. Parent files are opened only after child retirement.

    Ownership sampling has the retained helper's explicit detached-process limit.
    RSS is sampled, with absent witnesses left unknown rather than asserted zero.
    """
    def __init__(self, guard, output, repository):
        assert sys.platform == 'linux', 'all execution is SSH Linux only'
        assert dict(guard) == __cache_source_context__.guard, 'process guard differs from Source admission'
        __cache_source_context__.audit()
        assert guard['release_note'].strip(), 'explicit released Linux-window receipt required'
        self.expected, self.output = guard, Path(output).resolve(strict=True)
        self.membership = guard['membership']
        assert self.membership.startswith('0::/') and '\n' not in self.membership
        self.cgroup = Path('/sys/fs/cgroup') / self.membership[3:].lstrip('/')
        self.cpu = int(guard['cpu'])
        assert self.cpu in os.sched_getaffinity(0)
        self.events = _integers(self.cgroup / 'memory.events')
        assert self.events.get('oom', 0) == self.events.get('oom_kill', 0) == 0
        self.guard()
        q = Path(repository) / 'test/0050.high_level_languages/threaded_profile/qualification-r3'
        self.owner_path, self.rss_path = q / 'owned_process.py', q / 'owned_rss_observer.py'
        self.owner = _module('cache_real_pidfd_owner', self.owner_path, OWNER_SHA)
        rss = _module('cache_real_owned_rss', self.rss_path, RSS_SHA)
        self.rss = rss.OwnedRssObserver(self.membership)
        self.commands = []

    def guard(self, child=None):
        __cache_source_context__.audit()
        e = self.expected
        assert Path('/proc/sys/kernel/random/boot_id').read_text().strip() == e['boot_id']
        assert Path('/proc/self/cgroup').read_text().strip() == self.membership
        assert self.cgroup.stat().st_ino == e['cgroup_inode']
        assert (self.cgroup / 'memory.max').read_text().strip() == '68719476736'
        assert (self.cgroup / 'memory.swap.max').read_text().strip() == '0'
        assert _integers(self.cgroup / 'memory.events') == self.events, 'memory events changed'
        anon = _integers(self.cgroup / 'memory.stat')['anon']
        assert anon < 50 << 30
        disk = os.statvfs(self.output)
        assert disk.f_bavail * disk.f_frsize >= 2 << 30
        samples = []
        if child is not None:
            self.owner.remember_process(child)
            actual = child._uwvm_owned_process
            samples = [self.rss.sample(m['stat'], actual) for m in list(actual.members.values())]
            known = sum(row['rss_bytes'] for row in samples if row['rss_bytes'] is not None)
            limit = int(e.get('owned_rss_bytes', 4 << 30))
            assert 0 < limit <= 8 << 30 and known < limit
            self.rss.resolve_pending()
        return dict(boot_id=e['boot_id'], membership=self.membership, cgroup_inode=e['cgroup_inode'],
                    memory_max=64 << 30, swap_max=0, anon_bytes=anon, memory_events=self.events, rss=samples)

    def execute(self, name, argv, *, cwd=None, env=None, timeout=120, expected_termination=None, interaction=None, address_space_limit_bytes=None):
        assert expected_termination in (None, 'wasm-fatal-signal'), 'unknown expected termination contract'
        assert interaction in (None, 'debug-continue-quit'), 'unknown interactive protocol'
        assert address_space_limit_bytes is None or (type(address_space_limit_bytes) is int and 0 < address_space_limit_bytes <= 8 << 30), 'invalid per-command address-space limit'
        assert 0 < timeout <= 600 and Path(name).name == name and name not in ('.', '..')
        directory = self.output / (name + '.lifecycle')
        directory.mkdir(exist_ok=False)
        row = dict(argv=list(map(str, argv)), passed=False, natural_exit_observed=False,
                   address_space_limit_bytes=address_space_limit_bytes,
                   host_signal_is_success=False, stdout_stderr='separate bounded PIPE; parent-only persistence',
                   release_note=self.expected['release_note'], expected_termination=expected_termination,
                   semantic_classification_required=True, semantic_qualified=False, sampled_guard_count=0,
                   peak_sampled_known_rss_bytes=None, recent_guard_samples=[])
        debug = dict(prompt_count=0, commands=[], continued=False, normal_exit_seen=False, quit_sent=False, last_status=float('-inf'))
        row['captured_source_context_before'] = __cache_source_context__.receipt()
        row['inputs_before'] = {}
        for argument in argv:
            # Captured-source hex argv is DATA, not a filesystem path.
            if len(os.fsencode(argument)) > 4096:
                continue
            try:
                path = Path(argument)
                if path.is_file():
                    row['inputs_before'][str(path.resolve())] = digest(path)
            except OSError:
                continue
        child = None
        selector = selectors.DefaultSelector()
        buffers = {'stdout': bytearray(), 'stderr': bytearray()}
        started = time.monotonic()
        old_handlers = {number: signal.getsignal(number) for number in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP)}
        def interrupted(number, frame):
            raise KeyboardInterrupt('signal ' + str(number))
        for number in old_handlers:
            signal.signal(number, interrupted)
        try:
            row['guard_before'] = self.guard()
            def placement():
                os.sched_setaffinity(0, {self.cpu})
                resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
                if address_space_limit_bytes is not None:
                    resource.setrlimit(resource.RLIMIT_AS, (address_space_limit_bytes, address_space_limit_bytes))
            child = subprocess.Popen(argv, cwd=cwd, env=env, stdin=subprocess.PIPE if interaction else subprocess.DEVNULL,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, close_fds=True,
                start_new_session=True, preexec_fn=placement)
            self.owner.remember_process(child, force=True) # Before any poll/wait.
            if interaction:
                os.set_blocking(child.stdin.fileno(), False)
            for key, stream in (('stdout', child.stdout), ('stderr', child.stderr)):
                os.set_blocking(stream.fileno(), False)
                selector.register(stream, selectors.EVENT_READ, key)
            deadline, exit_deadline, last_guard = started + timeout, None, float('-inf')
            while True:
                self.owner.remember_process(child)
                now = time.monotonic()
                assert now < deadline, 'command deadline exceeded'
                if now - last_guard >= .5:
                    sample = self.guard(child)
                    row['sampled_guard_count'] += 1
                    row['recent_guard_samples'].append(sample)
                    del row['recent_guard_samples'][:-16]
                    witnessed = [m['rss_bytes'] for m in sample['rss'] if m['rss_bytes'] is not None]
                    if witnessed:
                        known = sum(witnessed)
                        row['peak_sampled_known_rss_bytes'] = max(row['peak_sampled_known_rss_bytes'] or 0, known)
                    last_guard = now
                for key, _ in selector.select(.05):
                    try:
                        raw = os.read(key.fd, 65536)
                    except BlockingIOError:
                        continue
                    if raw:
                        assert sum(map(len, buffers.values())) + len(raw) <= 8 << 20, 'raw capture cap'
                        buffers[key.data].extend(raw)
                    else:
                        selector.unregister(key.fileobj)
                if interaction:
                    view = bytes(buffers['stdout']) + bytes(buffers['stderr'])
                    prompts = view.count(b'(uwvm-debug) ')
                    debug['normal_exit_seen'] = b'Program exited normally.' in view
                    if prompts > debug['prompt_count']:
                        command = None
                        if not debug['continued']:
                            command = b'continue\n'
                        elif debug['normal_exit_seen'] and not debug['quit_sent']:
                            command = b'quit\n'
                        elif not debug['quit_sent'] and now - debug['last_status'] >= .03:
                            command = b'status\n'
                        if command is not None:
                            assert len(debug['commands']) < 1024, 'bounded debugger control commands'
                            try:
                                sent = os.write(child.stdin.fileno(), command)
                            except BlockingIOError:
                                sent = 0
                            assert sent in (0, len(command)), 'partial bounded debugger command'
                            if sent:
                                debug['commands'].append(command.decode('ascii').strip())
                                debug['prompt_count'] = prompts
                                debug['continued'] = True
                                debug['last_status'] = now
                                if command == b'quit\n':
                                    debug['quit_sent'] = True
                code = child.poll()
                if code is not None:
                    if exit_deadline is None:
                        exit_deadline = time.monotonic() + 1
                        row.update(returncode=code, natural_exit_observed=True)
                    if not selector.get_map():
                        break
                    assert time.monotonic() < exit_deadline, 'pipe retained after leader exit'
            if row['returncode'] < 0:
                allowed = {-signal.SIGILL, -signal.SIGTRAP, -signal.SIGABRT}
                assert expected_termination == 'wasm-fatal-signal' and row['returncode'] in allowed, 'unapproved native Host signal'
                row['unclassified_expected_guest_fatal_signal'] = -row['returncode']
                # Lifecycle completion alone grants no trap/cache credit. The
                # caller MUST retain its exact original fatal/stack classifier.
            if interaction:
                assert debug['continued'] and debug['normal_exit_seen'] and debug['quit_sent'], 'debugger Guest did not exit normally before quit'
                row['debug_protocol'] = debug
            row['passed'] = True # Lifecycle only; caller must validate Guest/cache oracle.
        except BaseException as exc:
            row.update(passed=False, error=repr(exc))
        finally:
            for number in old_handlers:
                signal.signal(number, signal.SIG_IGN)
            try:
                if child is not None:
                    self.owner.terminate_group(child, term_timeout=3, kill_timeout=2)
                    row['ownership'] = self.owner.owned_process_record(child)
                    self.rss.resolve_retired(child._uwvm_owned_process)
                    self.rss.require_resolved()
                    assert row['ownership']['closed'] and not row['ownership']['signals'], 'cleanup signals exclude success'
                    assert all(not m['pidfd_retained'] for m in row['ownership']['members'])
                    row['retired'] = True
                    row['post_retirement_identities'] = []
                    for member in row['ownership']['members']:
                        current = self.owner._stat(member['identity']['pid'])
                        absent = current is None or current['start'] != member['identity']['start']
                        row['post_retirement_identities'].append(dict(original=member['identity'], current=current, original_identity_absent_or_reused=absent))
                        assert absent, 'owned identity remains'
                row['guard_after'] = self.guard()
                row['captured_source_context_after'] = __cache_source_context__.receipt()
                row['inputs_after'] = {path: digest(path) for path in row['inputs_before']}
                assert row['inputs_after'] == row['inputs_before']
                assert digest(self.owner_path) == OWNER_SHA and digest(self.rss_path) == RSS_SHA
            except BaseException as exc:
                row.update(passed=False, retirement_error=repr(exc))
            finally:
                selector.close()
                if child is not None:
                    child.stdout.close(); child.stderr.close()
                    if child.stdin is not None:
                        child.stdin.close()
                row['seconds'] = time.monotonic() - started
                row['rss_peak_limitation'] = 'sampled owned RSS only; no full-lifetime upper bound'
                for key, raw in buffers.items():
                    path = directory / (key + '.raw')
                    path.write_bytes(raw)
                    row[key] = dict(path=str(path), bytes=len(raw), sha256=digest(path))
                save(directory / 'receipt.json', row)
                self.commands.append(row)
                for number, handler in old_handlers.items():
                    signal.signal(number, handler)
        assert row['passed'] and row.get('retired'), row
        return SimpleNamespace(returncode=row['returncode'], stdout=bytes(buffers['stdout']),
            stderr=bytes(buffers['stderr']), lifecycle=dict(path=str(directory / 'receipt.json'),
            sha256=digest(directory / 'receipt.json')))


# This source is executed only by the admitted future Linux bootstrap. It is
# test/evidence identity plumbing, never Runtime/cache-origin authority.
class _CapturedLoader:
    def __init__(self, context, alias):
        self.context, self.alias = context, alias

    def get_filename(self, fullname):
        assert fullname == self.context.prefix + '.' + self.alias
        return str(self.context.paths[self.alias])

    def get_source(self, fullname):
        self.get_filename(fullname)
        return self.context.sources[self.alias].decode('utf-8')

    def is_package(self, fullname):
        self.get_filename(fullname)
        return False

    def create_module(self, spec):
        raise RuntimeError('only the context may create captured modules')

    def exec_module(self, module):
        raise RuntimeError('ambient import/reload is not an admitted context load')


class SourceContext:
    """Capture the whole reviewed local closure before loading any member.

    Python/startup/stdlib/extensions are an independently admitted trust boundary.
    This context never claims an interpreter is hermetic or self-authenticated.
    """
    def __init__(self, repository, guard, bootstrap, bootstrap_bytes, package):
        self.repository = Path(repository).resolve(strict=True)
        self.guard = dict(guard)
        self.bootstrap = bootstrap
        self.prefix = package.__name__
        self.package = package
        self.modules = {'cache_cli_process': bootstrap}
        self.states = {'cache_cli_process': 'ready'}
        self.owned = {package.__name__: package, bootstrap.__name__: bootstrap}
        self.linecache_owned = {}
        self.sources, self.paths, self.rows = {}, {}, {}
        self.closed = False
        self.startup_path = tuple(sys.path)
        self.startup_meta_path = tuple(sys.meta_path)
        self.startup_path_hooks = tuple(sys.path_hooks)
        self.executed = []
        try:
            manifest_path = Path(os.environ['UWVM2_CACHE_SOURCE_CLOSURE']).resolve(strict=True)
            expected = os.environ['UWVM2_CACHE_SOURCE_CLOSURE_SHA256']
            raw = self._read(manifest_path, 1 << 20)
            assert re.fullmatch('[0-9a-f]{64}', expected) and hashlib.sha256(raw).hexdigest() == expected
            self.manifest_path, self.manifest_sha256 = manifest_path, expected
            manifest = json.loads(raw)
            assert manifest['schema'] == 'uwvm-cache-captured-source-closure-v1'
            assert manifest['repository_variant'] in ('uwvm2', 'uwvm2-stable', 'uwvm2-ros')
            assert 1 <= len(manifest['modules']) <= 96
            self.variant = manifest['repository_variant']
            total = 0
            for alias, row in manifest['modules'].items():
                assert re.fullmatch('[A-Za-z_][A-Za-z_0-9]*', alias)
                relative = Path(row['path'])
                assert not relative.is_absolute() and '..' not in relative.parts and relative.suffix == '.py'
                path = self.repository / relative
                assert path.resolve(strict=True) == path and path.is_relative_to(self.repository)
                assert type(row['bytes']) is int and 0 <= row['bytes'] <= 2 << 20 and total + row['bytes'] <= 16 << 20
                source = self._read(path, min(2 << 20, (16 << 20) - total))
                source.decode('utf-8', errors='strict')
                assert len(source) == row['bytes'] and hashlib.sha256(source).hexdigest() == row['sha256']
                assert row['role'] in ('module', 'entry', 'stdlib-only-producer')
                assert all(isinstance(name, str) for name in row['local_imports'] + row['stdlib_roots'])
                total += len(source)
                assert total <= 16 << 20
                self.sources[alias], self.paths[alias], self.rows[alias] = source, path, row
            assert total <= 16 << 20
            assert len(set(self.paths.values())) == len(self.paths)
            assert self.paths['cache_cli_process'] == Path(bootstrap.__file__).resolve(strict=True)
            assert self.sources['cache_cli_process'] == bootstrap_bytes
            for alias, row in self.rows.items():
                assert set(row['local_imports']) <= set(self.rows), ('undeclared transitive local closure', alias)
                assert set(row['stdlib_roots']) <= set(sys.stdlib_module_names) | set(sys.builtin_module_names)
            self._check_guard()
            bootstrap.__cache_source_context__ = self
            bootstrap.__source_bytes_sha256__ = self.rows['cache_cli_process']['sha256']
            bootstrap.__source_closure_sha256__ = expected
            bootstrap.__loader__ = _CapturedLoader(self, 'cache_cli_process')
            bootstrap.__spec__ = importlib.machinery.ModuleSpec(bootstrap.__name__, bootstrap.__loader__, origin=bootstrap.__file__)
            bootstrap.__spec__.has_location = True
            bootstrap.__cached__ = None
            self._linecache('cache_cli_process')
            self.executed.append(dict(alias='cache_cli_process', sha256=self.rows['cache_cli_process']['sha256'], bootstrap_exact_bytes=True))
            self.audit()
        except BaseException:
            self.close(audit=False)
            raise

    @staticmethod
    def _read(path, cap):
        assert path.is_absolute()
        fd = os.open(path, os.O_RDONLY | os.O_CLOEXEC | os.O_NOFOLLOW)
        try:
            first = os.fstat(fd)
            assert stat.S_ISREG(first.st_mode) and first.st_uid == os.geteuid() and 0 <= first.st_size <= cap
            data = bytearray()
            while len(data) <= cap:
                part = os.read(fd, min(65536, cap + 1 - len(data)))
                if not part:
                    break
                data.extend(part)
            last = os.fstat(fd)
            identity = lambda s: (s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns, s.st_ctime_ns)
            assert identity(first) == identity(last) and len(data) == first.st_size <= cap
            assert path.stat().st_dev == first.st_dev and path.stat().st_ino == first.st_ino
            return bytes(data)
        finally:
            os.close(fd)

    def _check_guard(self):
        if sys.flags.optimize != 0:
            raise RuntimeError('candidate assertion code must stay enabled')
        assert sys.platform == 'linux' and sys.flags.isolated and sys.flags.no_site and sys.flags.safe_path
        guard = self.guard
        assert guard['release_note'].strip()
        membership = Path('/proc/self/cgroup').read_text().strip()
        assert membership == guard['membership'] and membership.startswith('0::/') and '\n' not in membership
        cgroup = (Path('/sys/fs/cgroup') / membership[3:].lstrip('/')).resolve(strict=True)
        assert cgroup.stat().st_ino == guard['cgroup_inode']
        assert Path('/proc/sys/kernel/random/boot_id').read_text().strip() == guard['boot_id']
        assert (cgroup / 'memory.max').read_text().strip() == '68719476736'
        assert (cgroup / 'memory.swap.max').read_text().strip() == '0'

    def _linecache(self, alias):
        path = str(self.paths[alias])
        previous = linecache.cache.get(path)
        owned = (len(self.sources[alias]), None, self.sources[alias].decode('utf-8').splitlines(True), path)
        self.linecache_owned[alias] = (previous, owned)
        linecache.cache[path] = owned

    def _unlinecache(self, alias):
        if alias not in self.linecache_owned:
            return
        previous, owned = self.linecache_owned.pop(alias)
        path = str(self.paths[alias])
        assert linecache.cache.get(path) is owned, 'captured linecache identity changed'
        if previous is None:
            del linecache.cache[path]
        else:
            linecache.cache[path] = previous

    def _import(self, caller, name, globals=None, locals=None, fromlist=(), level=0):
        assert not self.closed
        if level:
            assert level == 1 and globals is not None and globals.get('__package__') == self.prefix
            name = self.prefix + ('.' + name if name else '')
        if name == self.prefix:
            assert fromlist
            for alias in fromlist:
                assert alias != '*' and alias in self.rows
                self._allow_local(caller, alias)
                setattr(self.package, alias, self.load(alias))
            return self.package
        if name.startswith(self.prefix + '.'):
            name = name[len(self.prefix) + 1:]
        if name in self.rows:
            self._allow_local(caller, name)
            return self.load(name)
        root = name.split('.', 1)[0]
        assert not level and root in self.rows[caller]['stdlib_roots'], ('undeclared/ambient import', caller, name)
        assert root in sys.stdlib_module_names or root in sys.builtin_module_names
        # The independently pinned Python/stdlib/startup closure remains trusted.
        return builtins.__import__(name, globals, locals, fromlist, 0)

    def _allow_local(self, caller, alias):
        assert alias in self.rows[caller]['local_imports'], ('undeclared transitive edge', caller, alias)

    def load(self, alias):
        assert not self.closed and alias in self.rows
        self._check_guard()
        if alias in self.modules:
            module = self.modules[alias]
            assert self.states[alias] == 'ready', ('candidate import cycle/reentry rejected', alias)
            assert sys.modules.get(module.__name__) is module and self.owned.get(module.__name__) is module
            return module
        assert self.rows[alias]['role'] != 'stdlib-only-producer', 'script execution is a separate bounded child contract'
        fullname = self.prefix + '.' + alias
        assert fullname not in sys.modules and fullname not in self.owned
        module = types.ModuleType(fullname)
        loader = _CapturedLoader(self, alias)
        module.__file__ = str(self.paths[alias])
        module.__package__ = self.prefix
        module.__loader__ = loader
        module.__spec__ = importlib.machinery.ModuleSpec(fullname, loader, origin=module.__file__)
        module.__spec__.has_location = True
        module.__cached__ = None
        module.__source_bytes_sha256__ = self.rows[alias]['sha256']
        module.__source_closure_sha256__ = self.manifest_sha256
        module.__cache_source_context__ = self
        scope = dict(vars(builtins))
        scope['__import__'] = lambda name, globals=None, locals=None, fromlist=(), level=0: self._import(alias, name, globals, locals, fromlist, level)
        module.__dict__['__builtins__'] = scope
        self.modules[alias] = module
        self.states[alias] = 'loading'
        self.owned[fullname] = module
        sys.modules[fullname] = module  # Our fresh private name supports dataclasses/typing.
        setattr(self.package, alias, module)
        try:
            self._linecache(alias)
            code = builtins.compile(self.sources[alias], module.__file__, 'exec', dont_inherit=True, optimize=0)
            builtins.exec(code, module.__dict__, module.__dict__)
            assert sys.modules.get(fullname) is module and self.owned.get(fullname) is module
            self.states[alias] = 'ready'
            self.executed.append(dict(alias=alias, sha256=self.rows[alias]['sha256'], bootstrap_exact_bytes=False))
            self.audit()
            return module
        except BaseException:
            self._discard(alias)
            raise

    def _discard(self, alias):
        module = self.modules.pop(alias)
        fullname = module.__name__
        errors = []
        try:
            self._unlinecache(alias)
        except BaseException as exc:
            errors.append(repr(exc))
        if sys.modules.get(fullname) is module:
            del sys.modules[fullname]
        else:
            errors.append('private module identity changed: ' + fullname)
        self.owned.pop(fullname, None)
        self.states.pop(alias, None)
        if getattr(self.package, alias, None) is module:
            delattr(self.package, alias)
        assert not errors, errors

    def load_path(self, path, expected):
        path = Path(path).resolve(strict=True)
        found = [alias for alias, p in self.paths.items() if p == path]
        assert len(found) == 1 and self.rows[found[0]]['sha256'] == expected
        return self.load(found[0])

    def assert_loaded_scope(self, scope):
        name = scope['__name__']
        assert name.startswith(self.prefix + '.')
        alias = name[len(self.prefix) + 1:]
        assert alias in self.modules and self.states[alias] == 'loading'
        assert self.modules[alias].__dict__ is scope and sys.modules.get(name) is self.modules[alias]
        assert scope['__source_bytes_sha256__'] == self.rows[alias]['sha256']

    def bind_entry(self, scope):
        assert scope['__name__'] == '__main__'
        path = Path(scope['__file__']).resolve(strict=True)
        aliases = [alias for alias, p in self.paths.items() if p == path]
        assert len(aliases) == 1 and self.rows[aliases[0]]['role'] == 'entry'
        # The keeper admitted this entry before startup; this is consistency,
        # never retroactive authentication of already-running Python code.
        assert scope.get('__uwvm_entry_captured_bytes__') == self.sources[aliases[0]], 'keeper must compile exactly its captured entry bytes'
        assert scope.get('__uwvm_entry_keeper_identity__') == 'cae2c05f7df6e9a2a94a84f5333eeef5c1946d96ea8b54cfe59eee5e17d23d21', 'keeper literal/startup identity must be admitted externally'
        assert self._read(path, 2 << 20) == self.sources[aliases[0]]
        scope['__cache_source_context__'] = self
        scope['__source_closure_sha256__'] = self.manifest_sha256
        self.entry_consistency = dict(alias=aliases[0], path=str(path), sha256=self.rows[aliases[0]]['sha256'], keeper_prestartup_admission_required=True, keeper_captured_entry_bytes_equal=True, keeper_literal_sha256='cae2c05f7df6e9a2a94a84f5333eeef5c1946d96ea8b54cfe59eee5e17d23d21')
        self.check_alias_contract()

    def check_alias_contract(self):
        # Authored future-Linux negative contract, not cache/Guest qualification.
        # The unrelated ordinary alias is restored without adopting it as code.
        alias = 'owned_process'
        missing = object()
        old = sys.modules.get(alias, missing)
        poison = types.ModuleType(alias)
        poison.__source_contract_negative_fixture__ = True
        sys.modules[alias] = poison
        try:
            loaded = self.load(alias)
            assert loaded is not poison and loaded.__name__ == self.prefix + '.' + alias
            assert loaded.__source_bytes_sha256__ == self.rows[alias]['sha256']
            assert sys.modules[loaded.__name__] is loaded
            self.alias_contract = dict(ordinary_alias_ignored=True,
                captured_source_sha256=self.rows[alias]['sha256'],
                private_module_registered=True, actual_cache_or_Guest_qualified=False)
        finally:
            assert sys.modules.get(alias) is poison, 'ordinary negative fixture unexpectedly changed'
            if old is missing:
                del sys.modules[alias]
            else:
                sys.modules[alias] = old

    def python_source_argv(self, path, arguments):
        path = Path(path).resolve(strict=True)
        aliases = [alias for alias, p in self.paths.items() if p == path]
        assert len(aliases) == 1 and self.rows[aliases[0]]['role'] == 'stdlib-only-producer'
        alias = aliases[0]
        assert not self.rows[alias]['local_imports']
        assert len(self.sources[alias]) <= 48 << 10, 'captured producer argv bound'
        self.audit()
        wrapper = "import sys; raw=bytes.fromhex(sys.argv[1]); filename=sys.argv[2]; sys.argv=sys.argv[2:]; scope={'__name__':'__main__','__file__':filename,'__package__':None,'__cached__':None,'__spec__':None}; exec(compile(raw,filename,'exec',dont_inherit=True,optimize=0),scope,scope)"
        return [sys.executable, '-I', '-S', '-c', wrapper, self.sources[alias].hex(), str(path), *map(str, arguments)]

    def receipt(self):
        return dict(manifest_path=str(self.manifest_path), manifest_sha256=self.manifest_sha256,
            private_prefix=self.prefix, captured_modules={alias: row['sha256'] for alias, row in self.rows.items()},
            executed=list(self.executed), initial_entry_consistency=dict(self.entry_consistency), alias_negative_contract=dict(self.alias_contract), standard_library_interpreter_hermetic_claimed=False,
            startup_interpreter_and_stdlib_trust='independently pinned before direct -I -S entry startup')

    def audit(self):
        assert not self.closed
        self._check_guard()
        assert tuple(sys.path) == self.startup_path and tuple(sys.meta_path) == self.startup_meta_path and tuple(sys.path_hooks) == self.startup_path_hooks, 'startup import policy changed'
        assert hashlib.sha256(self._read(self.manifest_path, 1 << 20)).hexdigest() == self.manifest_sha256
        for alias, source in self.sources.items():
            assert self._read(self.paths[alias], 2 << 20) == source, ('captured Source drift', alias)
        for name, module in self.owned.items():
            assert sys.modules.get(name) is module, ('private module identity changed', name)
        for alias, (old, owned) in self.linecache_owned.items():
            assert linecache.cache.get(str(self.paths[alias])) is owned

    def close(self, *, audit=True):
        if self.closed:
            return
        try:
            if audit:
                assert all(state == 'ready' for state in self.states.values())
                self.audit()
        finally:
            errors = []
            for alias in list(self.linecache_owned):
                try:
                    self._unlinecache(alias)
                except BaseException as exc:
                    errors.append(repr(exc))
            for name, module in list(self.owned.items())[::-1]:
                if sys.modules.get(name) is module:
                    del sys.modules[name]
                else:
                    errors.append('private module identity changed: ' + name)
            self.owned.clear()
            self.closed = True
            assert not errors, errors


def admit_source_context(repository, guard, bootstrap, source, package):
    return SourceContext(repository, guard, bootstrap, source, package)
