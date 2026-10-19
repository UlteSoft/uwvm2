The original O2 three-Wasm producer is unchanged. Its parent-owned real UWVM DBG attempt reached an owned-memory guard before the initial prompt; that is not a DBG PASS. A later GDB sample reached compilation/lowering through `finalize_gc_root_frame`/`materialize_gc_root_snapshots`. One sample shows where execution progressed, not which allocation caused memory growth. Root retains those VM attempts separately.

Independent producer/reference evidence is in [evidence/updates-r1](evidence/updates-r1/producer-updates-r1.json):

| Producer | Defined functions | Declared locals | EvalFrame body / locals | Reference scope |
| --- | ---: | ---: | --- | --- |
| Original full O2 | 8,841 | 29,175 | 65,920B /33 | Original46 native/Wasmtime cases; parent owns UWVM |
| Separate full O0 | 16,926 | 1,359,537 | 273,226B /22,353 | Wasmtime11/11; UWVM DBG not attempted |
| Separate full Oz | 10,170 | 25,081 | 54,625B /28 | Wasmtime11/11; parent owns UWVM DBG |

O2 max declared locals is86, not a giant EvalFrame local table. O0 worsened locals/code shape and was not tried in UWVM DBG; this does not identify the original memory cause.

Full Oz artifact: `/tmp/high-level-20261017/python/debug-full-Oz-r1/artifacts/python-semantics-debug-Oz.wasm`, SHA256 `43c8139355ccee3ecee22fe139fe7c951a857d6026a3314fe6c04e7e37cca427`. Generated config.c retains all71 builtin entries and is byte-identical to full O2. Expanded signatures of all42 P1 imports match. Official source, SDK24, feature/assertion macro state and frozen bytecode/header remain identical. Python assertions stay enabled in optimize0 bytecode; this is C optimization, not --with-pydebug or a GC-feature change.

All252 matched actual clang compile commands end with effective -Oz; earlier default -O3 is overridden. The launcher links with -Oz -g; prebuilt SDK libraries retain SDK identity. Body bytes3,724,257→2,943,583 (-20.96%), encoded instruction bytes3,699,002→2,916,737. These are bytes, not opcode/safepoint counts. More outlined functions mean byte reduction alone cannot prove lower DBG memory.

The first Oz helper finished configure/make but rejected a backslash-continued compiler-log line. Its helper/receipt remain preserved. The fixed parser normalizes continuations and resumes only exact-argv successful, closed/retired stages; final link, effective flags and macro/config/shape audit passed.

The [Oz reference](evidence/updates-r1/debug-full-Oz-references-r1/report.json) runs six positive/five invalid cases without preopens, compares complete stdout/stderr/exit to the hash-bound native oracle, and joins every old manifest row by name/argv/env/output/exit. Its [DBG manifest](evidence/updates-r1/debug-full-Oz-references-r1/cases.json) enables only positive1,000 rounds and explicitly labels Oz. Future Oz DBG results cannot retrospectively qualify O2 performance Wasm DBG. No Python-locals bridge is supplied.

Static builtin attribution:2,176 functions/943,791 bodyB (25.34%) clearly optional;6,230/2,675,597B (71.84%) core/bootstrap. Decimal6.46%, XML5.49%, hash3.53%, other6.64%, math1.26%, CJK1.52%, network0.44%; unattributed2.58% and ambiguous0.23% remain separate. Test builtins were already absent. Archive/name attribution is not measured linker savings or DBG-memory evidence.

Minimal-r1 used source Setup.local, while VPATH consumed build-dir Setup.local. All71 entries remained and the exact configuration assertion prevented publication. The failed attempt remains preserved. A fixed helper writes build-dir Setup.local after configure, but minimal-r2 was not run because full Oz took priority. Any future reduced profile must declare its narrower stdlib scope.

Unchanged O2 timed/profile modules also have [200k references](evidence/updates-r1/references-r3/report.json), checksum673761, native source/scalar/native-embedded/Wasmtime agreement and real gates. New [ordinary](evidence/updates-r1/manifests-r3/perf-timed-cases.json)/[gated](evidence/updates-r1/manifests-r3/perf-profile-cases.json) rows bind those receipts and explicit noinherit. Their Wasm SHAs do not change; these reference times are not formal performance ratios.
