// Parse and initialize every actual input with GC/function-references/exceptions enabled,
// then change ONLY the compiler/validator's exception policy. GC remains enabled. No retained source/type is edited.
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/validation/standard/wasm3/impl.h>
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
        bool const diagnostic_ok{expected_valid || (rejected && required &&
            error.err_selectable.wasm1p1_feature_required.feature ==
                ::uwvm2::parser::wasm::base::wasm1p1_feature_kind::exceptions &&
            error.err_selectable.wasm1p1_feature_required.subject == expected_subject)};
        bool const matches{actual_valid == expected_valid && diagnostic_ok};
        ++observations;
        normative_mismatches += !matches;
        ::fast_io::print(::fast_io::out(), "EH_POLICY file=", ::fast_io::mnp::os_c_str(file), " mode=", ::fast_io::mnp::os_c_str(mode),
            " parse_exceptions=1 init_exceptions=1 validation_gc=1 validation_exceptions=", ::fast_io::mnp::dec(exceptions_enabled),
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
        REQUIRE(!prepared.mod->tag_section_present && prepared.mod->imported_tag_vec_storage.empty());
        REQUIRE(codes().codes.size() == 1uz);
        REQUIRE(prepared.mod->type_section_storage.requires_gc == types().requires_gc);
        ::fast_io::print(::fast_io::out(), "EH_METADATA file=", ::fast_io::mnp::os_c_str(file),
            " actual_type_requires_gc=", ::fast_io::mnp::dec(types().requires_gc), "\n");
        for(bool exceptions_enabled : {true, false})
        {
            auto policy{enabled};
            ::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(policy).disable_exceptions = !exceptions_enabled;
            REQUIRE(!::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(policy).disable_gc);
            auto const expected_valid{exceptions_enabled || accepted_without_exceptions};
            auto const& parsed{::uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage};
            auto pure = [&](bool facade)
            {
                error_t error{};
                observe(file, facade ? "pure-runtime-policy" : "pure-core3", exceptions_enabled, expected_valid, expected_subject, error, [&]
                {
                    auto const& list{codes().codes};
                    for(::std::size_t index{}; index != list.size(); ++index)
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
    byte_vec provider;
    byte_vec const* provider_ptr{};
    if(argc > 1 && ::std::string_view{argv[1]} == "--provider")
    {
        REQUIRE(argc >= 6);
        ::fast_io::native_file_loader input{::fast_io::mnp::os_c_str(argv[2]), ::fast_io::open_mode::in};
        REQUIRE(input.size() >= 8uz && input.size() <= 65536uz);
        provider.resize(input.size());
        // [RAII file bytes ... end) / [owned provider bytes ... end)
        // [safe                                                   ] equal lengths bound this copy; source outlives every run.
        ::std::memcpy(provider.data(), input.data(), input.size());
        provider_ptr = ::std::addressof(provider);
        first = 3;
    }
    REQUIRE(argc >= first + 3 && (argc - first) % 3 == 0);
    for(int index{first}; index < argc; index += 3)
    {
        ::std::string_view const expected{argv[index]}, site{argv[index + 1]};
        REQUIRE(expected == "accept" || expected == "reject");
        REQUIRE(site == "none" || site == "table" || site == "global" || site == "element" || site == "function" || site == "local");
        REQUIRE((expected == "accept") == (site == "none"));
        auto const required_subject{site == "table" ? subject_t::table_type :
            site == "global" ? subject_t::global_type : site == "element" ? subject_t::element_segment :
            site == "local" ? subject_t::local_type : subject_t::function_type};
        ::fast_io::native_file_loader input{::fast_io::mnp::os_c_str(argv[index + 2]), ::fast_io::open_mode::in};
        REQUIRE(input.size() >= 8uz && input.size() <= 65536uz);
        byte_vec source;
        source.resize(input.size());
        // [RAII loader bytes ... end) / [owned source bytes ... end)
        // [safe                                                   ] equal actual lengths bound this copy.
        ::std::memcpy(source.data(), input.data(), input.size());
        run(source, argv[index + 2], expected == "accept", required_subject, provider_ptr);
    }
    ::fast_io::print(::fast_io::out(), "EH_TWO_PHASE observations=", ::fast_io::mnp::dec(observations),
        " normative_mismatches=", ::fast_io::mnp::dec(normative_mismatches), "\n");
    // Before-fix counterexamples remain failing regressions, never qualification PASS.
    return normative_mismatches != 0u;
}
