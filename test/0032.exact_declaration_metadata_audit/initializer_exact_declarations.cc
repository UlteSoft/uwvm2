// Genuine parse-enabled -> initialize-stricter feature admission. This process
// never changes an initialized instance's immutable policy or fabricates metadata.
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"

namespace
{
    using namespace uwvm2test::uwvm_int_strict;
    void require(bool condition, unsigned line)
    {
        if(!condition)
        {
            ::fast_io::print(::fast_io::err(), "FAIL initializer source contract line ", ::fast_io::mnp::dec(line), "\n");
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
    void observe_parser_metadata(Parsed const& parsed)
    {
        []<::uwvm2::parser::wasm::concepts::wasm_feature... Fs>(auto const& sections,
            ::uwvm2::utils::container::tuple<Fs...>)
        {
            using namespace ::uwvm2::parser::wasm;
            auto const& types{concepts::operation::get_first_type_in_tuple<
                standard::wasm1::features::type_section_storage_t<Fs...>>(sections)};
            auto const& codes{concepts::operation::get_first_type_in_tuple<
                standard::wasm1::features::code_section_storage_t<Fs...>>(sections)};
            auto const& imports{concepts::operation::get_first_type_in_tuple<
                standard::wasm1::features::import_section_storage_t<Fs...>>(sections)};
            auto const& functions{concepts::operation::get_first_type_in_tuple<
                standard::wasm1::features::function_section_storage_t>(sections)};
            REQUIRE(codes.codes.empty());
            constexpr ::std::size_t function_import_bucket{};
            static_assert(function_import_bucket < imports.importdesc_count);
            // [actual import buckets ... end): compile-time bound proves bucket zero exists.
            // [safe                         ] no pointer advance; borrow the actual function-import vector.
            REQUIRE(imports.importdesc.index_unchecked(function_import_bucket).empty());
            REQUIRE(functions.funcs.empty());
            bool signatures_require{};
            for(auto const& signature : types.owned_signatures)
            { signatures_require |= signature.requires_function_references; }
            ::fast_io::print(::fast_io::out(), "INITIALIZER_TWO_PHASE_METADATA type_function_references=",
                ::fast_io::mnp::dec(types.requires_function_references), " projected_function_signatures_require=",
                ::fast_io::mnp::dec(signatures_require), " local_functions=0 imported_functions=0\n");
        }(parsed.sections, ::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features);
    }
}

int main(int argc, char const* const* argv)
{
    REQUIRE(argc == 4);
    ::fast_io::cstring_view const feature{::fast_io::mnp::os_c_str(argv[1])};
    REQUIRE(feature == "simd" || feature == "reference-types" || feature == "multi-value");
    ::fast_io::cstring_view const expected{::fast_io::mnp::os_c_str(argv[2])};
    REQUIRE(expected == "accept" || expected == "reject");
    ::fast_io::native_file_loader input{::fast_io::mnp::os_c_str(argv[3]), ::fast_io::open_mode::in};
    REQUIRE(input.size() >= 8uz && input.size() <= 65536uz);
    byte_vec source;
    source.resize(input.size());
    // [RAII mapped bytes ... end) / [owned source allocation ... end)
    // [safe                                                   ] checked equal nonzero lengths bound the copy.
    ::std::memcpy(source.data(), input.data(), input.size());
    auto const parse_policy{enabled_features()};
    auto initialize_policy{parse_policy};
    auto& strict{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(initialize_policy)};
    if(feature == "simd") { strict.disable_simd = true; }
    else if(feature == "reference-types") { strict.disable_reference_types = true; }
    else { strict.disable_multi_value = true; }
    ::uwvm2::uwvm::io::show_verbose = false;
    ::uwvm2::uwvm::io::show_depend_warning = false;
    ::uwvm2::uwvm::utils::ansies::put_color = true;
    // This executable starts with real empty host storages; only the actual
    // execute-module record is installed before the standard loader/init sequence.
    REQUIRE(::uwvm2::uwvm::wasm::storage::all_module.empty());
    REQUIRE(::uwvm2::uwvm::wasm::storage::preloaded_wasm.empty());
    ::uwvm2::parser::wasm::base::error_impl parse_error{};
    // [owned source ... final live byte] one-past
    // [safe                             ] source.size() >= 8 proves a nonnull base and this bounded advance.
    auto const* begin{source.data()};
    auto const* end{begin + source.size()};
    // [owned source ...] one-past
    // [safe            ] ^^ end: within this one allocation; parser checks before every read.
    auto parsed{::uwvm2::uwvm::wasm::feature::binfmt_ver1_handler(begin, end, parse_error, parse_policy)};
    observe_parser_metadata(parsed);
    ::fast_io::print(::fast_io::out(), "INITIALIZER_TWO_PHASE_PARSE file=", ::fast_io::mnp::os_c_str(argv[3]),
        " disabled_feature=", ::fast_io::mnp::os_c_str(argv[1]), " parse_feature=1 init_feature=0 expected_initialized=",
        ::fast_io::mnp::dec(expected == "accept"), " guest_execution=0\n");
    auto& file{::uwvm2::uwvm::wasm::storage::execute_wasm};
    file = ::uwvm2::uwvm::wasm::type::wasm_file_t{1u};
    file.file_name = u8"initializer-two-phase.wasm";
    file.module_name = u8"initializer-exact-declaration-policy";
    file.binfmt_ver = 1u;
    file.wasm_parameter.binfmt1_para = initialize_policy;
    file.wasm_module_storage.wasm_binfmt_ver1_storage = ::std::move(parsed);
    REQUIRE(::uwvm2::uwvm::wasm::loader::construct_all_module_and_check_duplicate_module() ==
        ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok);
    REQUIRE(::uwvm2::uwvm::wasm::loader::check_import_exist_and_detect_cycles() ==
        ::uwvm2::uwvm::wasm::loader::load_and_check_modules_rtl::ok);
    // The actual initializer consumes this fresh parsed source under the tighter
    // file policy. A required type must terminate here with its ordinary fatal.
    ::uwvm2::uwvm::runtime::initializer::initialize_runtime();
    auto const found{::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.find(file.module_name)};
    REQUIRE(found != ::uwvm2::uwvm::runtime::storage::wasm_module_runtime_storage.end());
    REQUIRE(found->second.local_defined_function_vec_storage.empty());
    REQUIRE(found->second.imported_function_vec_storage.empty());
    bool const matches{expected == "accept"};
    ::fast_io::print(::fast_io::out(), "INITIALIZER_TWO_PHASE_RESULT actual_initialized=1 expected_initialized=",
        ::fast_io::mnp::dec(matches), " matches=", ::fast_io::mnp::dec(matches), " guest_execution=0\n");
    // An unexpected BEFORE admission remains a failing regression. For AFTER
    // negative evidence the keeper must qualify the exact initializer fatal,
    // feature name/value, color and signal; arbitrary death is never a PASS.
    return matches ? 0 : 1;
}
