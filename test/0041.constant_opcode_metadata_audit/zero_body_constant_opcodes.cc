// Genuine two-stage empty-code module admission: parse/init enabled, tighten
// only the validation/compilation policy. No function or byte span is fabricated.
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/uwvm/runtime/validator/validate.h>
#if __has_include(<uwvm2/runtime/compiler/uwvm_int/compile_cu_from_lazy_validator/impl.h>)
#include <uwvm2/runtime/compiler/uwvm_int/compile_cu_from_lazy_validator/impl.h>
#define UWVM2TEST_ZERO_BODY_INT_LAZY
#endif
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#if __has_include(<uwvm2/runtime/compiler/llvm_jit/compile_cu_from_lazy_validator/impl.h>)
#include <uwvm2/runtime/compiler/llvm_jit/compile_cu_from_lazy_validator/impl.h>
#define UWVM2TEST_ZERO_BODY_LLVM_LAZY
#endif
#endif

namespace
{
    using namespace uwvm2test::uwvm_int_strict;
    using error_t = ::uwvm2::validation::error::code_validation_error_impl;
    using error_code = ::uwvm2::validation::error::code_validation_error_code;
    using subject_t = ::uwvm2::parser::wasm::base::wasm1p1_error_subject;
    using feature_t = ::uwvm2::parser::wasm::base::wasm1p1_feature_kind;
    void select_feature(auto& policy, feature_t feature, bool enabled)
    {
        switch(feature)
        {
            case feature_t::gc: policy.disable_gc = !enabled; break;
            case feature_t::exceptions: policy.disable_exceptions = !enabled; break;
            case feature_t::function_references: policy.disable_function_references = !enabled; break;
            case feature_t::reference_types: policy.disable_reference_types = !enabled; break;
            case feature_t::simd: policy.disable_simd = !enabled; break;
            default: ::fast_io::fast_terminate();
        }
    }
    unsigned observations{}, normative_mismatches{};
    byte_vec provider_source{};
    bool has_provider{};
    void require(bool condition, unsigned line)
    {
        if(!condition)
        {
            ::fast_io::print(::fast_io::err(), "FAIL zero-body source contract line ", ::fast_io::mnp::dec(line), "\n");
            ::fast_io::fast_terminate();
        }
    }
#define REQUIRE(x) require(static_cast<bool>(x), __LINE__)
    auto enabled_features()
    {
        auto out{make_wasm1p1_feature_parameter()};
        auto& policy{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(out)};
        policy.disable_simd = false;
        policy.disable_reference_types = false;
        policy.disable_multi_value = false;
        policy.controllable_allow_multi_result_vector = false;
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
    template<typename Parsed>
    void check_empty_code(Parsed const& parsed)
    {
        auto const& codes{[]<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>(auto const& sections,
            ::uwvm2::utils::container::tuple<Fs...>) -> auto const&
        {
            return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                ::uwvm2::parser::wasm::standard::wasm1::features::code_section_storage_t<Fs...>>(sections);
        }(parsed.sections, ::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features)};
        REQUIRE(codes.codes.empty());
    }
    template<typename Parsed, typename Runtime>
    void observe_opcode_metadata(Parsed const& original, Parsed const& copied, Runtime const& runtime, unsigned expected_mask)
    {
        []<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>(auto const& first, auto const& second,
            auto const& actual, unsigned expected, ::uwvm2::utils::container::tuple<Fs...>)
        {
            namespace w = ::uwvm2::parser::wasm;
            namespace t = w::standard::wasm3::type;
            auto const& tables{w::concepts::operation::get_first_type_in_tuple<w::standard::wasm1::features::table_section_storage_t<Fs...>>(first.sections)};
            auto const& globals{w::concepts::operation::get_first_type_in_tuple<w::standard::wasm1::features::global_section_storage_t<Fs...>>(first.sections)};
            auto const& elements{w::concepts::operation::get_first_type_in_tuple<w::standard::wasm1::features::element_section_storage_t<Fs...>>(first.sections)};
            auto const& data{w::concepts::operation::get_first_type_in_tuple<w::standard::wasm1::features::data_section_storage_t<Fs...>>(first.sections)};
            auto const& types{w::concepts::operation::get_first_type_in_tuple<w::standard::wasm1::features::type_section_storage_t<Fs...>>(first.sections)};
            // Independent official-encoding witness over real decoded records;
            // this test never calls the production requirement classifier/helper.
            struct seen_t { bool required{};unsigned value{};subject_t site{}; } seen[5]{};
            auto note{[&](unsigned index, unsigned value, subject_t site)
            {
                REQUIRE(index < 5u); // Checked fixed scalar array access; no bytecode cursor.
                if(!seen[index].required) { seen[index] = {true, value, site}; }
            }};
            auto observe_expression{[&](auto const& expr, subject_t location)
            {
                for(auto const& instruction : expr.opcodes)
                {
                    unsigned const byte{static_cast<unsigned>(instruction.opcode)};
                    if(byte == 251u) { note(0u, byte, location); }
                    else if(byte == 253u) { note(4u, byte, location); }
                    else if(byte == 210u) { note(3u, byte, location); }
                    else if(byte == 208u)
                    {
                        note(3u, byte, location);
                        auto const heap{instruction.storage.ref_null_heap};
                        if(heap >= 0)
                        {
                            auto const index{static_cast<::std::uint_least64_t>(heap)};
                            REQUIRE(index < types.types.size());
                            bool function{types.core3_context.records.empty()};
                            if(!function)
                            {
                                REQUIRE(index < types.core3_context.records.size());
                                // REQUIRE bounds the real decoded context record before access.
                                function = types.core3_context.records.index_unchecked(static_cast<::std::size_t>(index)).kind == t::composite_kind::function;
                            }
                            note(function ? 2u : 0u, byte, location);
                        }
                        else
                        {
                            switch(static_cast<t::abstract_heap_type>(heap))
                            {
                                case t::abstract_heap_type::func: case t::abstract_heap_type::extern_: break;
                                case t::abstract_heap_type::exn: case t::abstract_heap_type::noexn: note(1u, byte, location); break;
                                case t::abstract_heap_type::any: case t::abstract_heap_type::eq: case t::abstract_heap_type::i31:
                                case t::abstract_heap_type::struct_: case t::abstract_heap_type::array: case t::abstract_heap_type::none:
                                case t::abstract_heap_type::nofunc: case t::abstract_heap_type::noextern: note(0u, byte, location); break;
                                default: REQUIRE(false);
                            }
                        }
                    }
                }
            }};
            // Original successful module-section order determines each feature's first actual opcode/site.
            for(auto const& expr : tables.initializers) { observe_expression(expr, subject_t::table_type); }
            for(auto const& global : globals.local_globals) { observe_expression(global.expr, subject_t::global_type); }
            for(auto const& element : elements.elems)
            {
                observe_expression(element.storage.segment.expr, subject_t::element_segment);
                for(auto const& expr : element.storage.segment.vec_expr) { observe_expression(expr, subject_t::element_segment); }
            }
            for(auto const& segment : data.datas)
            { if(static_cast<unsigned>(segment.type) != 1u) { observe_expression(segment.storage.segment.expr, subject_t::data_segment); } }
            unsigned const mask{(seen[0].required ? 1u : 0u) | (seen[1].required ? 2u : 0u) | (seen[2].required ? 4u : 0u) |
                (seen[3].required ? 8u : 0u) | (seen[4].required ? 16u : 0u)};
            REQUIRE(mask == expected);
            if constexpr(requires { globals.constant_expression_opcode_requirements; actual.constant_expression_opcode_requirements; })
            {
                auto const& cg{w::concepts::operation::get_first_type_in_tuple<w::standard::wasm1::features::global_section_storage_t<Fs...>>(second.sections)};
                auto equal_record{[&](auto const& first_record, auto const& copied_record, auto const& runtime_record, unsigned index)
                {
                    REQUIRE(index < 5u);
                    auto const& expected_record{seen[index]};
                    REQUIRE(first_record.required == expected_record.required && copied_record.required == expected_record.required &&
                        runtime_record.required == expected_record.required);
                    if(expected_record.required)
                    {
                        REQUIRE(first_record.value == expected_record.value && copied_record.value == expected_record.value && runtime_record.value == expected_record.value);
                        REQUIRE(first_record.subject == expected_record.site && copied_record.subject == expected_record.site && runtime_record.subject == expected_record.site);
                    }
                }};
                auto const& r{globals.constant_expression_opcode_requirements};auto const& c{cg.constant_expression_opcode_requirements};
                auto const& native{actual.constant_expression_opcode_requirements};
                equal_record(r.gc,c.gc,native.gc,0u);equal_record(r.exceptions,c.exceptions,native.exceptions,1u);
                equal_record(r.function_references,c.function_references,native.function_references,2u);
                equal_record(r.reference_types,c.reference_types,native.reference_types,3u);equal_record(r.simd,c.simd,native.simd,4u);
                ::fast_io::print(::fast_io::out(), "CONSTANT_OPCODE_METADATA available=1 mask=", ::fast_io::mnp::dec(mask),
                    " copied_equal=1 runtime_projection_equal=1 classifier_called=0\n");
            }
            else
            { ::fast_io::print(::fast_io::out(), "CONSTANT_OPCODE_METADATA available=0 BEFORE_no_new_fields mask=",
                ::fast_io::mnp::dec(mask), " projection_proved=0 classifier_called=0\n"); }
        }(original,copied,runtime,expected_mask,::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features);
    }
    template<typename Operation>
    void observe_compiler(char const* file, char const* mode, bool enabled, bool expected_valid,
        feature_t disabled_feature, subject_t expected_subject, unsigned expected_value, error_t& error, Operation operation)
    {
        bool rejected{};
        try { operation(); }
        catch(::fast_io::error const& failure)
        {
            // Only the actual parser/validator invalid domain is policy evidence.
            // Allocation, native, LLVM and unrelated parse errors cannot qualify.
            if(failure.domain != ::fast_io::parse_domain_value || failure.code !=
                static_cast<::std::size_t>(static_cast<char8_t>(::fast_io::parse_code::invalid))) { throw; }
            rejected = true;
        }
        bool const required{error.err_code == error_code::wasm1p1_feature_required};
        bool const actual_valid{!rejected && error.err_code == error_code::ok};
        bool const diagnostic_ok{expected_valid || (rejected && required &&
            error.err_selectable.wasm1p1_feature_required.feature == disabled_feature &&
            error.err_selectable.wasm1p1_feature_required.subject == expected_subject &&
            error.err_selectable.wasm1p1_feature_required.value == expected_value)};
        bool const matches{actual_valid == expected_valid && diagnostic_ok};
        ++observations;
        normative_mismatches += !matches;
        ::fast_io::print(::fast_io::out(), "ZERO_BODY_POLICY file=", ::fast_io::mnp::os_c_str(file),
            " mode=", ::fast_io::mnp::os_c_str(mode), " enabled=", ::fast_io::mnp::dec(enabled),
            " disabled_feature=", ::fast_io::mnp::dec(static_cast<unsigned>(disabled_feature)),
            " actual_valid=", ::fast_io::mnp::dec(actual_valid), " expected_valid=", ::fast_io::mnp::dec(expected_valid),
            " err_code=", ::fast_io::mnp::dec(static_cast<unsigned>(error.err_code)),
            " required_feature=", ::fast_io::mnp::dec(required ? static_cast<unsigned>(error.err_selectable.wasm1p1_feature_required.feature) : 0u),
            " required_subject=", ::fast_io::mnp::dec(required ? static_cast<unsigned>(error.err_selectable.wasm1p1_feature_required.subject) : 0u),
            " required_value=", ::fast_io::mnp::dec(required ? error.err_selectable.wasm1p1_feature_required.value : 0u),
            " matches=", ::fast_io::mnp::dec(matches), " guest_execution=0\n");
    }
    void observe_parser_bool(char const* file, char const* mode, bool enabled, bool expected_valid, bool actual_valid)
    {
        bool const matches{actual_valid == expected_valid};
        ++observations;
        normative_mismatches += !matches;
        ::fast_io::print(::fast_io::out(), "ZERO_BODY_POLICY file=", ::fast_io::mnp::os_c_str(file),
            " mode=", ::fast_io::mnp::os_c_str(mode), " enabled=", ::fast_io::mnp::dec(enabled),
            " actual_valid=", ::fast_io::mnp::dec(actual_valid), " expected_valid=", ::fast_io::mnp::dec(expected_valid),
            " matches=", ::fast_io::mnp::dec(matches), " error_object_observed=0 guest_execution=0\n");
    }
    void run(byte_vec const& source, char const* file, feature_t disabled_feature,
        bool accepted_without_feature, unsigned expected_mask, subject_t extended_subject, unsigned extended_opcode)
    {
        subject_t const expected_subject{extended_subject};
        unsigned const expected_value{extended_opcode};
        auto enabled_policy{enabled_features()};
        // FIRST: own real parser result, stricter pure admission before any
        // instance initialization/provider linking. Preserve actual registries.
        auto const wasm_registry_size{::uwvm2::uwvm::wasm::storage::all_module.size()};
        auto const runtime_registry_size{::uwvm2::uwvm::runtime::storage::active_runtime_registry().size()};
        REQUIRE(source.size() >= 8uz && source.size() <= 65536uz);
        auto const source_begin{source.data()};
        // [owned nonempty source ...] one-past: verified size bounds this advance.
        auto const source_end{source_begin + source.size()};
        ::uwvm2::parser::wasm::base::error_impl parse_error{};
        auto const before_effects{::uwvm2::uwvm::wasm::feature::binfmt_ver1_handler(source_begin, source_end, parse_error, enabled_policy)};
        check_empty_code(before_effects);
        for(bool enabled : {true, false})
        {
            auto policy{enabled_policy};
            auto& strict{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(policy)};
            select_feature(strict, disabled_feature, enabled);
            error_t admission_error{};
            observe_compiler(file, "pure-FIRST-before-initialization", enabled, enabled || accepted_without_feature,
                disabled_feature, expected_subject, expected_value, admission_error, [&]
            {
                ::uwvm2::validation::standard::wasm3::validate_module_declarations_with_runtime_policy(before_effects, admission_error, policy);
            });
        }
        REQUIRE(::uwvm2::uwvm::wasm::storage::all_module.size() == wasm_registry_size);
        REQUIRE(::uwvm2::uwvm::runtime::storage::active_runtime_registry().size() == runtime_registry_size);
        // Both consumer and actual provider are parsed/initialized all-enabled.
        // The independently tighter compilation policy below is never installed
        // into or written over either initialized source instance.
        auto prepared{has_provider ? prepare_runtime_from_wasm(source, u8"zero-body-two-phase",
            {{::std::addressof(provider_source), u8"stage6_provider", ::std::addressof(enabled_policy)}}, enabled_policy) :
            prepare_runtime_from_wasm(source, u8"zero-body-two-phase", {}, enabled_policy)};
        REQUIRE(prepared.mod != nullptr);
        REQUIRE(prepared.mod->local_defined_function_vec_storage.empty());
        // A genuine ref.func control may import one real provider function; consumer local bodies remain empty.
        ::uwvm2::uwvm::utils::ansies::put_color = true; // Qualify actual colored cold parser diagnostics in raw stderr.
        auto const& original_parsed{::uwvm2::uwvm::wasm::storage::execute_wasm.wasm_module_storage.wasm_binfmt_ver1_storage};
        auto const parsed{original_parsed}; // Actual deep-copy ctor, not synthetic metadata.
        check_empty_code(parsed);
        observe_opcode_metadata(original_parsed, parsed, *prepared.mod, expected_mask);
        ::fast_io::print(::fast_io::out(), "ZERO_BODY_METADATA file=", ::fast_io::mnp::os_c_str(file),
            " actual_provider=", ::fast_io::mnp::dec(has_provider),
            " imported_tables=", ::fast_io::mnp::dec(prepared.mod->imported_table_vec_storage.size()),
            " imported_globals=", ::fast_io::mnp::dec(prepared.mod->imported_global_vec_storage.size()),
            " local_functions=0 imported_functions=", ::fast_io::mnp::dec(prepared.mod->imported_function_vec_storage.size()),
            " type_gc=", ::fast_io::mnp::dec(prepared.mod->type_section_storage.requires_gc),
            " table_gc=", ::fast_io::mnp::dec(prepared.mod->table_declarations_require_gc),
            " global_gc=", ::fast_io::mnp::dec(prepared.mod->global_declarations_require_gc),
            " element_gc=", ::fast_io::mnp::dec(prepared.mod->element_declarations_require_gc), "\n");
        for(bool enabled : {true, false})
        {
            auto policy{enabled_policy};
            auto& strict{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(policy)};
            select_feature(strict, disabled_feature, enabled);
            bool const expected_valid{enabled || accepted_without_feature};
            // This is the genuine existing whole-module pure API, not a fake
            // validate_code invocation on a made-up function. It intentionally
            // observes only the bool contract; keeper must separately qualify
            // its raw formatted feature diagnostic for each negative case.
            bool const pure_valid{::uwvm2::uwvm::runtime::validator::validate_all_wasm_code_for_module(
                parsed, policy, u8"zero-body.oracle.wasm", u8"zero-body-two-phase")};
            observe_parser_bool(file, "pure-whole-module", enabled, expected_valid, pure_valid);
            error_t full_error{};
            observe_compiler(file, "uwvm-int-full", enabled, expected_valid, disabled_feature, expected_subject, expected_value, full_error, [&]
            {
                optable::compile_option options;
                auto product{compiler::compile_all_from_uwvm_single_func<k_test_byref_opt>(*prepared.mod, options, full_error, &policy)};
                (void)product;
            });
#if defined(UWVM2TEST_ZERO_BODY_INT_LAZY)
            error_t lazy_error{};
            observe_compiler(file, "uwvm-int-lazy-module-admission", enabled, expected_valid, disabled_feature, expected_subject, expected_value, lazy_error, [&]
            {
                namespace lazy = ::uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator;
                optable::compile_option options;
                lazy::lazy_split_config config{};
                config.eu_policy = lazy::lazy_execution_unit_split_policy_t::function_only;
                auto storage{lazy::initialize_lazy_module_storage(*prepared.mod, options, lazy_error, config, &policy)};
                REQUIRE(storage.compile_units.empty());
                // No function exists to materialize or invoke; this qualifies
                // module admission only and cannot count as a guest execution.
            });
#endif
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
            namespace jit = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
            // Exercise the actual public parser bool facade, not only the main
            // full compiler. Its default overload/global walker forward here.
            bool const jit_parser_valid{jit::validate_all_wasm_code_for_module(
                parsed, policy, u8"zero-body.oracle.wasm", u8"zero-body-llvm-parser")};
            observe_parser_bool(file, "llvm-parser-validation-only", enabled, expected_valid, jit_parser_valid);
            error_t jit_runtime_error{};
            observe_compiler(file, "llvm-runtime-validation-only", enabled, expected_valid, disabled_feature, expected_subject, expected_value, jit_runtime_error, [&]
            {
                jit::validate_runtime_wasm_code_for_module(*prepared.mod, jit_runtime_error, policy);
            });
            error_t jit_convenience_error{};
            observe_compiler(file, "llvm-first-function-convenience", enabled, expected_valid, disabled_feature, expected_subject, expected_value, jit_convenience_error, [&]
            {
                jit::compile_option options;
                options.validator_feature_parameter = &policy;
                auto product{jit::compile_all_from_uwvm_single_func(*prepared.mod, options, jit_convenience_error)};
                (void)product;
                // This real public path must admit a zero-function module's
                // declarations before returning its intentionally empty product.
            });
            error_t jit_error{};
            observe_compiler(file, "llvm-jit-full", enabled, expected_valid, disabled_feature, expected_subject, expected_value, jit_error, [&]
            {
                namespace jit = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
                jit::compile_option options;
                options.validator_feature_parameter = &policy;
                options.verify_llvm_jit_ir = true;
                auto product{jit::compile_all_from_uwvm(*prepared.mod, options, jit_error, 0)};
                (void)product;
                // The real full compiler may return an empty, unemitted product
                // for zero functions. No verifyModule/codegen claim is made.
            });
#if defined(UWVM2TEST_ZERO_BODY_LLVM_LAZY)
            error_t lazy_jit_error{};
            observe_compiler(file, "llvm-jit-lazy-module-admission", enabled, expected_valid, disabled_feature, expected_subject, expected_value, lazy_jit_error, [&]
            {
                namespace lazy = ::uwvm2::runtime::compiler::llvm_jit::compile_cu_from_lazy_validator;
                lazy::compile_option options;
                options.validator_feature_parameter = &policy;
                auto storage{lazy::initialize_lazy_module_storage(*prepared.mod, options, lazy_jit_error)};
                REQUIRE(storage.compile_units.empty());
            });
#endif
#endif
        }
    }
}

int main(int argc, char const* const* argv)
{
    int first{1};
    if(argc >= 3 && ::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[1])} == "--provider")
    {
        ::fast_io::native_file_loader input{::fast_io::mnp::os_c_str(argv[2]), ::fast_io::open_mode::in};
        REQUIRE(input.size() >= 8uz && input.size() <= 65536uz);
        provider_source.resize(input.size());
        // [mapped provider ... end) / [owned provider ... end): equal actual lengths bound this copy.
        ::std::memcpy(provider_source.data(), input.data(), input.size());
        has_provider = true; first = 3;
    }
    REQUIRE(argc >= first + 6 && (argc - first) % 6 == 0);
#if defined(UWVM2TEST_RUNNER_USE_LLVM_JIT)
    ::fast_io::print(::fast_io::out(), "CONSTANT_OPCODE_PROFILE llvm_roles_enabled=1\n");
#else
    ::fast_io::print(::fast_io::out(), "CONSTANT_OPCODE_PROFILE llvm_roles_enabled=0\n");
#endif
    for(int index{first}; index < argc; index += 6)
    {
        ::fast_io::cstring_view const feature{::fast_io::mnp::os_c_str(argv[index])};
        ::fast_io::cstring_view const expected{::fast_io::mnp::os_c_str(argv[index + 1])};
        REQUIRE(feature == "gc" || feature == "exceptions" || feature == "function-references" || feature == "reference-types" || feature == "simd");
        feature_t const selected_feature{feature == "gc" ? feature_t::gc : feature == "exceptions" ? feature_t::exceptions :
            feature == "function-references" ? feature_t::function_references : feature == "reference-types" ? feature_t::reference_types : feature_t::simd};
        REQUIRE(expected == "accept" || expected == "reject");
        auto parse_decimal{[](char const* argument, unsigned maximum)
        {
            ::fast_io::cstring_view const value{::fast_io::mnp::os_c_str(argument)};
            REQUIRE(!value.empty() && value.size() <= 3uz);
            auto const begin{value.data()};
            // [actual argv-owned decimal bytes ...] one-past: nonempty size<=3 bounds this advance.
            auto const end{begin + value.size()};
            unsigned result{};auto const [tail,status]{::fast_io::parse_by_scan(begin,end,::fast_io::mnp::dec_get<true,true>(result))};
            REQUIRE(status == ::fast_io::parse_code::ok && tail == end && result <= maximum);
            return result;
        }};
        unsigned const mask{parse_decimal(argv[index + 2],31u)}, opcode{parse_decimal(argv[index + 4],255u)};
        ::fast_io::cstring_view const site{::fast_io::mnp::os_c_str(argv[index + 3])};
        REQUIRE(site == "global" || site == "table" || site == "element" || site == "data");
        subject_t const subject{site == "table" ? subject_t::table_type : site == "element" ? subject_t::element_segment :
            site == "data" ? subject_t::data_segment : subject_t::global_type};
        // Expected admission is independent of the opcode-only mask: earlier
        // declaration types can require a feature with no matching constant opcode.
        ::fast_io::native_file_loader input{::fast_io::mnp::os_c_str(argv[index + 5]), ::fast_io::open_mode::in};
        REQUIRE(input.size() >= 8uz && input.size() <= 65536uz);
        byte_vec source;source.resize(input.size());
        // [RAII mapped consumer ... end) / [owned consumer ... end): equal verified lengths bound the copy.
        ::std::memcpy(source.data(), input.data(), input.size());
        run(source, argv[index + 5], selected_feature,
            expected == "accept", mask, subject, opcode);
    }
    ::fast_io::print(::fast_io::out(), "ZERO_BODY_TWO_PHASE observations=", ::fast_io::mnp::dec(observations),
        " normative_mismatches=", ::fast_io::mnp::dec(normative_mismatches), " guest_execution=0\n");
    return normative_mismatches != 0u;
}
