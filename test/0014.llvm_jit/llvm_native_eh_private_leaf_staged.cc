// Actual owning source -> complete fused traversal -> independent staged IR.
// No loaded private code, native CFI registration, guest execution or timing.
#define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <uwvm2/runtime/compiler/shared/wasm_exception_private_leaf_effect.h>
#include <uwvm2/uwvm/wasm/loader/wasm_file.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/Verifier.h>
#include <string_view>
namespace compiler = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
namespace full = ::uwvm2::uwvm::runtime::full;
namespace loader = ::uwvm2::uwvm::wasm::loader;
namespace initializer = ::uwvm2::uwvm::runtime::initializer;
namespace effect = ::uwvm2::runtime::compiler::shared::wasm_exception_private_leaf_effect;
namespace c = ::uwvm2::utils::container;
static void require(bool value, unsigned line)
{
    if(!value) { ::fast_io::io::perrln("FAIL native EH observer fused line=", line); ::fast_io::fast_terminate(); }
}
#define EH_OBSERVE_CHECK(...) require(static_cast<bool>((__VA_ARGS__)), __LINE__)
static c::u8string ir_text(compiler::full_function_symbol_t const& storage)
{
    EH_OBSERVE_CHECK(storage.llvm_jit_module.emitted && storage.llvm_jit_module.llvm_module);
    c::u8string text{}; compiler::details::raw_uwvm_string_ostream stream{text};
    storage.llvm_jit_module.llvm_module->print(stream, nullptr); stream.flush(); return text;
}
struct native_cleanup
{
    ~native_cleanup()
    {
        // [all local LLVM/context observations have already been destroyed]
        // [safe] No guest activation was executed by this compiler fixture.
        ::uwvm2::runtime::lib::llvm_jit_reset_runtime_state_host_api();
        full::retire_selected_full_source_after_drain();
    }
};
int main(int argc, char** argv)
{
#if !defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) || UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER != 1 || \
    !defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) || UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF != 1
    ::fast_io::io::perrln("observer fixture requires exact macro=1"); return 2;
#else
    EH_OBSERVE_CHECK(argc == 3);
    bool const valid{::std::string_view{argv[2]} == "valid"};
    EH_OBSERVE_CHECK(valid || ::std::string_view{argv[2]} == "invalid");
    ::uwvm2test::uwvm_int_strict::configure_llvm_jit_runner_runtime();
    ::uwvm2::runtime::lib::llvm_jit_reset_runtime_state_host_api();
    EH_OBSERVE_CHECK(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter());
    ::std::unique_ptr<::llvm::TargetMachine> target{::llvm::EngineBuilder{}.selectTarget()};
    EH_OBSERVE_CHECK(target && ::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::supports_itanium_dwarf_object(*target));
    auto const native_path{c::u8concat_uwvm(::fast_io::mnp::os_c_str(argv[1]))};
    auto source{full::full_source_instance::create_unparsed(::std::u8string{native_path.data(), native_path.size()}, u8"observer-fused")};
    EH_OBSERVE_CHECK(full::select_unparsed_full_source_after_drain(source));
    native_cleanup cleanup{};
    auto features{::uwvm2test::uwvm_int_strict::make_wasm1p1_feature_parameter()};
    auto& policy{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features)};
    policy.disable_exceptions = false; policy.disable_function_references = false; policy.disable_gc = false;
    ::uwvm2::uwvm::wasm::type::wasm_parameter_t parameters{}; parameters.binfmt1_para = features;
    EH_OBSERVE_CHECK(loader::load_wasm_file(source->file_for_native_initialization(), source->owned_file_name(),
        source->owned_rename(), parameters) == loader::load_wasm_file_rtl::ok);
    EH_OBSERVE_CHECK(loader::construct_all_module_and_check_duplicate_module() == loader::load_and_check_modules_rtl::ok);
    EH_OBSERVE_CHECK(loader::check_import_exist_and_detect_cycles() == loader::load_and_check_modules_rtl::ok);
    initializer::initialize_runtime();
    EH_OBSERVE_CHECK(source->seal_actual_initializer());
    auto const module{source->initialized_main_module()};
    EH_OBSERVE_CHECK(module != nullptr && source->bind_actual_compiled_main(0uz, module));
    EH_OBSERVE_CHECK(source->actual_full_validation_epoch() == 0u);
    compiler::compile_option options{};
    options.compilation_mode = compiler::llvm_jit_compilation_mode::full;
    options.validator_feature_parameter = ::std::addressof(features);
    options.native_exception_target_machine = target.get();
    options.emit_call_stack_frames = false; options.emit_unwind_call_stack_frames = true;
    options.native_eh_leaf_source_owner = source;
    // Preserve the actual fused diagnostic across exception unwinding.
    ::uwvm2::validation::error::code_validation_error_impl error{};
    auto compile{[&](::std::size_t workers)
    {
        error = {};
        compiler::compile_task_split_config split{};
        split.policy = compiler::compile_task_split_policy_t::function_count; split.split_size = 1uz; split.adjust_for_default_policy = false;
        return compiler::compile_all_from_uwvm(*module, options, error, workers, split);
    }};
    options.record_native_eh_leaf_observations = true;
    if(!valid)
    {
        bool rejected{};
        try { auto invalid{compile(0uz)}; EH_OBSERVE_CHECK(!invalid.native_eh_leaf_observations.complete); }
        catch(::fast_io::error const& failure)
        {
            // Only the actual Wasm parse domain's invalid code is a standard
            // rejection. Allocation/LLVM/configuration errors must fail instead.
            if(failure.domain != ::fast_io::parse_domain_value || failure.code !=
                static_cast<::std::size_t>(static_cast<char8_t>(::fast_io::parse_code::invalid))) { throw; }
            EH_OBSERVE_CHECK(error.err_code == ::uwvm2::validation::error::code_validation_error_code::end_result_mismatch);
            auto const& detail{error.err_selectable.end_result_mismatch};
            EH_OBSERVE_CHECK(detail.block_kind == c::u8string_view{u8"function"} && detail.expected_count == 1uz &&
                detail.actual_count == 0uz && detail.expected_type == ::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32);
            EH_OBSERVE_CHECK(module->local_defined_function_vec_storage.size() == 2uz);
            // [actual owner-retained invalid_later record][index 1 < size 2]
            // [safe] Borrow only this genuine last function, then compare
            // integer byte addresses. No diagnostic pointer is dereferenced,
            // no endpoint is advanced and no cross-object subtraction occurs.
            auto const& late{module->local_defined_function_vec_storage.index_unchecked(1uz)};
            EH_OBSERVE_CHECK(late.wasm_code_ptr != nullptr);
            auto const begin{reinterpret_cast<::std::uintptr_t>(late.wasm_code_ptr->body.expr_begin)};
            auto const end{reinterpret_cast<::std::uintptr_t>(late.wasm_code_ptr->body.code_end)};
            auto const diagnostic{reinterpret_cast<::std::uintptr_t>(error.err_curr)};
            EH_OBSERVE_CHECK(begin != 0u && end > begin && diagnostic >= begin && diagnostic == end - 1u);
            rejected = true;
        }
        EH_OBSERVE_CHECK(rejected && !options.native_eh_leaf_active_attempt && source->actual_full_validation_epoch() == 0u);
        ::fast_io::io::println("PASS actual fused late function=1 parse_domain=invalid end_result_mismatch expected=1 actual=0; no complete observation; native_execution=false");
        return 0;
    }
    auto check{[](compiler::full_function_symbol_t const& result)
    {
        EH_OBSERVE_CHECK(result.native_eh_leaf_observations.complete && result.local_funcs.size() == 5uz);
        EH_OBSERVE_CHECK(result.native_eh_leaf_observations.completed_functions == 5uz);
        EH_OBSERVE_CHECK(result.native_eh_leaf_observations.fragment_calls == 2uz);
        EH_OBSERVE_CHECK(result.native_eh_leaf_observations.consumed_effect_edges == 1uz);
        auto const& leaf{*result.local_funcs.index_unchecked(0uz).native_eh_leaf_observation->observation};
        EH_OBSERVE_CHECK(leaf.numeric_leaf_observed() && leaf.escaping_tag_count() == 2uz);
        auto const& local_caught{*result.local_funcs.index_unchecked(2uz).native_eh_leaf_observation->observation};
        EH_OBSERVE_CHECK(local_caught.numeric_leaf_observed() && local_caught.escaping_tag_count() == 0uz);
        auto const& retained{*result.local_funcs.index_unchecked(3uz).native_eh_leaf_observation};
        EH_OBSERVE_CHECK(retained.witnesses.size() == 1uz && effect::observe_consumed_direct_call(*retained.observation,
            0uz, leaf) == effect::selection::retaining_handler);
        auto const& dead{*result.local_funcs.index_unchecked(4uz).native_eh_leaf_observation};
        EH_OBSERVE_CHECK(dead.witnesses.empty() && dead.observation->direct_callsite_count() == 0uz);
    }};
    for(auto workers: {0uz, 2uz})
    {
        options.record_native_eh_leaf_observations = false;
        auto baseline{compile(workers)}; auto original_ir{ir_text(baseline)};
        EH_OBSERVE_CHECK(!baseline.native_eh_leaf_observations.complete);
        options.record_native_eh_leaf_observations = true;
        auto recorded{compile(workers)}; check(recorded);
        EH_OBSERVE_CHECK(ir_text(recorded) == original_ir && !options.native_eh_leaf_active_attempt);
        options.native_eh_leaf_observation_limits.instructions = 0uz;
        auto declined{compile(workers)};
        EH_OBSERVE_CHECK(!declined.native_eh_leaf_observations.complete && ir_text(declined) == original_ir);
        options.native_eh_leaf_observation_limits = {};
        options.native_eh_leaf_source_owner.reset();
        auto no_owner{compile(workers)};
        EH_OBSERVE_CHECK(!no_owner.native_eh_leaf_observations.complete && ir_text(no_owner) == original_ir);
        options.native_eh_leaf_source_owner = source;
    }
    options.stage_native_eh_private_leaf = true;
    for(auto workers: {0uz, 2uz})
    {
        auto candidate{compile(workers)}; check(candidate);
        auto const& stage{candidate.staged_native_eh_private_leaf};
#if defined(__riscv) && defined(__riscv_xlen) && __riscv_xlen == 64
        // This first staging matcher does not authenticate the actual RV64
        // materializer yet. Original full compilation must still succeed.
        EH_OBSERVE_CHECK(!stage);
#else
        EH_OBSERVE_CHECK(stage && stage->selected_edges() == 1uz && stage->clones() == 1uz &&
            stage->replaced_numeric_throws() == 2uz && !stage->has_loaded_code_permission() &&
            !stage->has_registered_native_cfi() && !stage->runtime_selected());
        auto const staged_ir{stage->ir_for_cold_inspection()};
        EH_OBSERVE_CHECK(staged_ir && !::llvm::verifyModule(*staged_ir) &&
            ::std::addressof(staged_ir->getContext()) != candidate.llvm_jit_module.llvm_context_holder.get());
        auto const leaf_name{compiler::details::get_llvm_wasm_function_name(*module, 0u)};
        auto const caller_name{compiler::details::get_llvm_wasm_function_name(*module, 1u)};
        auto const leaf_view{compiler::details::get_llvm_string_ref(leaf_name)};
        auto const original_leaf{candidate.llvm_jit_module.llvm_module->getFunction(leaf_view)};
        auto const original_caller{candidate.llvm_jit_module.llvm_module->getFunction(compiler::details::get_llvm_string_ref(caller_name))};
        auto const staged_leaf{staged_ir->getFunction(leaf_view)};
        auto const private_leaf{staged_ir->getFunction(leaf_view.str() + ".native_eh_private_leaf.r1")};
        EH_OBSERVE_CHECK(original_leaf && original_caller && staged_leaf && private_leaf && private_leaf->hasLocalLinkage() &&
            private_leaf->getFunctionType() == staged_leaf->getFunctionType() &&
            private_leaf->getCallingConv() == staged_leaf->getCallingConv() && !private_leaf->hasFnAttribute(::llvm::Attribute::NoUnwind) &&
            private_leaf->hasFnAttribute(::llvm::Attribute::UWTable) == staged_leaf->hasFnAttribute(::llvm::Attribute::UWTable) &&
            private_leaf->hasPersonalityFn() == staged_leaf->hasPersonalityFn());
        auto count_bridge{[](::llvm::Function const& function, bool private_bridge)
        {
            ::std::size_t count{};
            for(auto const& block: function) for(auto const& instruction: block)
            {
                auto const call{::llvm::dyn_cast<::llvm::CallBase>(::std::addressof(instruction))};
                if(call == nullptr || call->getCalledFunction() == nullptr) { continue; }
                auto const name{private_bridge ? compiler::details::get_llvm_runtime_bridge_function_symbol_name<
                    ::uwvm2::runtime::lib::details::llvm_jit_throw_numeric_private_leaf_no_trace_wide_r1>(call->getFunctionType()) :
                    compiler::details::get_llvm_runtime_bridge_function_symbol_name<
                    ::uwvm2::runtime::lib::details::llvm_jit_throw_numeric_abi_bridge>(call->getFunctionType())};
                if(call->getCalledFunction()->getName() == compiler::details::get_llvm_string_ref(name)) { ++count; }
            }
            return count;
        }};
        EH_OBSERVE_CHECK(count_bridge(*original_leaf, false) == 2uz && count_bridge(*original_leaf, true) == 0uz &&
            count_bridge(*staged_leaf, false) == 2uz && count_bridge(*private_leaf, false) == 0uz && count_bridge(*private_leaf, true) == 2uz);
        ::llvm::CallBase* actual{};
        for(auto& block: *original_caller) for(auto& instruction: block)
        {
            auto const call{::llvm::dyn_cast<::llvm::CallBase>(::std::addressof(instruction))};
            if(call != nullptr && call->getMetadata(compiler::native_eh_private_leaf::call_metadata_name) != nullptr)
            {
                // [actual compiler-owned function instructions] checked call.
                // [safe] Borrow only within this still-live candidate/context.
                EH_OBSERVE_CHECK(actual == nullptr); actual = call;
            }
        }
        EH_OBSERVE_CHECK(actual && actual->getCalledFunction() == original_leaf);
        compiler::native_eh_private_leaf::call_identity identity{};
        EH_OBSERVE_CHECK(compiler::native_eh_private_leaf::read_actual_call_metadata(*actual, identity) &&
            identity.function_index == 1uz && identity.callsite_index == 0uz && identity.target_index == 0uz);
        EH_OBSERVE_CHECK(!compiler::native_eh_private_leaf::attach_actual_call_metadata(*actual, identity));
        // Pure format negatives on a REAL fused call. They issue no proof and
        // do not invoke the private factory or execute either IR module.
        actual->setMetadata(compiler::native_eh_private_leaf::call_metadata_name,
            ::llvm::MDNode::get(actual->getContext(), ::llvm::ArrayRef<::llvm::Metadata*>{}));
        EH_OBSERVE_CHECK(!compiler::native_eh_private_leaf::read_actual_call_metadata(*actual, identity));
        ::llvm::Metadata* wrong_version[6uz]{};
        for(auto& value: wrong_version)
        { value = ::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(::llvm::Type::getInt64Ty(actual->getContext()), 2u)); }
        actual->setMetadata(compiler::native_eh_private_leaf::call_metadata_name, ::llvm::MDNode::get(actual->getContext(), wrong_version));
        EH_OBSERVE_CHECK(!compiler::native_eh_private_leaf::read_actual_call_metadata(*actual, identity));
#endif
    }
    options.native_eh_leaf_source_owner.reset();
    auto no_private_source{compile(0uz)};
    EH_OBSERVE_CHECK(!no_private_source.staged_native_eh_private_leaf && !no_private_source.native_eh_leaf_observations.complete);
    options.native_eh_leaf_source_owner = source;
    EH_OBSERVE_CHECK(source->actual_full_validation_epoch() == 0u);
    ::fast_io::io::println("PASS actual fused private-leaf staging controls; loaded_private_code=false runtime_selected=false native_execution=false timing=false");
    return 0;
#endif
}
