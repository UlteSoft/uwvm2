// Independent parser-copy, empty-module and initializer policy supplement.
// Parse and initialize every actual input with GC/function-references/exceptions enabled,
// then change ONLY the compiler/validator's exception policy. GC remains enabled. No retained source/type is edited.
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/validation/standard/wasm3/impl.h>
#if defined(UWVM2TEST_EH_RUNTIME_DIAGNOSTIC)
#include <uwvm2/runtime/lib/uwvm_runtime.h>
#endif
#if __has_include(<uwvm2/runtime/compiler/uwvm_int/compile_cu_from_lazy_validator/impl.h>)
#include <uwvm2/runtime/compiler/uwvm_int/compile_cu_from_lazy_validator/impl.h>
#define UWVM2TEST_EH_POLICY_INT_LAZY
#endif
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#if __has_include(<uwvm2/runtime/compiler/llvm_jit/compile_cu_from_lazy_validator/impl.h>)
#include <uwvm2/runtime/compiler/llvm_jit/compile_cu_from_lazy_validator/impl.h>
#define UWVM2TEST_EH_POLICY_LLVM_LAZY
#endif
#endif

namespace
{
    using namespace uwvm2test::uwvm_int_strict;
    namespace v3 = ::uwvm2::validation::standard::wasm3;
    using error_t = ::uwvm2::validation::error::code_validation_error_impl;
    using error_code = ::uwvm2::validation::error::code_validation_error_code;
    using subject_t = ::uwvm2::parser::wasm::base::wasm1p1_error_subject;
    unsigned observations{}, normative_mismatches{};
    bool initializer_only{}, disable_gc_too{}, runtime_fatal{}, runtime_fatal_llvm{};
    void require(bool condition, unsigned line)
    {
        if(!condition)
        {
            ::fast_io::print(::fast_io::err(), "FAIL EH two-phase source contract line ", ::fast_io::mnp::dec(line), "\n");
            ::fast_io::fast_terminate();
        }
    }
#define REQUIRE(x) require(static_cast<bool>(x), __LINE__)
    auto enabled_features()
    {
        auto out{make_wasm1p1_feature_parameter()};
        auto& policy{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(out)};
        policy.disable_gc = false;
        policy.disable_function_references = false;
        policy.disable_exceptions = false;
        policy.disable_extended_const = false;
        policy.disable_table_initializer = false;
        policy.disable_relaxed_simd = false;
        policy.disable_multi_memory = false;
        policy.disable_threads = false;
        policy.disable_tail_call = false;
        policy.disable_memory64 = false;
        policy.disable_table64 = false;
        return out;
    }
    auto const& codes()
    {
        auto const& parsed{::uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage};
        return []<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>(auto const& sections,
            ::uwvm2::utils::container::tuple<Fs...>) -> auto const&
        {
            return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(sections);
        }(parsed.sections, ::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features);
    }
    auto const& types()
    {
        auto const& parsed{::uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage};
        return []<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>(auto const& sections,
            ::uwvm2::utils::container::tuple<Fs...>) -> auto const&
        {
            return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>>(sections);
        }(parsed.sections, ::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features);
    }
    template<typename Types, typename RuntimeModule>
    void check_type_metadata_copy(Types const& original, RuntimeModule const& module)
    {
        // Exercise the real manual copy constructor and rebinding of owned signature carriers.
        auto const copied{original};
        REQUIRE(copied.types.size() == original.types.size());
        REQUIRE(copied.requires_gc == original.requires_gc);
        if constexpr(requires { original.requires_exceptions; module.type_section_storage.requires_exceptions; })
        {
            REQUIRE(copied.requires_exceptions == original.requires_exceptions);
            REQUIRE(module.type_section_storage.requires_exceptions == original.requires_exceptions);
            ::fast_io::print(::fast_io::out(), "EH_ROUNDTRIP metadata_available=1 type_requires_exceptions=",
                ::fast_io::mnp::dec(original.requires_exceptions),
                " table_requires_exceptions=", ::fast_io::mnp::dec(module.table_declarations_require_exceptions),
                " global_requires_exceptions=", ::fast_io::mnp::dec(module.global_declarations_require_exceptions),
                " element_requires_exceptions=", ::fast_io::mnp::dec(module.element_declarations_require_exceptions),
                " locals_require_exceptions=", ::fast_io::mnp::dec(module.code_declarations_require_exceptions), "\n");
        }
        else { ::fast_io::print(::fast_io::out(), "EH_ROUNDTRIP metadata_available=0\n"); }
    }
    template<typename Operation>
    void observe(char const* file, char const* mode, bool exceptions_enabled, bool expected_valid,
        subject_t expected_subject, error_t& error, Operation operation)
    {
        bool rejected{};
        try { operation(); }
        catch(::fast_io::error const& failure)
        {
            // Allocation/native/LLVM errors are NOT policy rejection evidence.
            if(failure.domain != ::fast_io::parse_domain_value || failure.code !=
                static_cast<::std::size_t>(static_cast<char8_t>(::fast_io::parse_code::invalid))) { throw; }
            rejected = true;
        }
        auto const required{error.err_code == error_code::wasm1p1_feature_required};
        auto const actual_valid{!rejected && error.err_code == error_code::ok};
        auto const expected_feature{!exceptions_enabled && disable_gc_too && types().requires_gc ?
            ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::gc :
            ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::exceptions};
        bool const diagnostic_ok{expected_valid || (rejected && required &&
            error.err_selectable.wasm1p1_feature_required.feature == expected_feature &&
            error.err_selectable.wasm1p1_feature_required.subject == expected_subject)};
        bool const matches{actual_valid == expected_valid && diagnostic_ok};
        ++observations;
        normative_mismatches += !matches;
        ::fast_io::print(::fast_io::out(), "EH_POLICY file=", ::fast_io::mnp::os_c_str(file), " mode=", ::fast_io::mnp::os_c_str(mode),
            " parse_exceptions=1 init_exceptions=1 validation_gc=", ::fast_io::mnp::dec(exceptions_enabled || !disable_gc_too),
            " validation_exceptions=", ::fast_io::mnp::dec(exceptions_enabled),
            " type_requires_gc=", ::fast_io::mnp::dec(types().requires_gc),
            " actual_valid=", ::fast_io::mnp::dec(actual_valid),
            " expected_valid=", ::fast_io::mnp::dec(expected_valid),
            " err_code=", ::fast_io::mnp::dec(static_cast<unsigned>(error.err_code)),
            " required_feature=", ::fast_io::mnp::dec(required ?
                static_cast<unsigned>(error.err_selectable.wasm1p1_feature_required.feature) : 0u),
            " required_subject=", ::fast_io::mnp::dec(required ?
                static_cast<unsigned>(error.err_selectable.wasm1p1_feature_required.subject) : 0u),
            " expected_subject=", ::fast_io::mnp::dec(static_cast<unsigned>(expected_subject)),
            " matches=", ::fast_io::mnp::dec(matches), "\n");
    }
    void run(byte_vec const& source, char const* file, bool accepted_without_exceptions, subject_t expected_subject,
        byte_vec const* provider = nullptr)
    {
        // The prepared owner, parsed allocation and feature parameters remain live until every
        // pure/fused product below is destroyed. No synthetic runtime provenance is substituted.
        auto enabled{enabled_features()};
        // Provider and execute bytes remain in their actual caller-owned allocations until the
        // linked runtime and every compiled product are retired. All modules parse/init with GC and exceptions on.
        auto prepared{provider == nullptr ?
            prepare_runtime_from_wasm(source, u8"eh-two-phase-policy", {}, enabled) :
            prepare_runtime_from_wasm(source, u8"eh-two-phase-policy",
                {{provider, u8"eh-policy-provider", ::std::addressof(enabled)}}, enabled)};
        REQUIRE(prepared.mod != nullptr);
        REQUIRE(prepared.mod->type_section_storage.requires_gc == types().requires_gc);
        check_type_metadata_copy(types(), *prepared.mod);
        ::fast_io::print(::fast_io::out(), "EH_METADATA file=", ::fast_io::mnp::os_c_str(file),
            " actual_type_requires_gc=", ::fast_io::mnp::dec(types().requires_gc), "\n");
        if(runtime_fatal)
        {
#if defined(UWVM2TEST_EH_RUNTIME_DIAGNOSTIC)
            namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
            mode::global_runtime_mode = mode::runtime_mode_t::full_compile;
            if(runtime_fatal_llvm)
            {
#if !defined(UWVM_DISABLE_JIT)
                mode::global_runtime_compiler = mode::runtime_compiler_t::llvm_jit_only;
                mode::global_runtime_llvm_jit_cache_path_mode = mode::runtime_llvm_jit_cache_path_mode_t::disabled;
#else
                ::fast_io::print(::fast_io::err(), "EH_RUNTIME_DIAGNOSTIC unavailable_llvm_profile=1\n");
                ::fast_io::fast_terminate();
#endif
            }
            else
            {
#if !defined(UWVM_DISABLE_INT)
                mode::global_runtime_compiler = mode::runtime_compiler_t::uwvm_interpreter_only;
#else
                ::fast_io::print(::fast_io::err(), "EH_RUNTIME_DIAGNOSTIC unavailable_int_profile=1\n");
                ::fast_io::fast_terminate();
#endif
            }
            mode::global_runtime_compile_threads = 0;
            mode::runtime_compile_threads_existed = true;
            ::uwvm2::uwvm::utils::ansies::put_color = true;
            // Only after genuine enabled parsing/initialization, tighten the actual wf policy
            // looked up by the public runtime. Do not alter bytes, spans or declaration metadata.
            auto& policy{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(
                ::uwvm2::uwvm::wasm::storage::execute_wasm.wasm_parameter.binfmt1_para)};
            policy.disable_exceptions = true;
            policy.disable_gc = disable_gc_too;
            ::fast_io::print(::fast_io::err(), "EH_RUNTIME_DIAGNOSTIC invoking_real_full_prepare=1\n");
            // Actual full admission -> fused compiler -> real fatal formatter. Expected failures
            // terminate here; the keeper must inspect raw output/status, offset and memory marker.
            REQUIRE(::uwvm2::runtime::lib::full_compile_prepare_host_api());
            ::fast_io::print(::fast_io::out(), "EH_RUNTIME_DIAGNOSTIC returned=1 expected_valid=",
                ::fast_io::mnp::dec(accepted_without_exceptions), "\n");
            normative_mismatches += !accepted_without_exceptions;
            return;
#else
            ::fast_io::print(::fast_io::err(), "EH_RUNTIME_DIAGNOSTIC unavailable_actual_runtime_link=1\n");
            ::fast_io::fast_terminate(); // An unavailable macro/profile is not a qualified diagnostic test.
#endif
        }
        if(initializer_only)
        {
            auto policy{enabled};
            ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(policy).disable_exceptions = true;
            ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(policy).disable_gc = disable_gc_too;
            // This is the genuine initializer's cold admission path, after enabled parsing/initialization.
            // A required feature must terminate with the ordinary colored fast_io fatal diagnostic.
            auto const& parsed{::uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage};
            ::uwvm2::uwvm::runtime::initializer::details::enforce_wasm1p1_initializer_feature_parameters(parsed, policy);
            ::fast_io::print(::fast_io::out(), "EH_INITIALIZER returned=1 expected_valid=",
                ::fast_io::mnp::dec(accepted_without_exceptions), "\n");
            normative_mismatches += !accepted_without_exceptions;
            return;
        }
        for(bool exceptions_enabled : {true, false})
        {
            auto policy{enabled};
            ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(policy).disable_exceptions = !exceptions_enabled;
            ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(policy).disable_gc =
                disable_gc_too && !exceptions_enabled;
            auto const expected_valid{exceptions_enabled || accepted_without_exceptions};
            // A genuine deep copy retains module-level declaration requirements and owns rebound type carriers.
            auto const parsed{::uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage};
            auto pure = [&](bool facade)
            {
                if(codes().codes.empty())
                {
                    // Pure validate_code is a function-body API. A module without bodies has no such call.
                    // Do not synthesize a function or count this explicit skip as a validation observation.
                    ::fast_io::print(::fast_io::out(), "EH_SKIP file=", ::fast_io::mnp::os_c_str(file),
                        " pure_no_local_bodies=1\n");
                    return;
                }
                error_t error{};
                observe(file, facade ? "pure-runtime-policy" : "pure-core3", exceptions_enabled, expected_valid, expected_subject, error, [&]
                {
                    auto const& list{codes().codes};
                    // The final _start is the only function validated here. In the unused-other-local
                    // probe it is empty; module-wide policy metadata must still preserve the earlier local declaration.
                    for(::std::size_t index{list.size() - 1uz}; index != list.size(); ++index)
                    {
                        // [owned code vector ... index ... end)
                        // [safe                            ] index < size proves this body borrow.
                        auto const& body{list.index_unchecked(index).body};
                        auto const imported{prepared.mod->imported_function_vec_storage.size()};
                        if(index > ::std::numeric_limits<::std::size_t>::max() - imported) { ::fast_io::fast_terminate(); }
                        // [actual parser-owned expression ... code_end]
                        // [safe                                      ] endpoints remain in the source owner; no cursor moves here.
                        auto const begin{reinterpret_cast<::std::byte const*>(body.expr_begin)};
                        auto const end{reinterpret_cast<::std::byte const*>(body.code_end)};
                        if(facade) { v3::validate_code_with_runtime_policy(parsed, imported + index, begin, end, error, policy); }
                        else { v3::validate_code(v3::wasm3_code_version{}, parsed, imported + index, begin, end, error, policy); }
                    }
                });
            };
            pure(false); pure(true);
            error_t integrated{};
            observe(file, "uwvm-int-full", exceptions_enabled, expected_valid, expected_subject, integrated, [&]
            {
                optable::compile_option options;
                auto product{compiler::compile_all_from_uwvm_single_func<k_test_byref_opt>(
                    *prepared.mod, options, integrated, &policy)};
                (void)product;
            });
#if defined(UWVM2TEST_EH_POLICY_INT_LAZY)
            error_t lazy_error{};
            observe(file, "uwvm-int-lazy-materialization", exceptions_enabled, expected_valid, expected_subject, lazy_error, [&]
            {
                namespace lazy = ::uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator;
                optable::compile_option options;
                lazy::lazy_split_config config{};
                config.eu_policy = lazy::lazy_execution_unit_split_policy_t::function_only;
                auto storage{lazy::initialize_lazy_module_storage(*prepared.mod, options, lazy_error, config, &policy)};
                lazy::lazy_compile_options compile;
                compile.compile_options = options;
                compile.validator_module_storage = &parsed;
                compile.validator_feature_parameter = &policy;
                // [actual lazy compile-unit vector ... end)
                // [safe                                   ] each index is bounded by the initialized module owner.
                for(::std::size_t index{}; index != storage.compile_units.size(); ++index)
                { lazy::compile_cu_from_lazy_validator<k_test_byref_opt>(*prepared.mod, storage, compile, index, lazy_error); }
            });
#endif
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
            error_t jit_error{};
            observe(file, "llvm-jit-full", exceptions_enabled, expected_valid, expected_subject, jit_error, [&]
            {
                namespace jit = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
                jit::compile_option options;
                options.validator_feature_parameter = &policy;
                options.verify_llvm_jit_ir = true;
                auto product{jit::compile_all_from_uwvm(*prepared.mod, options, jit_error, 0)};
                REQUIRE(product.llvm_jit_module.emitted);
                REQUIRE(!::llvm::verifyModule(*product.llvm_jit_module.llvm_module, &::llvm::errs()));
            });
#if defined(UWVM2TEST_EH_POLICY_LLVM_LAZY)
            error_t lazy_jit_error{};
            observe(file, "llvm-jit-lazy-materialization", exceptions_enabled, expected_valid, expected_subject, lazy_jit_error, [&]
            {
                namespace lazy = ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator;
                lazy::compile_option options;
                options.validator_feature_parameter = &policy;
                options.verify_llvm_jit_ir = true;
                auto storage{lazy::initialize_lazy_module_storage(*prepared.mod, options, lazy_jit_error)};
                lazy::lazy_compile_options compile;
                compile.compile_options = options;
                compile.validator_module_storage = &parsed;
                compile.validator_feature_parameter = &policy;
                for(::std::size_t index{}; index != storage.compile_units.size(); ++index)
                { lazy::compile_cu_from_lazy_validator(*prepared.mod, storage, compile, index, lazy_jit_error); }
            });
#endif
#endif
        }
    }
}

int main(int argc, char const* const* argv)
{
    // Optional fixed-name provider: --provider actual.oracle.wasm; then the same triples.
    int first{1};
    if(argc > first && ::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[first])} == "--both-disabled")
    { disable_gc_too = true; ++first; }
    if(argc > first)
    {
        auto const option{::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[first])}};
        if(option == "--initializer-only") { initializer_only = true; ++first; }
        else if(option == "--runtime-fatal-int" || option == "--runtime-fatal-llvm")
        { runtime_fatal = true; runtime_fatal_llvm = option == "--runtime-fatal-llvm"; ++first; }
    }
    byte_vec provider;
    byte_vec const* provider_ptr{};
    if(argc > first && ::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[first])} == "--provider")
    {
        REQUIRE(argc >= first + 5);
        ::fast_io::native_file_loader input{::fast_io::mnp::os_c_str(argv[first + 1]), ::fast_io::open_mode::in};
        REQUIRE(input.size() >= 8uz && input.size() <= 65536uz);
        provider.resize(input.size());
        // [RAII file bytes ... end) / [owned provider bytes ... end)
        // [safe                                                   ] equal lengths bound this copy; source outlives every run.
        ::std::memcpy(provider.data(), input.data(), input.size());
        provider_ptr = ::std::addressof(provider);
        first += 2;
    }
    REQUIRE(argc >= first + 3 && (argc - first) % 3 == 0);
    for(int index{first}; index < argc; index += 3)
    {
        ::fast_io::cstring_view const expected{::fast_io::mnp::os_c_str(argv[index])};
        ::fast_io::cstring_view const site{::fast_io::mnp::os_c_str(argv[index + 1])};
        REQUIRE(expected == "accept" || expected == "reject");
        REQUIRE(site == "none" || site == "table" || site == "global" || site == "element" || site == "function" || site == "local" || site == "tag");
        REQUIRE((expected == "accept") == (site == "none"));
        auto const required_subject{site == "table" ? subject_t::table_type :
            site == "global" ? subject_t::global_type : site == "element" ? subject_t::element_segment :
            site == "local" ? subject_t::local_type : site == "tag" ? subject_t::tag_type : subject_t::function_type};
        ::fast_io::native_file_loader input{::fast_io::mnp::os_c_str(argv[index + 2]), ::fast_io::open_mode::in};
        REQUIRE(input.size() >= 8uz && input.size() <= 65536uz);
        byte_vec source;
        source.resize(input.size());
        // [RAII loader bytes ... end) / [owned source bytes ... end)
        // [safe                                                   ] equal actual lengths bound this copy.
        ::std::memcpy(source.data(), input.data(), input.size());
        run(source, argv[index + 2], expected == "accept", required_subject, provider_ptr);
    }
    ::fast_io::print(::fast_io::out(), "EH_ROUNDTRIP_TWO_PHASE observations=", ::fast_io::mnp::dec(observations),
        " normative_mismatches=", ::fast_io::mnp::dec(normative_mismatches), "\n");
    // Before-fix counterexamples remain failing regressions, never qualification PASS.
    return normative_mismatches != 0u;
}
