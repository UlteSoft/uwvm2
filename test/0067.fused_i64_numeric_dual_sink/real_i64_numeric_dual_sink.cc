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
#if defined(UWVM2TEST_DUAL_I64_CACHED_RING)
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
# if !defined(UWVM_ENABLE_UWVM_INT_INSTRUCTION_REORDER) || !defined(UWVM2TEST_DUAL_I64_CACHED_RING)
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
            ::fast_io::print(::fast_io::err(), "real fused i64 numeric dual sink: ", ::fast_io::mnp::os_c_str(text), "\n");
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
    template<bool Wide>
    ::std::uint64_t run_native(dual::checked_module<option>& owned, runtime_module_t const& module,
                               ::std::size_t index, bool parameter = false)
    {
        require(owned.native_ir_available(index), "actual same-walk LLVM owner is available");
        auto fragment{owned.take_checked_ir(index)};
        require(fragment.emitted && fragment.llvm_module && fragment.llvm_context_holder &&
            !owned.native_ir_available(index), "physical LLVM ownership moves once, source slot loses availability");
        auto const name{full::details::get_llvm_wasm_function_name(module, static_cast<::std::uint_least32_t>(index))};
        auto const definition{fragment.llvm_module->getFunction(full::details::get_llvm_string_ref(name))};
        require(definition && !definition->isDeclaration() &&
            definition->getCallingConv() == full::details::get_llvm_jit_wasm_calling_conv(), "actual original typed LLVM ABI");
        require(full::details::verify_llvm_jit_module(*fragment.llvm_module, true), "mandatory actual final LLVM module verifier");
        ::std::unique_ptr<::llvm::TargetMachine> machine{::llvm::EngineBuilder{}.selectTarget()};
        require(bool(machine) && machine->createDataLayout() == fragment.llvm_module->getDataLayout(),
            "actual native target matches retained emission layout");
#if LLVM_VERSION_MAJOR >= 21
        require(machine->getTargetTriple() == fragment.llvm_module->getTargetTriple(), "actual LLVM target triple");
#else
        require(machine->getTargetTriple().str() == fragment.llvm_module->getTargetTriple(), "actual LLVM target triple");
#endif
        // The real engine dies BEFORE the context. The runtime/source outlives
        // both. This test resolves actual definitions, not arbitrary PC memory.
        auto context{::std::move(fragment.llvm_context_holder)};
        auto emitted{::std::move(fragment.llvm_module)};
        ::std::unique_ptr<::llvm::ExecutionEngine> engine{::llvm::EngineBuilder{
            ::std::unique_ptr<::llvm::Module>{emitted.release()}}.setEngineKind(::llvm::EngineKind::JIT).create(machine.release())};
        require(bool(engine), "real MCJIT engine from original one-walk physical SSA module");
        engine->finalizeObject();
        require(!name.empty() && name.size() <= static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()),
            "actual owned symbol size before end iterator formation");
        // [owned name.data() ... name.size() characters] one-past
        // [safe] checked extent BEFORE forming LLVM's actual std::string ABI name.
        native_symbol_name native_name{name.data(), name.data() + name.size()};
        auto const address{engine->getFunctionAddress(native_name)};
        require(address != 0u && address <= (::std::numeric_limits<::std::uintptr_t>::max)(), "actual finalized native typed entry");
        using bits_type = ::std::conditional_t<Wide, ::std::uint64_t, ::std::uint32_t>;
        if(parameter)
        {
            using entry_type = bits_type (UWVM2TEST_WASM_ABI*)(bits_type);
            return reinterpret_cast<entry_type>(static_cast<::std::uintptr_t>(address))(bits_type{0x80000000u});
        }
        using entry_type = bits_type (UWVM2TEST_WASM_ABI*)();
        return reinterpret_cast<entry_type>(static_cast<::std::uintptr_t>(address))();
    }
    ::std::uint64_t run_native_cached(dual::checked_module<option>& owned, runtime_module_t const& module, ::std::size_t index = 19uz)
    {
        require(index < module.local_defined_function_vec_storage.size(), "actual24 typed function index");
        require(owned.native_ir_available(index), "same-walk cached-local function LLVM owner");
        auto fragment{owned.take_checked_ir(index)};
        require(fragment.emitted && fragment.llvm_module && fragment.llvm_context_holder &&
            !owned.native_ir_available(index), "actual cached-local module moves once");
        auto const name{full::details::get_llvm_wasm_function_name(module, static_cast<::std::uint_least32_t>(index))};
        auto const definition{fragment.llvm_module->getFunction(full::details::get_llvm_string_ref(name))};
        require(definition && !definition->isDeclaration() && definition->arg_size() == 24uz &&
            definition->getReturnType()->isIntegerTy(64u) &&
            definition->getCallingConv() == full::details::get_llvm_jit_wasm_calling_conv(), "real exact24 typed LLVM ABI");
        for(auto const& argument: definition->args()) { require(argument.getType()->isIntegerTy(64u), "each actual i64 argument"); }
        require(full::details::verify_llvm_jit_module(*fragment.llvm_module, true), "actual cached-ring SSA verifier");
        ::std::unique_ptr<::llvm::TargetMachine> machine{::llvm::EngineBuilder{}.selectTarget()};
        require(bool(machine) && machine->createDataLayout() == fragment.llvm_module->getDataLayout(), "actual retained native layout");
#if LLVM_VERSION_MAJOR >= 21
        require(machine->getTargetTriple() == fragment.llvm_module->getTargetTriple(), "actual retained native triple");
#else
        require(machine->getTargetTriple().str() == fragment.llvm_module->getTargetTriple(), "actual retained native triple");
#endif
        auto context{::std::move(fragment.llvm_context_holder)}; auto emitted{::std::move(fragment.llvm_module)};
        ::std::unique_ptr<::llvm::ExecutionEngine> engine{::llvm::EngineBuilder{
            ::std::unique_ptr<::llvm::Module>{emitted.release()}}.setEngineKind(::llvm::EngineKind::JIT).create(machine.release())};
        require(bool(engine), "real actual24 typed native engine"); engine->finalizeObject();
        require(!name.empty() && name.size() <= static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()),
            "same owned cached-local symbol extent");
        // [owned complete name ...] one-past; size proved BEFORE forming this end iterator.
        // [safe                  ] ^^ native_name borrows only through its construction.
        native_symbol_name native_name{name.data(), name.data() + name.size()};
        auto const address{engine->getFunctionAddress(native_name)};
        require(address != 0u && address <= (::std::numeric_limits<::std::uintptr_t>::max)(), "actual resolved exact typed entry");
        using u64 = ::std::uint64_t;
        using entry_type = u64 (UWVM2TEST_WASM_ABI*)(u64,u64,u64,u64,u64,u64,u64,u64,u64,u64,u64,u64,
                                                     u64,u64,u64,u64,u64,u64,u64,u64,u64,u64,u64,u64);
        return reinterpret_cast<entry_type>(static_cast<::std::uintptr_t>(address))(
            1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,12u,13u,14u,15u,16u,17u,18u,19u,20u,21u,22u,23u,24u);
    }
    template<bool Wide>
    ::std::uint64_t run_native_eight(dual::checked_module<option>& owned, runtime_module_t const& module, ::std::size_t index)
    {
        require(index < module.local_defined_function_vec_storage.size(), "actual8 typed function index");
        require(owned.native_ir_available(index), "same-walk cached-local function LLVM owner");
        auto fragment{owned.take_checked_ir(index)};
        require(fragment.emitted && fragment.llvm_module && fragment.llvm_context_holder &&
            !owned.native_ir_available(index), "actual cached-local module moves once");
        auto const name{full::details::get_llvm_wasm_function_name(module, static_cast<::std::uint_least32_t>(index))};
        auto const definition{fragment.llvm_module->getFunction(full::details::get_llvm_string_ref(name))};
        require(definition && !definition->isDeclaration() && definition->arg_size() == 8uz &&
            definition->getReturnType()->isIntegerTy(Wide ? 64u : 32u) &&
            definition->getCallingConv() == full::details::get_llvm_jit_wasm_calling_conv(), "real exact8 typed LLVM ABI");
        for(auto const& argument: definition->args()) { require(argument.getType()->isIntegerTy(Wide ? 64u : 32u), "each actual i64 argument"); }
        require(full::details::verify_llvm_jit_module(*fragment.llvm_module, true), "actual cached-ring SSA verifier");
        ::std::unique_ptr<::llvm::TargetMachine> machine{::llvm::EngineBuilder{}.selectTarget()};
        require(bool(machine) && machine->createDataLayout() == fragment.llvm_module->getDataLayout(), "actual retained native layout");
#if LLVM_VERSION_MAJOR >= 21
        require(machine->getTargetTriple() == fragment.llvm_module->getTargetTriple(), "actual retained native triple");
#else
        require(machine->getTargetTriple().str() == fragment.llvm_module->getTargetTriple(), "actual retained native triple");
#endif
        auto context{::std::move(fragment.llvm_context_holder)}; auto emitted{::std::move(fragment.llvm_module)};
        ::std::unique_ptr<::llvm::ExecutionEngine> engine{::llvm::EngineBuilder{
            ::std::unique_ptr<::llvm::Module>{emitted.release()}}.setEngineKind(::llvm::EngineKind::JIT).create(machine.release())};
        require(bool(engine), "real actual8 typed native engine"); engine->finalizeObject();
        require(!name.empty() && name.size() <= static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()),
            "same owned cached-local symbol extent");
        // [owned complete name ...] one-past; size proved BEFORE forming this end iterator.
        // [safe                  ] ^^ native_name borrows only through its construction.
        native_symbol_name native_name{name.data(), name.data() + name.size()};
        auto const address{engine->getFunctionAddress(native_name)};
        require(address != 0u && address <= (::std::numeric_limits<::std::uintptr_t>::max)(), "actual resolved exact typed entry");
        using u64 = ::std::conditional_t<Wide, ::std::uint64_t, ::std::uint32_t>;
        using entry_type = u64 (UWVM2TEST_WASM_ABI*)(u64,u64,u64,u64,u64,u64,u64,u64);
        return reinterpret_cast<entry_type>(static_cast<::std::uintptr_t>(address))(
            1u,2u,3u,4u,5u,6u,7u,8u);
    }
    void positive(char const* path)
    {
#if defined(UWVM2TEST_DUAL_I64_PRELOAD_BATCH)
        scoped_preload_policy actual_preload_policy{};
#endif
        auto source{source_file(path)}; auto features{enabled_features()};
        auto prepared{prepare_runtime_from_wasm(source, u8"real-i64-numeric-dual-sink", {}, features)};
        require(prepared.mod && prepared.mod->local_defined_function_vec_storage.size() == 33uz,
            "actual all18 i64 functions,identity baseline,original heavy24/clean heavy8,modern start and five trap definitions");
        ::std::unique_ptr<::llvm::TargetMachine> machine{::llvm::EngineBuilder{}.selectTarget()};
        require(bool(machine), "actual target retained through synchronous typed emission");
        dual::emission_policy policy{}; policy.target = machine.get(); policy.emit_i64_numeric = true;
        ::uwvm2::validation::error::code_validation_error_impl error{};
        auto owned{dual::compile<option>(*prepared.mod, {}, policy, error, &features)};
        require(owned.typed_admission_complete() && error.err_code == error_code::ok &&
            owned.ring_artifact().local_funcs.size() == 33uz && owned.diagnostics().size() == 33uz,
            "every original typed function/fixup precedes actual factory seal");
        constexpr ::std::array<::std::uint64_t,18uz> expected{{0x0000000000000040ull, 0x0000000000000040ull, 0x0000000000000040ull, 0x8000000000000000ull, 0xffffffffffffffffull, 0x0000000000000000ull, 0xfffffffffffffffdull, 0x7fffffffffffffffull, 0x0000000000000000ull, 0x0000000000000001ull, 0x5555555555555555ull, 0xffffffffffffffffull, 0xaaaaaaaaaaaaaaaaull, 0x0000000000000002ull, 0xc000000000000000ull, 0x4000000000000000ull, 0x0000000000000001ull, 0x8000000000000000ull}};
        for(::std::size_t i{}; i != expected.size(); ++i)
        {
            auto const local{i + 1uz}; // <=18, both real runners recheck owner/index.
            require(owned.diagnostics()[local].reason == dual::native_unavailability::none &&
                owned.diagnostics()[local].i64_numeric_transitions == 1uz,
                "one original same-walk accepted numeric64 event");
            require(run_ring<true>(owned,*prepared.mod,local) == expected[i] &&
                run_native<true>(owned,*prepared.mod,local) == expected[i],
                "actual ring/native exact64 bits including zero counts,shiftmask and remainder overflow");
        }
        byte_vec parameters(24uz * 8uz);
        for(::std::size_t i{}; i != 24uz; ++i)
        {
            ::std::uint64_t const bits{i + 1uz};
            auto const offset{i * 8uz};
            require(offset <= parameters.size() && sizeof(bits) <= parameters.size() - offset,
                "exact tight24 original native-width parameter extent before address formation");
            // [owned24 tight i64 cells] end; checked subtraction BEFORE forming cell pointer.
            // [safe                 ] ^^ destination has the complete sizeof(bits) extent.
            ::fast_io::freestanding::my_memcpy(parameters.data() + offset, ::std::addressof(bits), sizeof(bits));
        }
        require(owned.diagnostics()[19uz].i64_numeric_transitions == 47uz &&
            run_ring<true>(owned,*prepared.mod,19uz,parameters) == 300ull &&
            run_native_cached(owned,*prepared.mod) == 300ull,
            "actual24 live i64 locals,identity barriers and47 numeric operations cross cached-ring slots and spills");
        // The ORIGINAL get24/add23 shape remains a separate actual body. Pending
        // combine state may prevent the clean add-reduce scanner from committing;
        // do not guess a native-decline oracle from the byte shape alone.
        require(run_ring<true>(owned,*prepared.mod,30uz,parameters) == 300ull,
            "original get24/add23 real typed ring result without changing the shape");
        bool const heavy24_ir{owned.native_ir_available(30uz)};
#if defined(UWVM2TEST_DUAL_I64_PRELOAD_BATCH)
        require(heavy24_ir && owned.diagnostics()[30uz].i64_numeric_transitions == 23uz,
            "actual original24 cached preload commits preserve all first events and 23 numeric SSA transitions");
#endif
        if(heavy24_ir)
        { require(run_native_cached(owned,*prepared.mod,30uz) == 300ull,
            "when original heavy24 first events are complete, real native result matches"); }
        else
        { require(owned.diagnostics()[30uz].reason != dual::native_unavailability::none,
            "original heavy24 declined partial IR is explicitly DATA, not invalid Wasm"); }
        byte_vec eight_parameters(8uz * 8uz);
        require(eight_parameters.size() <= parameters.size(), "owned exact8 prefix BEFORE copying parameters");
        // [owned24 input cells] ... end; [owned8 result cells] end
        // [safe] exact complete prefix size checked BEFORE copy; no cursor advances.
        ::fast_io::freestanding::my_memcpy(eight_parameters.data(), parameters.data(), eight_parameters.size());
        require(run_ring<true>(owned,*prepared.mod,31uz,eight_parameters) == 36ull,
            "clean get8/add7 heavy commit has the actual typed ring result");
        require(owned.native_ir_available(31uz) && owned.diagnostics()[31uz].i64_numeric_transitions == 7uz &&
            run_native_eight<true>(owned,*prepared.mod,31uz) == 36ull,
            "clean original heavy8 i64 commit publishes the same seven first numeric events and real native result");
        byte_vec narrow_parameters(8uz * 4uz);
        for(::std::size_t i{}; i != 8uz; ++i)
        {
            ::std::uint32_t const bits{static_cast<::std::uint32_t>(i + 1uz)};
            auto const offset{i * 4uz};
            require(offset <= narrow_parameters.size() && sizeof(bits) <= narrow_parameters.size() - offset,
                "actual8 i32 native ABI extent BEFORE forming destination cell");
            // [owned8 tight i32 cells] end; subtraction bounds prove this cell BEFORE +offset.
            // [safe] complete sizeof(bits) destination; no parser or byte cursor is reused.
            ::fast_io::freestanding::my_memcpy(narrow_parameters.data() + offset, ::std::addressof(bits), sizeof(bits));
        }
        require(owned.native_ir_available(32uz) && owned.diagnostics()[32uz].i32_numeric_transitions == 7uz &&
            run_ring<false>(owned,*prepared.mod,32uz,narrow_parameters) == 36ull &&
            run_native_eight<false>(owned,*prepared.mod,32uz) == 36ull,
            "clean original heavy8 i32 commit shares its first kernel events and actual exact32 native result");
        require(run_ring<true>(owned,*prepared.mod,20uz) == 64ull &&
            run_native<true>(owned,*prepared.mod,20uz) == 64ull,
            "defaultable i64 local has real zero SSA before clz");
        for(auto const local: {21uz,22uz,23uz,24uz})
        { require(!owned.native_ir_available(local), "legal GC/control/tail/call subset explicitly native-unavailable"); }
        require(run_ring<true>(owned,*prepared.mod,21uz) == 42ull &&
            run_ring<true>(owned,*prepared.mod,22uz) == 42ull &&
            run_ring<true>(owned,*prepared.mod,23uz) == 64ull,
            "legal modern GC,nested physical deadness,and real tail-call keep original ring validity");
        auto const start_result{interpreter_runner<option>::run(owned.ring_artifact().local_funcs.index_unchecked(24uz),
            prepared.mod->local_defined_function_vec_storage.index_unchecked(24uz), {}, nullptr, nullptr)};
        require(start_result.results.empty(), "genuine called-start checks all18 outputs plus cached24 and modern functions");
        for(::std::size_t local{25uz}; local != 30uz; ++local)
        {
            require(owned.native_ir_available(local), "unused but valid five trap IR definitions remain materialized");
            auto fragment{owned.take_checked_ir(local)};
            require(fragment.emitted && fragment.llvm_module &&
                full::details::verify_llvm_jit_module(*fragment.llvm_module, true),
                "actual trap IR definitions verify; this component does not claim trap execution");
        }
        auto old_policy{policy}; old_policy.emit_i64_numeric = false;
        auto old_slice{dual::compile<option>(*prepared.mod, {}, old_policy, error, &features)};
        require(old_slice.typed_admission_complete() && !old_slice.native_ir_available(1uz) &&
            run_ring<true>(old_slice,*prepared.mod,4uz) == 0x8000000000000000ull,
            "independent i64 policy false preserves previous native slice and full typed ring");
        auto quota_policy{policy}; quota_policy.module_operations = 0uz;
        auto quota{dual::compile<option>(*prepared.mod, {}, quota_policy, error, &features)};
        require(quota.typed_admission_complete() && !quota.native_ir_available(1uz) &&
            run_ring<true>(quota,*prepared.mod,9uz) == 0ull,
            "zero optional successful-module-IR quota still admits all original typed bodies");
        ::fast_io::print(::fast_io::out(), "REAL_DUAL_I64 numeric=18 cached_parameters=24 original_heavy24_ir=", heavy24_ir, " clean_heavy8_i32_i64_results=matched called_start=1 modern=3 ring_native_bits=matched trap_execution=unqualified\n");
    }
    void unused_negative(char const* path, error_code expected)
    {
        auto source{source_file(path)}; auto features{enabled_features()};
        auto prepared{prepare_runtime_from_wasm(source, u8"real-i64-numeric-dual-unused-invalid", {}, features)};
        require(prepared.mod && prepared.mod->local_defined_function_vec_storage.size() == 2uz,
            "actual empty start followed by genuinely unused modern invalid body");
        ::std::unique_ptr<::llvm::TargetMachine> machine{::llvm::EngineBuilder{}.selectTarget()};
        require(bool(machine), "actual optional native target");
        dual::emission_policy policy{}; policy.target = machine.get(); policy.emit_i64_numeric = true;
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
        ::fast_io::print(::fast_io::out(), "REAL_DUAL_I64 unused_invalid_diagnostic=", static_cast<unsigned>(expected), " accepted=0\n");
    }
}
int main(int argc, char const* const* argv)
{
    require(argc == 7, "positive and five official parsed modern unused-invalid Wasm files");
    require(!::llvm::InitializeNativeTarget() && !::llvm::InitializeNativeTargetAsmPrinter(), "actual native LLVM target init");
    install_unexpected_traps(); positive(argv[1]);
    unused_negative(argv[2], error_code::numeric_operand_type_mismatch);
    unused_negative(argv[3], error_code::numeric_operand_type_mismatch);
    unused_negative(argv[4], error_code::numeric_operand_type_mismatch);
    unused_negative(argv[5], error_code::operand_stack_underflow);
    unused_negative(argv[6], error_code::numeric_operand_type_mismatch);
}
