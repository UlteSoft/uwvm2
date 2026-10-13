from pathlib import Path
import difflib, hashlib, json, os, stat

L = Path(__file__).parent
B = L.parents[3]
sha = lambda data: hashlib.sha256(data).hexdigest()
changes = {}
before = {}

def edit(repo, relative, old, new):
    key = repo + "/" + relative
    if key not in changes:
        before[key] = (B / key).read_bytes()
        changes[key] = before[key].decode()
    assert changes[key].count(old) == 1, (key, old[:100], changes[key].count(old))
    changes[key] = changes[key].replace(old, new)

for repo in ("uwvm2", "uwvm2-ros"):
    root = "src/uwvm2/runtime/lib/"
    edit(repo, root + "uwvm_runtime.h",
         "source_initializer_binding_preparation_declined, full_publication_record_preparation_declined };",
         "source_initializer_binding_preparation_declined, full_publication_record_preparation_declined, full_code_owner_handoff_declined };")
    edit(repo, root + "uwvm_runtime.h",
         "        bool runtime_full_publication_records_prepared{}; // Final private shape; no LIVE publication.",
         "        bool runtime_full_publication_records_prepared{}; // Final private shape; no LIVE publication.\n"
         "        ::std::size_t prepared_full_code_owner_modules{}, prepared_full_code_owner_functions{}, prepared_full_code_effective_bodies{};\n"
         "        bool runtime_full_code_owners_transferred_privately{}, runtime_full_code_targets_allocation_preserved{}; // Same genuine allocations; no LIVE seal.")
    edit(repo, root + "uwvm_runtime.h",
         "        bool full_publication_records_rechecked_after_physical_join{}; // Same actual private record/engine roster.",
         "        bool full_publication_records_rechecked_after_physical_join{}; // Same actual private record/engine roster.\n"
         "        bool full_code_owners_rechecked_after_physical_join{}; // Actual final native owners, still private.")
    edit(repo, root + "uwvm_runtime_checkpoint_staged_llvm_full_engine.h",
         "        ::uwvm2::utils::container::delete_owned_ptr<details::pending_llvm_jit_code_ranges> ranges_{};",
         "        ::uwvm2::utils::container::delete_owned_ptr<details::pending_llvm_jit_code_ranges> ranges_{};\n"
         "        // Comparison receipts minted only by the private world handoff.\n"
         "        // The FINAL full-code record supplies ownership, never these pointers.\n"
         "        ::llvm::ExecutionEngine const* handed_off_engine_{};\n"
         "        ::llvm::LLVMContext const* handed_off_context_{};\n"
         "        ::std::uintptr_t const* handed_off_targets_{};")
    edit(repo, root + "uwvm_runtime_native_owner_loaded_endpoints.h",
         "namespace uwvm2::runtime::lib { extern \"C++\" { class runtime_checkpoint_staged_llvm_full_engine; } }",
         "namespace uwvm2::runtime::lib { extern \"C++\" { class runtime_checkpoint_staged_llvm_full_engine; class runtime_checkpoint_world_transaction; } }")
    edit(repo, root + "uwvm_runtime_native_owner_loaded_endpoints.h",
         "        friend class ::uwvm2::runtime::lib::runtime_checkpoint_staged_llvm_full_engine;",
         "        friend class ::uwvm2::runtime::lib::runtime_checkpoint_staged_llvm_full_engine;\n"
         "        friend class ::uwvm2::runtime::lib::runtime_checkpoint_world_transaction;")
    edit(repo, root + "uwvm_runtime_pending_code_ranges.h",
         "namespace uwvm2::runtime::lib::details",
         "namespace uwvm2::runtime::lib { extern \"C++\" { class runtime_checkpoint_world_transaction; } }\nnamespace uwvm2::runtime::lib::details")
    edit(repo, root + "uwvm_runtime_pending_code_ranges.h",
         "        struct range { ::std::uintptr_t begin{}, size{}; };",
         "        friend class ::uwvm2::runtime::lib::runtime_checkpoint_world_transaction;\n"
         "        struct range { ::std::uintptr_t begin{}, size{}; };\n"
         "        ::llvm::ExecutionEngine const* private_handoff_engine_{};")
    edit(repo, root + "uwvm_runtime_pending_code_ranges.h",
         "    public:\n#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF)",
         "        [[nodiscard]] bool can_handoff_private_owner(::llvm::ExecutionEngine const& actual) const noexcept\n"
         "        { return engine==::std::addressof(actual) && !actual.hasError() && !private_handoff_engine_ &&\n"
         "                 debug_full_capture_ && !committed_ && !observation_failed_; }\n"
         "        void handoff_private_owner(::llvm::ExecutionEngine const& actual) noexcept\n"
         "        {\n"
         "            if(!can_handoff_private_owner(actual)) { ::fast_io::fast_terminate(); }\n"
         "            private_handoff_engine_=::std::addressof(actual);detach();\n"
         "        }\n"
         "        [[nodiscard]] bool private_handoff_matches(::llvm::ExecutionEngine const& actual) const noexcept\n"
         "        { return private_handoff_engine_==::std::addressof(actual) && engine==nullptr &&\n"
         "                 !actual.hasError() && debug_full_capture_ && !committed_ && !observation_failed_; }\n\n"
         "    public:\n#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF)")
    edit(repo, root + "uwvm_runtime_checkpoint_world_resources.h",
         '# include "uwvm_runtime_checkpoint_world_publication_records.h"',
         '# include "uwvm_runtime_checkpoint_world_publication_records.h"\n# include "uwvm_runtime_checkpoint_world_full_code_handoff.h"')
    edit(repo, root + "uwvm_runtime_checkpoint_world_publication_records.h",
         "[[nodiscard]] bool preflight_private_full_publication_records() const noexcept\n{",
         "[[nodiscard]] bool preflight_private_full_publication_records() const noexcept\n{\n"
         "    if(full_code_handoff_complete_) { return preflight_private_handed_off_full_code(); }")
    edit(repo, root + "uwvm_runtime_checkpoint_world_preparation_api.h",
         "            out.status=llvm_jit_checkpoint_prepare_status::prepared_and_discarded;\n            out.wasip1_environments=environments.size();",
         "            out.status=llvm_jit_checkpoint_prepare_status::full_code_owner_handoff_declined;\n"
         "            if(!prepared->handoff_private_full_code_owners(out))\n"
         "            { out.resource_diagnostic=static_cast<unsigned>(prepared->phase_);return; }\n"
         "            out.status=llvm_jit_checkpoint_prepare_status::prepared_and_discarded;\n            out.wasip1_environments=environments.size();")
    edit(repo, root + "uwvm_runtime_checkpoint_native_retirement_state.h",
         "        actual.result.full_publication_records_rechecked_after_physical_join=true;",
         "        actual.result.full_publication_records_rechecked_after_physical_join=true;\n"
         "        actual.result.full_code_owners_rechecked_after_physical_join=true;")
    for name in ("debug_checkpoint_prepared_retirement_runtime.cc", "debug_wasip1_prepared_retirement_runtime.cc",
                 "debug_checkpoint_complete_instance_runtime.cc", "debug_checkpoint_complete_preload_runtime.cc"):
        relative = "test/0017.runtime/" + name
        if "complete_" in name:
            edit(repo, relative,
                 "                        prepared.runtime_full_publication_records_prepared &&",
                 "                        prepared.runtime_full_code_owners_transferred_privately && prepared.runtime_full_code_targets_allocation_preserved &&\n"
                 "                        prepared.prepared_full_code_owner_modules==prepared.modules && prepared.prepared_full_code_owner_functions==prepared.functions &&\n"
                 "                        prepared.runtime_full_publication_records_prepared &&")
        else:
            edit(repo, relative,
                 "pending.preparation.runtime_full_publication_records_prepared &&",
                 "pending.preparation.runtime_full_code_owners_transferred_privately && pending.preparation.runtime_full_code_targets_allocation_preserved &&\n"
                 "        pending.preparation.prepared_full_code_owner_modules==pending.preparation.modules && pending.preparation.prepared_full_code_owner_functions==pending.preparation.functions &&\n"
                 "        !pending.full_code_owners_rechecked_after_physical_join && pending.preparation.runtime_full_publication_records_prepared &&")
            who = "joined" if "debug_checkpoint_prepared_" in name else "ready"
            edit(repo, relative,
                 who + ".full_publication_records_rechecked_after_physical_join &&" if who=="joined" else who + '.full_publication_records_rechecked_after_physical_join,"',
                 who + ".full_publication_records_rechecked_after_physical_join && " + who + ".full_code_owners_rechecked_after_physical_join &&" if who=="joined" else
                 who + ".full_publication_records_rechecked_after_physical_join && " + who + '.full_code_owners_rechecked_after_physical_join,"')
    key = repo + "/" + root + "uwvm_runtime_checkpoint_world_full_code_handoff.h"
    assert not (B / key).exists()
    changes[key] = (L / "full_code_handoff.fragment.h").read_text()

with (L / "before.json").open("x") as f:
    json.dump({k: sha(v) for k, v in before.items()}, f, indent=2)
    f.write("\n")
for key, data in before.items():
    destination = L / "before" / key
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("xb") as f:
        f.write(data)
        f.flush()
        os.fsync(f.fileno())
with (L / "edit-plan.json").open("x") as f:
    json.dump(changes, f)
    f.flush()
    os.fsync(f.fileno())
for key, content in changes.items():
    destination = B / key
    if key in before:
        assert destination.read_bytes() == before[key], key
        mode = stat.S_IMODE(destination.stat().st_mode)
    else:
        assert not destination.exists()
        mode = 0o644
    temporary = destination.with_name(destination.name + ".r40-owned-tmp")
    with temporary.open("x") as f:
        f.write(content)
        f.flush()
        os.fsync(f.fileno())
    os.chmod(temporary, mode)
    if key in before:
        assert destination.read_bytes() == before[key], key
    os.replace(temporary, destination)
patch = "".join("".join(difflib.unified_diff(before.get(k, b"").decode().splitlines(True), v.splitlines(True),
    fromfile="before/"+k, tofile="after/"+k)) for k, v in sorted(changes.items()))
(L / "owned-source-changes.patch").write_text(patch)
print("owned files changed", len(changes), "before files retained", len(before))
