// Genuine fused admission + retained LLVM bitcode + production MCJIT path.
// Deliberately unqualified native-EH target causes a REAL original lowering
// decline; no synthetic state transition or Wasm pure-validation prepass.
// Ordinary-only lazy implementation; ROS has only its actual full backends.
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/compiler/llvm_jit/compile_cu_from_lazy_validator/impl.h>
#include <array>
#include <atomic>
#include <limits>
#if !defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
#error "Compile this genuine lazy LLVM component with the actual LLVM runtime closure."
#endif
namespace
{
    using namespace uwvm2test::uwvm_int_strict;
    namespace lazy = ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator;
    namespace full = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
    namespace thread = ::uwvm2::utils::thread;
    using validation_error = ::uwvm2::validation::error::code_validation_error_impl;
    void require(bool valid, char const* message)
    {
        if(!valid)
        {
            ::fast_io::print(::fast_io::err(), "lazy native decline: ", ::fast_io::mnp::os_c_str(message), "\n");
            ::fast_io::fast_terminate();
        }
    }
    auto all_features()
    {
        auto features{make_wasm1p1_feature_parameter()};
        auto& policy{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features)};
        policy.disable_gc = false; policy.disable_function_references = false;
        policy.disable_exceptions = false; policy.disable_tail_call = false;
        policy.disable_memory64 = false;
        return features;
    }
    byte_vec read_source(char const* path)
    {
        ::fast_io::native_file_loader input{::fast_io::mnp::os_c_str(path), ::fast_io::open_mode::in};
        require(input.size() >= 8uz && input.size() <= 65536uz, "actual source file bounds");
        byte_vec result(input.size());
        // [actual owned input / exactly sized immutable-source copy] equal extents
        // [safe] proven 8..65536 bytes BEFORE copy; no cursor advances.
        ::fast_io::freestanding::my_memcpy(result.data(), input.data(), input.size());
        return result;
    }
    full::compile_option options_for(wasm_feature_parameter_t const& features, bool unwind)
    {
        full::compile_option options{};
        options.validator_feature_parameter = &features;
        options.compilation_mode = full::llvm_jit_compilation_mode::lazy;
        options.verify_llvm_jit_ir = true;
        options.emit_unwind_call_stack_frames = unwind;
        options.emit_call_stack_frames = false;
        options.emit_precise_gc_root_frames = false;
        // This is actual target ABI availability, never a syntax feature flag.
        // The real original emitter declines reachable throw with this policy.
        options.native_exception_target_machine = nullptr;
        return options;
    }
    void valid_unused_decline(char const* path)
    {
        auto source{read_source(path)}; auto features{all_features()};
        auto prepared{prepare_runtime_from_wasm(source, u8"native-decline-valid", {}, features)};
        require(prepared.mod != nullptr && prepared.mod->local_defined_function_vec_storage.size() == 2uz,
            "actual two-function module identity");
        for(bool const unwind: {false, true})
        {
            validation_error error{}; auto options{options_for(features, unwind)};
            auto storage{lazy::initialize_lazy_module_storage(*prepared.mod, options, error)};
            require(error.err_code == ::uwvm2::validation::error::code_validation_error_code::ok,
                "legal unused reachable throw stays valid");
            auto const& plan{storage.checked_ir_plan};
            require(plan && plan->admission_available() && !plan->resource_exhausted() &&
                plan->matches_source(*prepared.mod) && plan->matches_emission_options(options),
                "actual sealed complete typed admission, no quota waiver");
            require(!plan->native_ir_available(0uz) && plan->native_ir_available(1uz) &&
                !plan->native_ir_available(2uz) && !plan->native_ir_available(SIZE_MAX),
                "native availability per actual owned bitcode record and index bounds");
            full::llvm_jit_compiler_first_decline decline{};
            require(plan->native_lowering_decline(0uz, decline) && decline.function_index == 0uz &&
                decline.stage == full::llvm_jit_compiler_decline_stage::instruction_or_function_finish &&
                decline.primary_opcode == 0x08u && !plan->native_lowering_decline(1uz, decline),
                "real original throw-emitter decline retained separately from validity");
            auto& failed{storage.functions.index_unchecked(0uz)};
            auto const failed_cu{failed.primary_cu_index};
            require(failed_cu < storage.compile_units.size() &&
                failed.materialization_state.state.load(::std::memory_order_acquire) == thread::lazy_compile_state::failed &&
                storage.compile_units.index_unchecked(failed_cu).state.state.load(::std::memory_order_acquire) == thread::lazy_compile_state::failed,
                "native-unavailable function/CU failed before scheduler publication");
            auto& unavailable{storage.materialized_functions.index_unchecked(0uz)};
            require(!lazy::details::load_lazy_materialized_ready(unavailable, ::std::memory_order_acquire) &&
                unavailable.entry_address == 0u && unavailable.raw_entry_address == 0u && !unavailable.llvm_jit_engine,
                "declined body never receives a native entry or owner");
            std::array<std::size_t, 1uz> declined_selection{0uz};
            full::llvm_jit_module_storage_t rejected{};
            require(!plan->materialize_checked_ir(*prepared.mod, options, declined_selection, rejected) &&
                !rejected.llvm_module && !rejected.llvm_context_holder,
                "declined selection rejects before allocating merged IR, never rewalks Wasm");
            lazy::lazy_compile_options demand{}; demand.compile_options = options;
            demand.validator_feature_parameter = &features;
            lazy::details::compile_lazy_local_function_group(*prepared.mod, storage, demand, 0uz, error);
            require(failed.materialization_state.state.load(::std::memory_order_acquire) == thread::lazy_compile_state::failed &&
                !unavailable.llvm_jit_engine && error.err_code == ::uwvm2::validation::error::code_validation_error_code::ok,
                "native decline demand stays separate, no synthetic interpreter fallback");
            auto const entry_cu{storage.functions.index_unchecked(1uz).primary_cu_index};
            require(entry_cu < storage.compile_units.size(), "actual entry compile unit bound");
            // This invokes the real production grouped bitcode/codegen path.
            // Unwind includes the declined direct callee in its typed cohort;
            // the preclaim availability filter must skip it, not fail entry.
            lazy::compile_cu_from_lazy_validator(*prepared.mod, storage, demand, entry_cu, error);
            auto const& entry{storage.materialized_functions.index_unchecked(1uz)};
            require(lazy::details::load_lazy_materialized_ready(entry, ::std::memory_order_acquire) &&
                entry.entry_address != 0u && entry.raw_entry_address != 0u && entry.llvm_jit_engine,
                "actual retained IR materializes and owns numeric native entry");
            using entry_type = ::std::int32_t (UWVM2TEST_WASM_ABI*)();
            auto const call{reinterpret_cast<entry_type>(entry.entry_address)};
            require(call() == 42 && failed.materialization_state.state.load(::std::memory_order_acquire) == thread::lazy_compile_state::failed &&
                !unavailable.llvm_jit_engine && !lazy::details::load_lazy_materialized_ready(unavailable, ::std::memory_order_acquire),
                "real machine execution returns 42 without executing/materializing declined body");
            ::fast_io::print(::fast_io::out(), "LAZY_NATIVE_DECLINE typed_valid=1 native_unused_failed=1 retained_entry_result=42 unwind=",
                ::fast_io::mnp::dec(unwind), " synthetic_fallback=0\n");
        }
    }
    void invalid_after_decline(char const* path)
    {
        auto source{read_source(path)}; auto features{all_features()};
        auto prepared{prepare_runtime_from_wasm(source, u8"native-decline-invalid", {}, features)};
        require(prepared.mod != nullptr && prepared.mod->local_defined_function_vec_storage.size() == 3uz,
            "actual invalid-after-decline module identity");
        validation_error error{}; auto options{options_for(features, false)}; bool refused{};
        try
        {
            auto storage{lazy::initialize_lazy_module_storage(*prepared.mod, options, error)};
            (void)storage;
        }
        catch(::fast_io::error const& caught)
        {
            if(caught.domain != ::fast_io::parse_domain_value || caught.code !=
                static_cast<::std::size_t>(static_cast<char8_t>(::fast_io::parse_code::invalid))) { throw; }
            auto const& invalid{prepared.mod->local_defined_function_vec_storage.index_unchecked(1uz)};
            require(invalid.wasm_code_ptr != nullptr, "actual invalid code owner");
            // [actual immutable second expression begin ... end] one-past
            // [safe] integer bounds ONLY; diagnostic pointer is never read/advanced.
            auto const begin{reinterpret_cast<::std::uintptr_t>(invalid.wasm_code_ptr->body.expr_begin)};
            auto const end{reinterpret_cast<::std::uintptr_t>(invalid.wasm_code_ptr->body.code_end)};
            auto const point{reinterpret_cast<::std::uintptr_t>(error.err_curr)};
            refused = error.err_code != ::uwvm2::validation::error::code_validation_error_code::ok &&
                begin != 0u && end > begin && point >= begin && point < end;
        }
        require(refused, "earlier native decline cannot skip later invalid unused body checks");
        ::fast_io::print(::fast_io::out(), "LAZY_NATIVE_DECLINE later_unused_invalid_refusal=1 guest_execution=0\n");
    }
}
int main(int argc, char const* const* argv)
{
    require(argc == 3, "exact valid and invalid-after-native-decline Wasm inputs");
    valid_unused_decline(argv[1]); invalid_after_decline(argv[2]);
}
