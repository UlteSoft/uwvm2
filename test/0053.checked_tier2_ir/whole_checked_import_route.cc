// Genuine initialized source -> fused typed emission -> factory-owned ALL
// fragments -> whole consumer -> actual MCJIT result. Cold alias setup is test
// DATA after initializer completion, not guest mutation or publication proof.
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/compiler/llvm_jit/compile_cu_from_lazy_validator/impl.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Target/TargetMachine.h>
#include <array>
#include <limits>
#include <memory>
#if !defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
#error "Use the real matching LLVM/runtime provider closure."
#endif
namespace
{
    using namespace uwvm2test::uwvm_int_strict;
    namespace full = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
    namespace lazy = ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator;
    namespace runtime = ::uwvm2::uwvm::runtime::storage;
    void require(bool valid, char const* message)
    {
        if(!valid)
        {
            ::fast_io::print(::fast_io::err(), "whole checked import route: ", ::fast_io::mnp::os_c_str(message), "\n");
            ::fast_io::fast_terminate();
        }
    }
    byte_vec read_source(char const* path)
    {
        ::fast_io::native_file_loader input{::fast_io::mnp::os_c_str(path), ::fast_io::open_mode::in};
        require(input.size() >= 8uz && input.size() <= 65536uz, "actual complete source extent");
        byte_vec result(input.size());
        // [RAII owned file bytes / equally sized immutable owned source] end
        // [safe] complete extent bound BEFORE copy; neither pointer advances.
        ::fast_io::freestanding::my_memcpy(result.data(), input.data(), input.size());
        return result;
    }
    void check_actual_route(::llvm::Module const& module)
    {
        ::std::size_t rows{};
        for(auto const& function: module)
        {
            for(auto const& block: function)
            {
                auto const terminator{block.empty() || !block.back().isTerminator() ? nullptr : ::std::addressof(block.back())};
                if(terminator == nullptr || terminator->getMetadata("uwvm.checked.lazy.unwind.import.route.v1") == nullptr) { continue; }
                auto const branch{::llvm::dyn_cast<::llvm::BranchInst>(terminator)};
                require(branch && branch->isConditional(), "real tagged compiler branch");
                auto const condition{::llvm::dyn_cast<::llvm::ConstantInt>(branch->getCondition())};
                require(condition && condition->getType()->isIntegerTy(1u) && condition->isOne(),
                    "whole consumer selects actual same-module typed alternative");
                ++rows;
            }
        }
        require(rows == 1uz, "exactly one real staged import site survives before optimization");
    }
    void execute(char const* module_path, char const* provider_path)
    {
        auto source{read_source(module_path)}, provider{read_source(provider_path)};
        auto policy{make_wasm1p1_feature_parameter()};
        ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(policy).disable_memory64 = false;
        auto prepared{prepare_runtime_from_wasm(source, u8"whole-route-consumer",
            {preloaded_wasm_module{.wasm_bytes = &provider, .module_name = u8"checked-route-provider"}}, policy)};
        require(prepared.mod && prepared.mod->imported_function_vec_storage.size() == 1uz &&
            prepared.mod->local_defined_function_vec_storage.size() == 2uz, "real initialized source shape");
        auto const foreign{full::details::resolve_runtime_direct_callee(*prepared.mod, 0u)};
        require(foreign.state_valid && !foreign.direct_callable, "actual foreign import never treated as local");
        // Same cold alias setup as the genuine grouped fixture. Source operands
        // remain immutable. The actual resolver still owes exact target member,
        // canonical function signature and original compiler CC/prototype.
        auto& alias{const_cast<runtime::wasm_module_storage_t*>(prepared.mod)->imported_function_vec_storage.index_unchecked(0uz)};
        alias.link_kind = runtime::imported_function_link_kind::defined;
        alias.target.defined_ptr = const_cast<runtime::local_defined_function_storage_t*>(
            &prepared.mod->local_defined_function_vec_storage.index_unchecked(0uz));
        auto const local{full::details::resolve_runtime_direct_callee(*prepared.mod, 0u)};
        require(local.state_valid && local.direct_callable && local.func_index == 1u, "actual same-module alias identity");
        require(lazy::details::ensure_llvm_jit_native_target_initialized(), "actual process LLVM target");
        ::std::unique_ptr<::llvm::TargetMachine> target{::llvm::EngineBuilder{}.selectTarget()};
        require(bool(target), "real target owner lifetime extends through consumption and native compilation");
        ::std::array<runtime::llvm_jit_raw_call_target_t, 2uz> raw{};
        ::std::array<::std::uintptr_t, 2uz> typed{};
        full::compile_option options{}; options.validator_feature_parameter = &policy;
        options.compilation_mode = full::llvm_jit_compilation_mode::tiered;
        options.emit_call_stack_frames = false; options.emit_unwind_call_stack_frames = true;
        options.emit_precise_gc_root_frames = false; options.route_wasm_calls_through_runtime_bridge = true;
        options.native_exception_target_machine = target.get();
        options.lazy_defined_targets_are_atomic = true;
        options.lazy_defined_raw_call_target_base_address = reinterpret_cast<::std::uintptr_t>(raw.data());
        options.lazy_defined_raw_call_target_count = raw.size();
        options.lazy_defined_typed_entry_target_base_address = reinterpret_cast<::std::uintptr_t>(typed.data());
        options.lazy_defined_typed_entry_target_count = typed.size();
        ::uwvm2::validation::error::code_validation_error_impl error{};
        auto storage{lazy::initialize_lazy_module_storage(*prepared.mod, options, error)};
        require(storage.checked_ir_plan && storage.checked_ir_plan->admission_available() &&
            storage.checked_ir_plan->native_ir_available(0uz) && storage.checked_ir_plan->native_ir_available(1uz),
            "original fused emission retained both real checked native fragments");
        // Whole consumption authenticates ALL immutable factory definitions;
        // unlike a genuine T1 group it does not forge scheduler compiling states.
        for(auto const& function: storage.functions)
        { require(function.materialization_state.state.load(::std::memory_order_acquire) ==
            ::uwvm2::utils::thread::lazy_compile_state::uncompiled, "whole consumer cannot depend on fake claims"); }
        full::full_function_symbol_t whole{};
        require(storage.checked_ir_plan->consume_whole_checked_symbol(*prepared.mod, whole), "actual ALL-owner consumer");
        check_actual_route(*whole.llvm_jit_module.llvm_module);
        auto& module{*whole.llvm_jit_module.llvm_module};
#if LLVM_VERSION_MAJOR >= 21
        module.setTargetTriple(target->getTargetTriple());
#else
        module.setTargetTriple(target->getTargetTriple().str());
#endif
        module.setDataLayout(target->createDataLayout());
        require(full::details::verify_llvm_jit_module(module, true), "actual linked SSA and target ABI verify");
        auto const name{full::details::get_llvm_wasm_function_name(*prepared.mod, 2u)};
        auto const answer{module.getFunction(full::details::get_llvm_string_ref(name))};
        require(answer && !answer->isDeclaration() && answer->getCallingConv() == full::details::get_llvm_jit_wasm_calling_conv(),
            "actual typed answer definition and calling convention");
        auto context{::std::move(whole.llvm_jit_module.llvm_context_holder)};
        auto emitted{::std::move(whole.llvm_jit_module.llvm_module)};
        whole.llvm_jit_module.emitted = false;
        ::std::unique_ptr<::llvm::ExecutionEngine> engine{::llvm::EngineBuilder{::std::unique_ptr<::llvm::Module>{emitted.release()}}
            .setEngineKind(::llvm::EngineKind::JIT).create(target.release())};
        require(bool(engine), "same real MCJIT owns ALL selected definitions");
        engine->finalizeObject();
        require(!name.empty() && name.size() <= static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()),
            "actual generated name extent before end formation");
        // [owned generated symbol0 ... size] one-past
        // [safe] extent proven BEFORE forming name.data()+size. This is the
        // exact std-string-compatible ExecutionEngine ABI, not a guessed view.
        lazy::details::llvm_jit_function_address_name_t native_name{name.data(), name.data() + name.size()};
        auto const address{engine->getFunctionAddress(native_name)};
        require(address != 0u && address <= (::std::numeric_limits<::std::uintptr_t>::max)(), "real resolved typed entry");
        using entry = ::std::int64_t (UWVM2TEST_WASM_ABI*)();
        auto const run{reinterpret_cast<entry>(static_cast<::std::uintptr_t>(address))};
        require(run() == 42, "actual whole-owned typed import returns42; foreign original would return99");
        ::fast_io::print(::fast_io::out(), "CHECKED_WHOLE_IMPORT all_owned_definitions=1 fake_claims=0 actual_result=42 raw_wasm_rewalk=0\n");
    }
}
int main(int argc, char const* const* argv)
{
    require(argc == 3, "existing unwind_import_route.wasm and provider.wasm required");
    execute(argv[1], argv[2]);
}
