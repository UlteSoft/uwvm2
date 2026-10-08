#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>

// Native metadata regression only. These actual registry/vector records do not
// claim an initialized source, valid Wasm body, executable entry or epoch.
int main()
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    namespace compiler = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
    namespace details = compiler::details;
    namespace checkpoint = ::uwvm2::runtime::checkpoint;
    storage::runtime_registry_type private_world{}, unrelated_world{};
    private_world.reserve(3uz);
    private_world.try_emplace(::uwvm2::utils::container::u8string_view{u8"consumer"}, storage::wasm_module_storage_t{});
    private_world.try_emplace(::uwvm2::utils::container::u8string_view{u8"relay"}, storage::wasm_module_storage_t{});
    private_world.try_emplace(::uwvm2::utils::container::u8string_view{u8"provider"}, storage::wasm_module_storage_t{});
    auto& consumer{private_world.find(::uwvm2::utils::container::u8string_view{u8"consumer"})->second};
    auto& relay{private_world.find(::uwvm2::utils::container::u8string_view{u8"relay"})->second};
    auto& provider{private_world.find(::uwvm2::utils::container::u8string_view{u8"provider"})->second};
    consumer.imported_function_vec_storage.resize(1uz);
    relay.imported_function_vec_storage.resize(8uz);
    provider.local_defined_function_vec_storage.resize(1uz);
    storage::wasm_binfmt1_final_function_type_t signature{};
    storage::wasm_binfmt1_final_wasm_code_t code{};
    auto& actual_function{provider.local_defined_function_vec_storage.index_unchecked(0uz)};
    actual_function.function_type_ptr = ::std::addressof(signature);
    actual_function.wasm_code_ptr = ::std::addressof(code);
    auto& first{consumer.imported_function_vec_storage.index_unchecked(0uz)};
    first.link_kind = storage::imported_function_link_kind::imported;
    first.target.imported_ptr = relay.imported_function_vec_storage.data();
    for(::std::size_t index{}; index != relay.imported_function_vec_storage.size(); ++index)
    {
        // [exact allocated relay records0..7] end
        // [safe] current index<count and next index<count BEFORE pointer GEP.
        auto& item{relay.imported_function_vec_storage.index_unchecked(index)};
        if(index + 1uz < relay.imported_function_vec_storage.size())
        {
            item.link_kind = storage::imported_function_link_kind::imported;
            item.target.imported_ptr = relay.imported_function_vec_storage.data() + index + 1uz;
        }
        else
        {
            item.link_kind = storage::imported_function_link_kind::defined;
            item.target.defined_ptr = ::std::addressof(actual_function);
        }
    }
    auto*& selected{storage::details::selected_runtime_registry};
    struct restore_selector
    {
        storage::runtime_registry_type*& selected;
        storage::runtime_registry_type* old;
        ~restore_selector() { selected=old; }
    } restore{selected, selected};
    // The actual active registry intentionally contains none of this world's
    // records. No initialized source or global source owner is forged/swapped.
    selected = ::std::addressof(unrelated_world);
    if(details::get_runtime_imported_function_link_walk_bound(consumer) != 1uz ||
       details::get_runtime_imported_function_link_walk_bound(consumer, ::std::addressof(private_world)) != 9uz)
    { return 1; }
    auto const ordinary{details::resolve_runtime_direct_callee(consumer, 0u)};
    if(ordinary.state_valid) { return 2; } // unchanged ordinary bound rejects this unrelated long chain
    auto const explicit_result{details::resolve_runtime_direct_callee(consumer, 0u, ::std::addressof(private_world))};
    if(!explicit_result.state_valid || explicit_result.direct_callable || explicit_result.function_type_ptr != ::std::addressof(signature))
    { return 3; }
    if(details::resolve_runtime_direct_callee(consumer, 0u, ::std::addressof(unrelated_world)).state_valid)
    { return 4; }

    compiler::local_func_storage_t local{};
    local.runtime_module_ptr = ::std::addressof(consumer);
    local.compiler_registry = ::std::addressof(private_world);
    checkpoint::function_plan plan{};
    plan.profile = checkpoint::compilation_profile::create_for_trusted_manager();
    if(!plan.profile) { return 5; }
    details::runtime_local_func_llvm_jit_emit_state_t state{};
    state.local_func_storage_ptr = ::std::addressof(local);
    state.checkpoint_plan = ::std::addressof(plan);
    if(!details::runtime_local_func_llvm_jit_checkpoint_import_has_defined_owner(state, 0u)) { return 6; }
    local.compiler_registry = nullptr;
    if(details::runtime_local_func_llvm_jit_checkpoint_import_has_defined_owner(state, 0u)) { return 7; }
    local.compiler_registry = ::std::addressof(private_world);

    // [allocated relay vector] end
    // [safe] ordinal7<count BEFORE mutation; its next cursor is a real element
    // of this private world, but the finite registry-bound walk must reject it.
    auto& last{relay.imported_function_vec_storage.index_unchecked(7uz)};
    last.link_kind = storage::imported_function_link_kind::imported;
    last.target.imported_ptr = ::std::addressof(first);
    if(details::resolve_runtime_direct_callee(consumer, 0u, ::std::addressof(private_world)).state_valid ||
       details::runtime_local_func_llvm_jit_checkpoint_import_has_defined_owner(state, 0u)) { return 8; }

    // An actual foreign allocation is comparison-only: reject before record read.
    storage::imported_function_storage_t foreign_import{};
    last.target.imported_ptr = ::std::addressof(foreign_import);
    if(details::resolve_runtime_direct_callee(consumer, 0u, ::std::addressof(private_world)).state_valid ||
       details::runtime_local_func_llvm_jit_checkpoint_import_has_defined_owner(state, 0u)) { return 9; }
    auto const interior{reinterpret_cast<storage::imported_function_storage_t const*>(
        reinterpret_cast<::std::uintptr_t>(relay.imported_function_vec_storage.data()) + 1u)};
    last.target.imported_ptr = interior;
    if(details::resolve_runtime_direct_callee(consumer, 0u, ::std::addressof(private_world)).state_valid ||
       details::runtime_local_func_llvm_jit_checkpoint_import_has_defined_owner(state, 0u)) { return 10; }
    last.link_kind = storage::imported_function_link_kind::defined;
    storage::local_defined_function_storage_t foreign_defined{};
    last.target.defined_ptr = ::std::addressof(foreign_defined);
    if(details::resolve_runtime_direct_callee(consumer, 0u, ::std::addressof(private_world)).state_valid ||
       details::runtime_local_func_llvm_jit_checkpoint_import_has_defined_owner(state, 0u)) { return 11; }
    last.target.defined_ptr = ::std::addressof(actual_function);
    plan.profile = checkpoint::compilation_profile::create_for_trusted_observer();
    if(details::runtime_local_func_llvm_jit_checkpoint_import_has_defined_owner(state, 0u)) { return 12; }
    if(selected != ::std::addressof(unrelated_world)) { return 13; }
    return 0;
}
