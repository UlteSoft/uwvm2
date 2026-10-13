// Real parser/init -> fused LLVM SSA -> owned bitcode -> claimed-cohort
// consumption -> production grouped MCJIT. The post-init alias rebinding is
// explicit cold fixture setup; it is NOT guest mutation or initializer proof.
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/compiler/llvm_jit/compile_cu_from_lazy_validator/impl.h>
#include <array>
#include <atomic>
#if !defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
#error "Use the actual matching LLVM/runtime provider closure."
#endif
namespace
{
    using namespace uwvm2test::uwvm_int_strict;
    namespace lazy = ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator;
    namespace full = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
    namespace runtime = ::uwvm2::uwvm::runtime::storage;
    namespace thread = ::uwvm2::utils::thread;
    constexpr char key[]{"uwvm.checked.lazy.unwind.import.route.v1"};
    void require(bool value, char const* message)
    {
        if(!value)
        {
            ::fast_io::print(::fast_io::err(), "checked unwind import route: ", ::fast_io::mnp::os_c_str(message), "\n");
            ::fast_io::fast_terminate();
        }
    }
    byte_vec read(char const* name)
    {
        ::fast_io::native_file_loader input{::fast_io::mnp::os_c_str(name), ::fast_io::open_mode::in};
        require(input.size() >= 8uz && input.size() <= 65536uz, "actual source extent");
        byte_vec result(input.size());
        // [actual owned file input / equally sized retained source] exact extents
        // [safe] 8..65536 bounds BEFORE copy, neither pointer advances.
        ::fast_io::freestanding::my_memcpy(result.data(), input.data(), input.size());
        return result;
    }
    struct route_count { ::std::size_t present{}, selected{}; };
    route_count count_routes(::llvm::Module const& module)
    {
        route_count count{};
        for(auto const& function: module)
        {
            for(auto const& block: function)
            {
                auto const terminal{block.empty() || !block.back().isTerminator() ? nullptr : ::std::addressof(block.back())};
                if(terminal == nullptr || terminal->getMetadata(key) == nullptr) { continue; }
                auto const branch{::llvm::dyn_cast<::llvm::BranchInst>(terminal)};
                require(branch != nullptr && branch->isConditional(), "actual tagged branch");
                auto const condition{::llvm::dyn_cast<::llvm::ConstantInt>(branch->getCondition())};
                require(condition != nullptr && condition->getType()->isIntegerTy(1u), "actual constant route condition");
                ++count.present; count.selected += !condition->isZero();
            }
        }
        return count;
    }
    struct publication
    {
        lazy::lazy_module_storage_t* storage{};
        ::std::array<::std::uintptr_t, 2uz>* typed{};
        static void commit(void* context, ::std::size_t index) noexcept
        {
            auto& self{*static_cast<publication*>(context)};
            require(index < self.typed->size() && index < self.storage->materialized_functions.size(), "actual publication index");
            auto const entry{self.storage->materialized_functions.index_unchecked(index).entry_address};
            require(entry != 0u, "actual prepared native entry");
            ::std::atomic_ref<::std::uintptr_t>{self.typed->at(index)}.store(entry, ::std::memory_order_release);
        }
    };
    void verify_routes(char const* module_path, char const* provider_path)
    {
        auto source{read(module_path)}; auto provider{read(provider_path)};
        auto policy{make_wasm1p1_feature_parameter()};
        auto& core{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(policy)};
        core.disable_memory64 = false;
        auto prepared{prepare_runtime_from_wasm(source, u8"checked-route-consumer",
            {preloaded_wasm_module{.wasm_bytes = &provider, .module_name = u8"checked-route-provider"}}, policy)};
        require(prepared.mod != nullptr && prepared.mod->imported_function_vec_storage.size() == 1uz &&
            prepared.mod->local_defined_function_vec_storage.size() == 2uz, "actual import/local source shape");
        auto& alias{const_cast<runtime::wasm_module_storage_t*>(prepared.mod)->imported_function_vec_storage.index_unchecked(0uz)};
        auto const foreign{full::details::resolve_runtime_direct_callee(*prepared.mod, 0u)};
        require(foreign.state_valid && !foreign.direct_callable, "actual initialized foreign import is not optimized as local");
        // Explicit test-only cold alias setup after genuine initialization. Both
        // descriptors retain exactly the original i64->i64 declaration signature.
        alias.link_kind = runtime::imported_function_link_kind::defined;
        alias.target.defined_ptr = const_cast<runtime::local_defined_function_storage_t*>(
            &prepared.mod->local_defined_function_vec_storage.index_unchecked(0uz));
        auto const resolved{full::details::resolve_runtime_direct_callee(*prepared.mod, 0u)};
        require(resolved.state_valid && resolved.direct_callable && resolved.func_index == 1u, "actual local alias membership");
        ::std::array<runtime::llvm_jit_raw_call_target_t, 2uz> raw{};
        ::std::array<::std::uintptr_t, 2uz> typed{};
        full::compile_option options{}; options.validator_feature_parameter = &policy;
        options.compilation_mode = full::llvm_jit_compilation_mode::tiered;
        options.emit_call_stack_frames = false; options.emit_unwind_call_stack_frames = true;
        options.emit_precise_gc_root_frames = false; options.route_wasm_calls_through_runtime_bridge = true;
        options.lazy_defined_targets_are_atomic = true;
        options.lazy_defined_raw_call_target_base_address = reinterpret_cast<::std::uintptr_t>(raw.data());
        options.lazy_defined_raw_call_target_count = raw.size();
        options.lazy_defined_typed_entry_target_base_address = reinterpret_cast<::std::uintptr_t>(typed.data());
        options.lazy_defined_typed_entry_target_count = typed.size();
        ::uwvm2::validation::error::code_validation_error_impl error{};
        auto admitted{lazy::initialize_lazy_module_storage(*prepared.mod, options, error)};
        auto const original_plan{admitted.checked_ir_plan.get()};
        auto storage{::std::move(admitted)};
        require(admitted.checked_ir_plan == nullptr && storage.checked_ir_plan.get() == original_plan,
            "genuine moved storage retains the same private allocation, no stack-address identity");
        auto const& plan{storage.checked_ir_plan};
        require(plan && plan->admission_available() && plan->native_ir_available(0uz) && plan->native_ir_available(1uz),
            "actual complete typed admission and original verified native fragments");
        auto wrong_world{options}; wrong_world.compiler_registry = &runtime::active_runtime_registry();
        require(!plan->matches_emission_options(wrong_world),
            "actual full-only registry borrow cannot match retained lazy policy");
        ::std::array<::std::size_t, 2uz> all{0uz, 1uz};
        full::llvm_jit_module_storage_t merged{};
        require(plan->materialize_checked_ir(*prepared.mod, options, all, merged), "actual owned compiler bitcode merge");
        auto const original{count_routes(*merged.llvm_module)};
        require(original.present == 1uz && original.selected == 0uz, "real fused import CFG initially selects original bridge");
        require(!plan->select_complete_cohort_unwind_import_routes(*prepared.mod, storage, all, merged) &&
            count_routes(*merged.llvm_module).selected == 0uz, "unclaimed cohort refuses before mutation");
        for(auto& function: storage.functions)
        {
            function.materialization_state.state.store(thread::lazy_compile_state::compiling, ::std::memory_order_release);
            require(function.primary_cu_index < storage.compile_units.size(), "actual claim CU bound");
            storage.compile_units.index_unchecked(function.primary_cu_index).state.state.store(thread::lazy_compile_state::compiling,
                ::std::memory_order_release);
        }
        require(plan->select_complete_cohort_unwind_import_routes(*prepared.mod, storage, all, merged) &&
            count_routes(*merged.llvm_module).selected == 1uz && !::llvm::verifyModule(*merged.llvm_module),
            "actual claimed all-local cohort switches real typed SSA branch and verifies");
        ::std::array<::std::size_t, 1uz> only_caller{1uz};
        full::llvm_jit_module_storage_t partial{};
        require(plan->materialize_checked_ir(*prepared.mod, options, only_caller, partial) &&
            plan->select_complete_cohort_unwind_import_routes(*prepared.mod, storage, only_caller, partial) &&
            count_routes(*partial.llvm_module).present == 1uz && count_routes(*partial.llvm_module).selected == 0uz,
            "missing actual target claim retains original raw route");
        for(auto& function: storage.functions)
        {
            function.materialization_state.state.store(thread::lazy_compile_state::uncompiled, ::std::memory_order_release);
            storage.compile_units.index_unchecked(function.primary_cu_index).state.state.store(thread::lazy_compile_state::uncompiled,
                ::std::memory_order_release);
        }
        lazy::lazy_compile_options demand{}; demand.compile_options = options; demand.validator_feature_parameter = &policy;
        publication published{&storage, &typed};
        lazy::details::compile_lazy_local_function_group(*prepared.mod, storage, demand, 1uz, error,
            &publication::commit, &published);
        auto const& answer{storage.materialized_functions.index_unchecked(1uz)};
        require(lazy::details::load_lazy_materialized_ready(answer, ::std::memory_order_acquire) &&
            answer.entry_address != 0u && typed[0uz] != 0u && typed[1uz] != 0u, "real grouped MCJIT and target-slot publication");
        using entry = ::std::int64_t (UWVM2TEST_WASM_ABI*)();
        auto const execute{reinterpret_cast<entry>(answer.entry_address)};
        require(execute() == 42, "real staged typed import alternative returns 42, foreign fallback would return99");
        ::fast_io::print(::fast_io::out(), "CHECKED_UNWIND_IMPORT claims=real typed_route=1 actual_result=42 raw_body_rewalk=0\n");
    }
}
int main(int argc, char const* const* argv)
{
    require(argc == 3, "module.wasm provider.wasm required");
    verify_routes(argv[1], argv[2]);
}
