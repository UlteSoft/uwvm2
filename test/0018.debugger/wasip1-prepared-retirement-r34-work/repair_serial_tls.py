from pathlib import Path
import json, hashlib, shutil

L = Path(__file__).parent
B = L.parents[3]
M = json.loads((L / 'repaired-inputs-v7.json').read_text())
before = L / 'before-v8-serial-tls'
assert not before.exists()
rows = []
for repo in ('uwvm2', 'uwvm2-ros'):
    for fixture in ('debug_checkpoint_prepared_retirement', 'debug_checkpoint_native_cohort_retirement'):
        n = repo + '/test/0017.runtime/' + fixture + '_runtime.cc'
        p = B / n
        h = hashlib.file_digest(p.open('rb'), 'sha256').hexdigest()
        assert h == M[n], ('concurrent edit', p)
        b = before / n
        b.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(p, b)
        s = p.read_text()
        def change(old, new):
            global s
            assert s.count(old) == 1, (p, old, s.count(old))
            s = s.replace(old, new)
        change('bool release_body{}, release_tls{};',
            'bool release_body{};\n    ::std::array<bool,2u> release_tls{};\n    ::std::size_t tls_entered_mask{};')
        change('struct tls_teardown\n{\n    ::std::shared_ptr<exit_windows> windows{};',
            'struct tls_teardown\n{\n    ::std::shared_ptr<exit_windows> windows{};\n    ::std::size_t index{};')
        change('::std::unique_lock lock{windows->mutex}; ++windows->tls_waiters; windows->changed.notify_all();\n        windows->changed.wait(lock,[&] { return windows->release_tls; });',
            'REQUIRE(index < 2u);\n        ::std::unique_lock lock{windows->mutex};\n        REQUIRE((windows->tls_entered_mask & (::std::size_t{1u} << index)) == 0u);\n        windows->tls_entered_mask |= ::std::size_t{1u} << index;\n        ++windows->tls_waiters; windows->changed.notify_all();\n        windows->changed.wait(lock,[&] { return windows->release_tls[index]; });')
        change('::std::uint32_t output{0xa5a5a5a5u};',
            '::std::uint32_t output{0xa5a5a5a5u};\n    ::std::size_t index{};')
        change('teardown.windows = self.windows;', 'teardown.windows = self.windows; teardown.index = self.index;')
        change('launches[index]->observed = state; launches[index]->windows = windows; launches[index]->start = start;',
            'launches[index]->observed = state; launches[index]->windows = windows; launches[index]->start = start;\n        launches[index]->index = index;')
        change('REQUIRE(windows->changed.wait_until(lock,deadline(),[&] { return windows->tls_waiters == 2u; }));\n      REQUIRE(windows->tls_finished == 0u);',
            '// Windows serializes thread-detach callbacks under the loader lock.\n      // One blocked TLS callback can keep its peer from entering TLS cleanup.\n      auto const entered{windows->changed.wait_until(lock,deadline(),[&] { return windows->tls_waiters != 0u; })};\n      if(!entered) { ::fast_io::print(::fast_io::err(),"NATIVE_TLS_TIMEOUT entered=",::fast_io::mnp::dec(windows->tls_waiters),\n          " finished=",::fast_io::mnp::dec(windows->tls_finished),"\\n"); }\n      REQUIRE(entered && windows->tls_entered_mask != 0u && windows->tls_waiters <= 2u);\n      REQUIRE(windows->tls_finished == 0u);')
        partial = '''// Release one real TLS destructor, then require its peer to enter and\n    // remain blocked. This exercises both physical owners without requiring\n    // concurrent Windows loader callbacks, and proves partial exit is insufficient.\n    ::std::size_t first_tls{};\n    { ::std::lock_guard lock{windows->mutex};\n      first_tls = (windows->tls_entered_mask & 1u) != 0u ? 0u : 1u;\n      REQUIRE(windows->tls_entered_mask != 0u);\n      windows->release_tls[first_tls] = true; windows->changed.notify_all(); }\n    { ::std::unique_lock lock{windows->mutex};\n      REQUIRE(windows->changed.wait_until(lock,deadline(),[&] { return windows->tls_waiters == 2u && windows->tls_finished == 1u; }));\n      REQUIRE(windows->tls_entered_mask == 3u && !windows->release_tls[1u-first_tls]); }\n    auto partial_join{lib::llvm_jit_checkpoint_continue_native_retirement_host_api(operation,short_deadline())};\n    REQUIRE(partial_join.status == outcome::pending_native_join && partial_join.execution_drained && !partial_join.native_workers_joined);\n    legacy_refused();\n'''
        if fixture == 'debug_checkpoint_prepared_retirement':
            partial += '    REQUIRE(partial_join.candidate_world_retained);\n'
        partial += '    { ::std::lock_guard lock{windows->mutex}; windows->release_tls[1u-first_tls] = true; windows->changed.notify_all(); }'
        change('{ ::std::lock_guard lock{windows->mutex}; windows->release_tls = true; windows->changed.notify_all(); }', partial)
        change('" actual_workers=2 execution_pending=1 tls_pending=2 physical_join=1 admission_closed_until_join=1",',
            '" actual_workers=2 execution_pending=1 tls_pending=2 partial_tls_exit_refused=1 physical_join=1 admission_closed_until_join=1",')
        p.write_text(s)
        rows.append(dict(path=n, before_sha256=h, after_sha256=hashlib.file_digest(p.open('rb'), 'sha256').hexdigest()))
(L / 'serial-tls-repair-source.json').write_text(json.dumps(dict(passed=True, fixtures=rows,
    production_source_unchanged=True, original_twenty_second_deadlines_unchanged=True,
    both_tls_destructors_and_actual_joins_required=True,
    additional_partial_tls_exit_must_keep_admission_closed=True), indent=2) + '\n')
print('Synchronized four fixtures: serial TLS detach, both destructors and actual joins, partial exit refusal')
