// Actual parser -> original fused admission -> owned compiler bitcode ->
// whole-symbol projection -> real MCJIT execution. No pure prevalidation,
// raw Wasm replay, fake binding/publication or closed T2 performance claim.
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/compiler/llvm_jit/compile_cu_from_lazy_validator/impl.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <uwvm2/runtime/lib/uwvm_runtime_pending_code_ranges.h>
#include <uwvm2/runtime/lib/uwvm_runtime_native_function_address.h>
#if !defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
#error "Use the real matching LLVM/runtime provider closure."
#endif
namespace
{
    using namespace uwvm2test::uwvm_int_strict;
    namespace full = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
    namespace lazy = ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator;
    void require(bool valid, char const* message)
    {
        if(!valid)
        {
            ::fast_io::print(::fast_io::err(), "checked whole IR owner: ", ::fast_io::mnp::os_c_str(message), "\n");
            ::fast_io::fast_terminate();
        }
    }
    byte_vec source_file(char const* path)
    {
        ::fast_io::native_file_loader input{::fast_io::mnp::os_c_str(path), ::fast_io::open_mode::in};
        require(input.size() >= 8uz && input.size() <= 65536uz, "actual complete source file extent");
        byte_vec result(input.size());
        // [RAII file input / equal owned immutable source] exact equal extents
        // [safe] previous source bound BEFORE copy; no pointer advances.
        ::fast_io::freestanding::my_memcpy(result.data(), input.data(), input.size());
        return result;
    }
    auto features()
    {
        auto result{make_wasm1p1_feature_parameter()};
        auto& policy{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(result)};
        policy.disable_gc = false; policy.disable_function_references = false;
        policy.disable_exceptions = false; policy.disable_tail_call = false; policy.disable_memory64 = false;
        return result;
    }
    void consume_and_execute(char const* path)
    {
        auto source{source_file(path)}; auto policy{features()};
        auto prepared{prepare_runtime_from_wasm(source, u8"checked-whole-owner", {}, policy)};
        require(prepared.mod && prepared.mod->local_defined_function_vec_storage.size() == 2uz, "actual two-function source identity");
        full::compile_option options{}; options.validator_feature_parameter = &policy;
        options.compilation_mode = full::llvm_jit_compilation_mode::tiered;
        options.emit_call_stack_frames = false; options.emit_unwind_call_stack_frames = false;
        options.emit_precise_gc_root_frames = false; options.emit_tiered_loop_reentry_entries = true;
        ::uwvm2::validation::error::code_validation_error_impl error{};
        auto storage{lazy::initialize_lazy_module_storage(*prepared.mod, options, error)};
        require(storage.checked_ir_plan && storage.checked_ir_plan->admission_available() &&
            storage.checked_ir_plan->native_ir_available(0uz) && storage.checked_ir_plan->native_ir_available(1uz),
            "actual source checked and both native fragments available");
        auto const& observed{storage.materialized_functions.index_unchecked(0uz).local_func};
        require(!observed.tiered_loop_reentries.empty(), "genuine same-walk loop metadata exists");
        auto const observed_loop{observed.tiered_loop_reentries.front_unchecked()};
        auto const observed_begin{observed.code_begin}; auto const observed_end{observed.code_end};
        // Deliberately alter an external scheduler metadata projection. A whole
        // artifact must copy the factory's immutable exact metadata, not use this
        // public mutable record as the source of an emission/entry permit.
        auto& external{storage.materialized_functions.index_unchecked(0uz).local_func};
        external.module_id = SIZE_MAX;
        external.tiered_loop_reentries.front_unchecked().wasm_code_offset = SIZE_MAX;
        full::full_function_symbol_t whole{};
        require(storage.checked_ir_plan->consume_whole_checked_symbol(*prepared.mod, whole) &&
            whole.local_funcs.size() == 2uz && whole.llvm_jit_module.emitted &&
            whole.llvm_jit_module.llvm_module && whole.llvm_jit_module.llvm_context_holder,
            "genuine all-owned-bitcode full-shaped symbol and LLVM owners");
        auto const& restored{whole.local_funcs.index_unchecked(0uz)};
        require(restored.module_id == options.curr_wasm_id && restored.function_index == 0uz &&
            restored.runtime_module_ptr == prepared.mod && restored.code_begin == observed_begin && restored.code_end == observed_end &&
            restored.tiered_loop_reentries.front_unchecked().wasm_code_offset == observed_loop.wasm_code_offset &&
            restored.tiered_loop_reentries.front_unchecked().entry_id == observed_loop.entry_id,
            "exact factory-minted metadata remains independent of modified scheduler record");
        auto const old_owner{whole.llvm_jit_module.llvm_module.get()};
        require(!storage.checked_ir_plan->consume_whole_checked_symbol(*prepared.mod, whole) &&
            whole.llvm_jit_module.llvm_module.get() == old_owner && whole.local_funcs.size() == 2uz,
            "nonempty caller output is not replaced or silently retired");
        require(lazy::details::ensure_llvm_jit_native_target_initialized(), "actual process target");
        ::std::unique_ptr<::llvm::TargetMachine> target{::llvm::EngineBuilder{}.selectTarget()};
        require(bool(target), "actual target owner");
        auto& module{*whole.llvm_jit_module.llvm_module};
#if LLVM_VERSION_MAJOR >= 21
        module.setTargetTriple(target->getTargetTriple());
#else
        module.setTargetTriple(target->getTargetTriple().str());
#endif
        module.setDataLayout(target->createDataLayout());
        require(full::details::verify_llvm_jit_module(module, true), "actual linked module verifies on target");
        auto const name{full::details::get_llvm_wasm_function_name(*prepared.mod, 1u)};
        auto const answer{module.getFunction(full::details::get_llvm_string_ref(name))};
        require(answer && !answer->isDeclaration(), "real checked answer definition");
        auto const calling_convention{answer->getCallingConv()};
        require(calling_convention == full::details::get_llvm_jit_wasm_calling_conv(), "actual typed entry ABI");
        // The owner/context destruction order follows the production wrapper.
        // This source fixture has no host import, debugger/checkpoint hooks, GC
        // collection, native publication, CFI stack report or restore authority.
        auto context{::std::move(whole.llvm_jit_module.llvm_context_holder)};
        auto emitted{::std::move(whole.llvm_jit_module.llvm_module)};
        whole.llvm_jit_module.emitted = false;
        ::std::unique_ptr<::llvm::ExecutionEngine> engine{::llvm::EngineBuilder{::std::unique_ptr<::llvm::Module>{emitted.release()}}
            .setEngineKind(::llvm::EngineKind::JIT).create(target.release())};
        require(bool(engine), "actual MCJIT engine owns all merged definitions");
        ::uwvm2::runtime::lib::details::pending_llvm_jit_code_ranges pending_ranges{*engine, true};
        engine->finalizeObject();
        require(!name.empty() && name.size() <= static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()),
            "actual owned generated symbol extent");
        // [owned name.data() ... name.size() characters] one-past
        // [safe] full size proven before forming the end iterator. LLVM's
        // actual getFunctionAddress ABI requires this std::string-compatible
        // name type; StringRef is a different API and is never substituted.
        lazy::details::llvm_jit_function_address_name_t native_name{name.data(), name.data() + name.size()};
        auto const wide{engine->getFunctionAddress(native_name)};
        require(wide != 0u && wide <= (::std::numeric_limits<::std::uintptr_t>::max)(), "actual resolved native entry extent");
        auto const code{::uwvm2::runtime::lib::details::native_function_code_address(static_cast<::std::uintptr_t>(wide))};
        require(pending_ranges.owns_pending_loaded_function_entry(code) &&
            !pending_ranges.owns_pending_loaded_function_entry(0u),
            "actual same-engine loaded function entry, no guessed text membership");
        pending_ranges.commit([](::std::uintptr_t, ::std::uintptr_t) noexcept {},
            [](::std::uintptr_t, ::std::uintptr_t) noexcept {});
        require(!pending_ranges.owns_pending_loaded_function_entry(code),
            "private listener entry-query unavailable after real commit/detach");
        using entry_type = ::std::int32_t (UWVM2TEST_WASM_ABI*)();
        auto const run{reinterpret_cast<entry_type>(static_cast<::std::uintptr_t>(wide))};
        require(run() == 42, "real two-function checked IR and loop returns 42");
        ::fast_io::print(::fast_io::out(), "CHECKED_WHOLE_IR exact_metadata=1 retained_fragments=2 actual_result=42 raw_wasm_rewalk=0\n");
    }
    void whole_decline_is_not_invalid(char const* path)
    {
        auto source{source_file(path)}; auto policy{features()};
        auto prepared{prepare_runtime_from_wasm(source, u8"checked-whole-unavailable", {}, policy)};
        require(prepared.mod != nullptr, "actual declined source identity");
        full::compile_option options{}; options.validator_feature_parameter = &policy;
        options.compilation_mode = full::llvm_jit_compilation_mode::tiered;
        options.emit_call_stack_frames = false; options.native_exception_target_machine = nullptr;
        ::uwvm2::validation::error::code_validation_error_impl error{};
        auto storage{lazy::initialize_lazy_module_storage(*prepared.mod, options, error)};
        full::full_function_symbol_t absent{};
        require(storage.checked_ir_plan && storage.checked_ir_plan->admission_available() &&
            !storage.checked_ir_plan->consume_whole_checked_symbol(*prepared.mod, absent) &&
            absent.local_funcs.empty() && !absent.llvm_jit_module.llvm_module && !absent.llvm_jit_module.llvm_context_holder &&
            error.err_code == ::uwvm2::validation::error::code_validation_error_code::ok,
            "legal missing native fragment refuses whole optimization before output mutation");
        ::fast_io::print(::fast_io::out(), "CHECKED_WHOLE_IR legal_unavailable=1 output_unchanged=1 fake_raw_fallback=0\n");
    }
}
int main(int argc, char const* const* argv)
{
    require(argc == 3, "exact whole-symbol and original native-decline binaries");
    consume_and_execute(argv[1]); whole_decline_is_not_invalid(argv[2]);
}
