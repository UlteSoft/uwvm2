// Actual finalized parser/initializer -> ONE original INT fused walker ->
// original ring artifact and real LLVM SSA module. No pure body prevalidation,
// second body translator, fake stop/publication, or whole-tiered claim.
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/compiler/shared/i32_dual_emission.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include <array>
#include <type_traits>
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT) || defined(UWVM2TEST_STRICT_NO_INTERPRETER)
#error "This fixture must execute the actual original INT artifact, not a replacement JIT test runner."
#endif
#if defined(UWVM_DISABLE_INT) || defined(UWVM_DISABLE_JIT)
#error "Use matching actual INT and LLVM compiler/runtime/provider source closure."
#endif
namespace
{
    using namespace ::uwvm2test::uwvm_int_strict;
    namespace dual = ::uwvm2::runtime::compiler::shared::i32_dual_emission;
    namespace full = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
    using error_code = ::uwvm2::validation::error::code_validation_error_code;
#if defined(UWVM2TEST_DUAL_COMPARE_CACHED_RING)
    constexpr auto option{make_tailcall_fully_split_opt<3uz, 3uz, 8uz, 8uz>()};
    static_assert(option.is_tail_call && option.i32_stack_top_begin_pos != option.i32_stack_top_end_pos &&
        option.i64_stack_top_begin_pos != option.i64_stack_top_end_pos &&
        option.i32_stack_top_begin_pos != SIZE_MAX && option.i64_stack_top_begin_pos != SIZE_MAX);
#elif defined(UWVM2TEST_DUAL_I32_TAIL)
    constexpr auto option{k_test_tail_min_opt};
#else
    constexpr auto option{k_test_byref_opt};
#endif
#if defined(UWVM2TEST_DUAL_I64_PRELOAD_BATCH)
# if !defined(UWVM_ENABLE_UWVM_INT_INSTRUCTION_REORDER) || !defined(UWVM2TEST_DUAL_COMPARE_CACHED_RING)
#  error "Actual preload fixture requires compiled instruction reorder and nonempty integer register-ring slots."
# endif
    struct scoped_preload_policy
    {
        using level_type = ::uwvm2::uwvm::runtime::runtime_mode::runtime_uwvm_int_opcode_conbination_level_t;
        bool original_reorder{::uwvm2::uwvm::runtime::runtime_mode::runtime_uwvm_int_enable_instruction_reorder};
        level_type original_combine{::uwvm2::uwvm::runtime::runtime_mode::global_runtime_uwvm_int_opcode_conbination_level};
        scoped_preload_policy() noexcept
        {
            ::uwvm2::uwvm::runtime::runtime_mode::runtime_uwvm_int_enable_instruction_reorder = true;
            // Keep the exact original pending gate clean, so this extra source
            // recipe actually reaches the original cached preload scanner.
            ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_uwvm_int_opcode_conbination_level = level_type::disable;
        }
        ~scoped_preload_policy()
        {
            ::uwvm2::uwvm::runtime::runtime_mode::runtime_uwvm_int_enable_instruction_reorder = original_reorder;
            ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_uwvm_int_opcode_conbination_level = original_combine;
        }
        scoped_preload_policy(scoped_preload_policy const&) = delete;
        scoped_preload_policy& operator=(scoped_preload_policy const&) = delete;
    };
#endif
    template<typename> struct first_argument;
    template<typename R, typename C, typename A>
    struct first_argument<R (C::*)(A)> { using type = ::std::remove_cvref_t<A>; };
    using native_symbol_name = typename first_argument<decltype(&::llvm::ExecutionEngine::getFunctionAddress)>::type;
    void require(bool valid, char const* text)
    {
        if(!valid)
        {
            ::fast_io::print(::fast_io::err(), "real fused integer compare dual sink: ", ::fast_io::mnp::os_c_str(text), "\n");
            ::fast_io::fast_terminate();
        }
    }
    byte_vec source_file(char const* path)
    {
        ::fast_io::native_file_loader input{::fast_io::mnp::os_c_str(path), ::fast_io::open_mode::in};
        require(input.size() >= 8uz && input.size() <= 65536uz, "bounded complete actual Wasm source");
        byte_vec source(input.size());
        // [RAII file input ... equal-length owned source] source_end
        // [safe] checked sizes BEFORE exact copy; neither pointer is advanced.
        ::fast_io::freestanding::my_memcpy(source.data(), input.data(), input.size());
        return source;
    }
    auto enabled_features()
    {
        auto result{make_wasm1p1_feature_parameter()};
        auto& policy{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(result)};
        policy.disable_gc = false; policy.disable_memory64 = false; policy.disable_multi_memory = false;
        policy.disable_function_references = false; policy.disable_exceptions = false; policy.disable_tail_call = false;
        return result;
    }
    template<bool Wide>
    ::std::uint64_t run_ring(dual::checked_module<option> const& owned, runtime_module_t const& module,
                            ::std::size_t index, byte_vec const& parameters = {})
    {
        require(index < module.local_defined_function_vec_storage.size() &&
            index < owned.ring_artifact().local_funcs.size(), "same actual local/ring index bounds");
        auto result{interpreter_runner<option>::run(owned.ring_artifact().local_funcs.index_unchecked(index),
            module.local_defined_function_vec_storage.index_unchecked(index), parameters, nullptr, nullptr)};
        using bits_type = ::std::conditional_t<Wide, ::std::uint64_t, ::std::uint32_t>;
        require(result.results.size() == sizeof(bits_type), "actual tight original ring result ABI extent");
        bits_type bits{};
        // [actual complete result bytes] one-past
        // [safe] exact same ABI extent BEFORE bit-preserving copy, no endian parser.
        ::fast_io::freestanding::my_memcpy(::std::addressof(bits), result.results.data(), sizeof(bits));
        return bits;
    }
    template<bool Wide, bool ParamWide = Wide>
    ::std::uint64_t run_native(dual::checked_module<option>& owned, runtime_module_t const& module,
                              ::std::size_t index, unsigned parameter_kind = 0u)
    {
        require(owned.native_ir_available(index), "actual one-walk width LLVM owner");
        auto fragment{owned.take_checked_ir(index)};
        require(fragment.emitted && fragment.llvm_module && fragment.llvm_context_holder &&
            !owned.native_ir_available(index), "actual IR ownership moves exactly once");
        auto const name{full::details::get_llvm_wasm_function_name(module, static_cast<::std::uint_least32_t>(index))};
        auto const definition{fragment.llvm_module->getFunction(full::details::get_llvm_string_ref(name))};
        unsigned const expected_arguments{parameter_kind == 0u ? 0u : parameter_kind == 1u ? 1u : 2u};
        require(definition && !definition->isDeclaration() && definition->arg_size() == expected_arguments &&
            definition->getReturnType()->isIntegerTy(Wide ? 64u : 32u) &&
            definition->getCallingConv() == full::details::get_llvm_jit_wasm_calling_conv(), "real exact conversion prototype and CC");
        unsigned position{};
        for(auto const& argument: definition->args())
        {
            unsigned const bits{parameter_kind == 2u ? 64u : (ParamWide ? 64u : 32u)};
            require(argument.getType()->isIntegerTy(bits), "each actual typed mixed-width argument"); ++position;
        }
        require(full::details::verify_llvm_jit_module(*fragment.llvm_module, true), "actual final SSA verifier");
        ::std::unique_ptr<::llvm::TargetMachine> machine{::llvm::EngineBuilder{}.selectTarget()};
        require(bool(machine) && machine->createDataLayout() == fragment.llvm_module->getDataLayout(), "actual matching target layout");
#if LLVM_VERSION_MAJOR >= 21
        require(machine->getTargetTriple() == fragment.llvm_module->getTargetTriple(), "actual target triple");
#else
        require(machine->getTargetTriple().str() == fragment.llvm_module->getTargetTriple(), "actual target triple");
#endif
        // Context/source survive the REAL MCJIT engine; no guessed target/PC.
        auto context{::std::move(fragment.llvm_context_holder)}; auto emitted{::std::move(fragment.llvm_module)};
        ::std::unique_ptr<::llvm::ExecutionEngine> engine{::llvm::EngineBuilder{
            ::std::unique_ptr<::llvm::Module>{emitted.release()}}.setEngineKind(::llvm::EngineKind::JIT).create(machine.release())};
        require(bool(engine), "real owned native engine"); engine->finalizeObject();
        require(!name.empty() && name.size() <= static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()),
            "same owned symbol extent before end iterator");
        // [actual complete name characters] one-past
        // [safe] extent checked BEFORE forming this end; construction copies.
        native_symbol_name native_name{name.data(), name.data() + name.size()};
        auto const address{engine->getFunctionAddress(native_name)};
        require(address != 0u && address <= (::std::numeric_limits<::std::uintptr_t>::max)(), "real finalized exact typed symbol");
        using result_type = ::std::conditional_t<Wide, ::std::uint64_t, ::std::uint32_t>;
        using parameter_type = ::std::conditional_t<ParamWide, ::std::uint64_t, ::std::uint32_t>;
        if(parameter_kind == 2u)
        {
            require(!Wide, "actual pair64 has i32 result");
            using entry_type = ::std::uint32_t (UWVM2TEST_WASM_ABI*)(::std::uint64_t,::std::uint64_t);
            return reinterpret_cast<entry_type>(static_cast<::std::uintptr_t>(address))(0xffffffffffffffffull,0ull);
        }
        if(parameter_kind == 1u)
        {
            using entry_type = result_type (UWVM2TEST_WASM_ABI*)(parameter_type);
            return reinterpret_cast<entry_type>(static_cast<::std::uintptr_t>(address))(parameter_type{0x80000000u});
        }
        using entry_type = result_type (UWVM2TEST_WASM_ABI*)();
        return reinterpret_cast<entry_type>(static_cast<::std::uintptr_t>(address))();
    }
    template<typename Bits>
    void append_parameter(byte_vec& output, Bits value)
    {
        auto const offset{output.size()};
        require(offset <= SIZE_MAX - sizeof(value), "typed parameter allocation sum before resize");
        output.resize(offset + sizeof(value));
        require(offset <= output.size() && sizeof(value) <= output.size() - offset, "complete tight ABI cell before pointer formation");
        // [actual tight existing parameters][new sizeof(Bits) cell] end
        // [safe] checked size before forming cell; raw host ABI copy, no LE parser.
        ::fast_io::freestanding::my_memcpy(output.data() + offset, ::std::addressof(value), sizeof(value));
    }
    void positive(char const* path)
    {
        auto source{source_file(path)}; auto features{enabled_features()};
        auto prepared{prepare_runtime_from_wasm(source, u8"real-integer-compare-dual-sink", {}, features)};
        require(prepared.mod && prepared.mod->local_defined_function_vec_storage.size() == 30uz, "actual22 comparisons+3 typed parameter bodies+4 modern functions+start");
        ::std::unique_ptr<::llvm::TargetMachine> machine{::llvm::EngineBuilder{}.selectTarget()};
        require(bool(machine), "actual target lives through synchronous first-walk emission");
        dual::emission_policy policy{}; policy.target = machine.get(); policy.emit_integer_compare = true;
        ::uwvm2::validation::error::code_validation_error_impl error{};
        auto owned{dual::compile<option>(*prepared.mod, {}, policy, error, &features)};
        require(owned.typed_admission_complete() && error.err_code == error_code::ok && owned.ring_artifact().local_funcs.size() == 30uz,
            "all original typed bodies/fixups precede seal");
        constexpr ::std::array<::std::uint32_t,22uz> expected{{1u,1u,1u,1u,0u,0u,1u,1u,0u,0u,1u,0u,1u,1u,1u,0u,0u,1u,1u,0u,0u,1u}};
        for(::std::size_t local{}; local != expected.size(); ++local)
        {
            require(run_ring<false>(owned,*prepared.mod,local) == expected[local] &&
                run_native<false>(owned,*prepared.mod,local) == expected[local], "actual ICmp22+canonical i32 true/false bits");
        }
        byte_vec narrow{}; append_parameter(narrow,::std::uint32_t{0x80000000u});
        byte_vec wide{}; append_parameter(wide,::std::uint64_t{0x80000000u});
        byte_vec pair{}; append_parameter(pair,::std::uint64_t{0xffffffffffffffffull}); append_parameter(pair,::std::uint64_t{0ull});
        require(run_ring<false>(owned,*prepared.mod,22uz,narrow) == 0u && run_native<false,false>(owned,*prepared.mod,22uz,1u) == 0u,
            "real parami32 to resulti32 eqz ABI");
        require(run_ring<false>(owned,*prepared.mod,23uz,wide) == 0u && run_native<false,true>(owned,*prepared.mod,23uz,1u) == 0u,
            "real parami64 to resulti32 eqz ABI");
        require(pair.size() == 16uz && run_ring<false>(owned,*prepared.mod,24uz,pair) == 1u &&
            run_native<false>(owned,*prepared.mod,24uz,2u) == 1u, "real pair64 unsigned compare and original preload callbacks");
        for(::std::size_t local{25uz}; local != 30uz; ++local)
        { require(!owned.native_ir_available(local), "GC/control/tail/memory64/start subset is valid ring but unavailable native slice"); }
        require(run_ring<false>(owned,*prepared.mod,25uz) == 42u && run_ring<false>(owned,*prepared.mod,26uz) == 42u &&
            run_ring<false>(owned,*prepared.mod,27uz) == 1u && run_ring<false>(owned,*prepared.mod,28uz) == 42u,
            "genuine modern ring calls and bypassed dead lexical child comparisons");
        auto const start{interpreter_runner<option>::run(owned.ring_artifact().local_funcs.index_unchecked(29uz),
            prepared.mod->local_defined_function_vec_storage.index_unchecked(29uz), {}, nullptr, nullptr)};
        require(start.results.empty(), "genuine called start validates all22 comparison outputs and actual modern functions");
        auto old_policy{policy}; old_policy.emit_integer_compare = false;
        auto old_slice{dual::compile<option>(*prepared.mod, {}, old_policy, error, &features)};
        require(old_slice.typed_admission_complete() && !old_slice.native_ir_available(0uz) && run_ring<false>(old_slice,*prepared.mod,6uz) == 1u,
            "compare policyfalse admits original ring and preserves prior slice");
        auto quota_policy{policy}; quota_policy.module_operations = 0uz;
        auto quota{dual::compile<option>(*prepared.mod, {}, quota_policy, error, &features)};
        require(quota.typed_admission_complete() && !quota.native_ir_available(0uz) && run_ring<false>(quota,*prepared.mod,0uz) == 1u,
            "zero successful IR quota still admits original typed bodies");
        ::fast_io::print(::fast_io::out(), "REAL_DUAL_INTEGER_COMPARE comparison=22 typed_parameters=3 called_start=1 modern=4 native_ring_i32=matched full_tiered_qualified=0\n");
    }
    void unused_negative(char const* path, error_code expected)
    {
        auto source{source_file(path)}; auto features{enabled_features()};
        auto prepared{prepare_runtime_from_wasm(source, u8"real-integer-compare-dual-unused-invalid", {}, features)};
        require(prepared.mod && prepared.mod->local_defined_function_vec_storage.size() == 2uz,
            "actual empty start followed by genuinely unused modern invalid body");
        ::std::unique_ptr<::llvm::TargetMachine> machine{::llvm::EngineBuilder{}.selectTarget()};
        require(bool(machine), "actual optional native target");
        dual::emission_policy policy{}; policy.target = machine.get(); policy.emit_integer_compare = true;
        ::uwvm2::validation::error::code_validation_error_impl error{};
        bool rejected{};
        try { auto refused{dual::compile<option>(*prepared.mod, {}, policy, error, &features)}; (void)refused; }
        catch(::fast_io::error const& actual)
        {
            if(actual.domain != ::fast_io::parse_domain_value || actual.code !=
                static_cast<::std::size_t>(static_cast<char8_t>(::fast_io::parse_code::invalid))) { throw; }
            rejected = true;
        }
        auto const code{prepared.mod->local_defined_function_vec_storage.index_unchecked(1uz).wasm_code_ptr};
        require(code != nullptr, "actual invalid function source metadata");
        auto const begin{reinterpret_cast<::std::byte const*>(code->body.expr_begin)};
        auto const end{reinterpret_cast<::std::byte const*>(code->body.code_end)};
        require(rejected && error.err_code == expected && error.err_curr >= begin && error.err_curr < end,
            "specific first-walk modern unused-body diagnostic inside the actual second expression");
        ::fast_io::print(::fast_io::out(), "REAL_DUAL_INTEGER_COMPARE unused_invalid_diagnostic=", static_cast<unsigned>(expected), " accepted=0\n");
    }
}
int main(int argc, char const* const* argv)
{
    require(argc == 7, "positive and5 official parsed modern unused-invalid Wasm files");
    require(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter(), "actual native LLVM target initialization");
    install_unexpected_traps(); positive(argv[1]);
    unused_negative(argv[2], error_code::numeric_operand_type_mismatch);
    unused_negative(argv[3], error_code::numeric_operand_type_mismatch);
    unused_negative(argv[4], error_code::numeric_operand_type_mismatch);
    unused_negative(argv[5], error_code::operand_stack_underflow);
    unused_negative(argv[6], error_code::numeric_operand_type_mismatch);
}
