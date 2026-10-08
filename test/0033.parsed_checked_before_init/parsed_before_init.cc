#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <fast_io.h>
#include <fast_io_unit/string.h>
#include <uwvm2/uwvm/runtime/checked_source/impl.h>
#include <uwvm2/uwvm/runtime/initializer/impl.h>
#include <uwvm2/runtime/lib/uwvm_runtime.h>
#if defined(UWVM_TEST_ORD_CHECKED_LAZY_CONSUMER) && UWVM_TEST_ORD_CHECKED_LAZY_CONSUMER == 1
# include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
# include <uwvm2/runtime/compiler/uwvm_int/compile_cu_from_lazy_validator/impl.h>
#endif
namespace
{
    namespace full = ::uwvm2::uwvm::runtime::full;
    namespace lib = ::uwvm2::runtime::lib;
    namespace wasm = ::uwvm2::uwvm::wasm;
    namespace runtime = ::uwvm2::uwvm::runtime;
    namespace checked = runtime::checked_source;
    using error = ::uwvm2::validation::error::code_validation_error_impl;
    using error_code = ::uwvm2::validation::error::code_validation_error_code;
    void require(bool value, unsigned line)
    {
        if(!value)
        {
            ::fast_io::print(::fast_io::err(), "parsed_before_init: FAIL line=", ::fast_io::mnp::dec(line), "\n");
            ::fast_io::fast_terminate();
        }
    }
#define REQUIRE(x) require(static_cast<bool>(x), __LINE__)
    auto features()
    {
        wasm::feature::wasm_binfmt_ver1_feature_parameter_storage_t result{};
        auto& p{wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(result)};
        p.disable_multi_value = false; p.disable_reference_types = false;
        p.disable_table_instructions = false; p.disable_multiple_tables = false;
        p.disable_bulk_memory = false; p.disable_sign_extension = false;
        p.disable_nontrapping_float_to_int = false; p.disable_simd = false;
        p.controllable_allow_multi_result_vector = false; p.controllable_allow_multi_table = false;
        p.disable_gc = false; p.disable_exceptions = false; p.disable_function_references = false;
        p.disable_memory64 = false; p.disable_table64 = false; p.disable_multi_memory = false;
        p.disable_tail_call = false; p.disable_extended_const = false; p.disable_table_initializer = false;
        return result;
    }
    void drained_setup()
    {
        lib::reset_runtime_state_host_api(); full::retire_selected_full_source_after_drain();
        wasm::storage::all_module.clear(); wasm::storage::all_module_export.clear();
        wasm::storage::preloaded_wasm.clear(); wasm::storage::preload_local_imported.clear();
#if defined(UWVM_SUPPORT_PRELOAD_DL)
        wasm::storage::preloaded_dl.clear();
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
        wasm::storage::weak_symbol.clear();
#endif
        ::uwvm2::uwvm::io::show_verbose = false; ::uwvm2::uwvm::io::show_depend_warning = false;
        REQUIRE(runtime::storage::active_runtime_registry().empty());
    }
    auto load_checked(char const* path, error& failure)
    {
        return checked::parsed_integer_source_plan::load_validate_after_drain(
            ::fast_io::u8concat_std(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(path))),
            ::fast_io::u8concat_std(u8"parsed-checked-before-init"), features(), failure);
    }
    void owned_image_quota_before_source_selection(char const* path)
    {
        drained_setup(); error failure{}; checked::parsed_source_image_status resource{};
        auto const serial{::uwvm2::runtime::gc::published_initializer_serial.load(::std::memory_order_acquire)};
        auto const generation{lib::observe_compiler_runtime_generation_host_api()};
        auto parsed{checked::parsed_integer_source_plan::load_validate_after_drain(
            ::fast_io::u8concat_std(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(path))),
            ::fast_io::u8concat_std(u8"owned-image-resource-probe"), features(), failure,
            {}, ::std::addressof(resource), 1uz)};
        REQUIRE(!parsed && failure.err_code == error_code::ok &&
            resource.error == ::uwvm2::utils::control::owned_image_error::quota_exceeded);
        REQUIRE(!full::selected_full_source_owner_pin() && runtime::storage::active_runtime_registry().empty());
        REQUIRE(wasm::storage::all_module.empty() && wasm::storage::all_module_export.empty());
        REQUIRE(::uwvm2::runtime::gc::published_initializer_serial.load(::std::memory_order_acquire) == serial &&
            lib::observe_compiler_runtime_generation_host_api() == generation);
        ::fast_io::print(::fast_io::out(), "PARSED_CHECKED image_quota_resource_unavailable=1 validation_error_unchanged=1 no_source_selected=1 no_initializer_effect=1\n");
    }
    void invalid_before_effects(char const* path)
    {
        drained_setup();
        auto const before{::uwvm2::runtime::gc::published_initializer_serial.load(::std::memory_order_acquire)};
        auto const generation{lib::observe_compiler_runtime_generation_host_api()};
        error failure{}; checked::parsed_integer_source_plan::owner parsed{}; bool actual_error{};
        try { parsed = load_checked(path, failure); }
        catch(::fast_io::error const& exception)
        {
            REQUIRE(exception.domain == ::fast_io::parse_domain_value && exception.code ==
                static_cast<::std::size_t>(static_cast<char8_t>(::fast_io::parse_code::invalid)));
            actual_error = true;
        }
        REQUIRE(actual_error && !parsed && failure.err_code == error_code::br_value_type_mismatch);
        REQUIRE(failure.err_selectable.br_value_type_mismatch.op_code_name == u8"local.get (unset non-null local)");
        auto selected{full::selected_full_source_owner_pin()};
        REQUIRE(selected && selected->registry().empty() && !selected->initialized_from_actual_state());
        REQUIRE(runtime::storage::active_runtime_registry().empty() && wasm::storage::all_module.empty());
        REQUIRE(::uwvm2::runtime::gc::published_initializer_serial.load(::std::memory_order_acquire) == before);
        REQUIRE(lib::observe_compiler_runtime_generation_host_api() == generation);
        full::retire_selected_full_source_after_drain();
        ::fast_io::print(::fast_io::out(), "PARSED_CHECKED unused_invalid_before_effects=1 runtime_registry_empty=1 initializer_serial_unchanged=1 earlier_gc_body_validated=1\n");
    }
#if defined(UWVM_TEST_ORD_CHECKED_LAZY_CONSUMER) && UWVM_TEST_ORD_CHECKED_LAZY_CONSUMER == 1
    template<::uwvm2test::uwvm_int_strict::optable::uwvm_interpreter_translate_option_t Options>
    void actual_lazy_record_consumer(checked::parsed_integer_source_plan::owner const& parsed, char const* abi)
    {
        namespace strict = ::uwvm2test::uwvm_int_strict;
        namespace lazy = ::uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator;
        error failure{};
        auto bound{lazy::checked_integer_module_plan::bind_after_actual_initializer(parsed, failure)};
        REQUIRE(bound && bound->function_count() == parsed->function_count() && failure.err_code == error_code::ok);
        auto const* module{parsed->source()->initialized_main_module()}; REQUIRE(module);
        for(::std::size_t index{}; index != parsed->function_count(); ++index)
        { REQUIRE(bound->function(index) == parsed->function(index).get()); }
        strict::optable::compile_option options{};
        auto storage{lazy::initialize_checked_lazy_module_storage(*module, options, bound)};
        for(auto const& function: storage.compiled.local_funcs) { REQUIRE(function.op.operands.empty()); }
        lazy::lazy_compile_options materialize{}; materialize.compile_options = options;
        materialize.validator_feature_parameter = ::std::addressof(bound->features());
        lazy::compile_cu_from_lazy_validator<Options>(*module, storage, materialize, 1uz, failure);
        REQUIRE(storage.compiled.local_funcs[0uz].op.operands.empty() && !storage.compiled.local_funcs[1uz].op.operands.empty() &&
            storage.compiled.local_funcs[2uz].op.operands.empty());
        strict::byte_vec arguments(sizeof(::std::int32_t)); ::std::int32_t argument{7};
        // [owned native i32 argument] end; allocation size is exact before this payload copy.
        ::std::memcpy(arguments.data(), ::std::addressof(argument), sizeof(argument));
        auto actual{strict::interpreter_runner<Options>::run(storage.compiled.local_funcs[1uz],
            module->local_defined_function_vec_storage[1uz], arguments, nullptr, nullptr)};
        REQUIRE(actual.results.size() == sizeof(argument) && strict::load_i32(actual.results) == 47);
        REQUIRE(lazy::lower_checked_integer_function<Options>(bound, *module, 2uz, storage.compiled.local_funcs[2uz]) ==
            lazy::checked_integer_lowering_status::unsupported_instruction && storage.compiled.local_funcs[2uz].op.operands.empty());
        ::fast_io::print(::fast_io::out(), "PARSED_CHECKED abi=", ::fast_io::mnp::os_c_str(abi),
            " original_record_identity=1 startup_streams=0 requested_first_use_only=1 actual_integer_result=47 unsupported_gc_no_raw_fallback=1\n");
    }
#endif
    void valid_then_actual_effects(char const* path, char const* expected_sha = nullptr)
    {
        drained_setup(); error failure{};
        auto const before{::uwvm2::runtime::gc::published_initializer_serial.load(::std::memory_order_acquire)};
        auto parsed{load_checked(path, failure)};
        REQUIRE(parsed && parsed->matches_selected_source() && parsed->function_count() == 3uz && failure.err_code == error_code::ok);
        auto source{parsed->source()}; REQUIRE(source && source->registry().empty() && !source->initialized_from_actual_state());
        REQUIRE(source->file().has_owned_source_image() && source->file().wasm_file.empty());
        auto const identities{parsed->source_module_identities()}; REQUIRE(identities.size() == 1uz);
        auto const& identity{identities[0uz]};
        REQUIRE(identity.source_ordinal == 0uz && identity.binfmt_version == 1u &&
            identity.source_byte_length == source->file().source_size() && identity.local_function_count == 3uz &&
            identity.parsed_import_count == 0uz && parsed->source_file_closure_complete());
        ::std::string digest{}; ::fast_io::ostring_ref_std output{::std::addressof(digest)};
        for(auto const byte: identity.source_sha256)
        { ::fast_io::print(output, ::fast_io::mnp::hex<false, true>(::std::to_integer<::std::uint_least8_t>(byte))); }
        REQUIRE(digest.size() == 64uz);
        if(expected_sha != nullptr)
        {
            REQUIRE(digest == ::fast_io::concat_std(::fast_io::mnp::os_c_str(expected_sha)));
            // argv path names a dedicated mutable fixture copy; immutable
            // original oracle bytes/headers are never overwritten by this test.
            auto const begin{source->file().source_cbegin()};
            auto const end{source->file().source_cend()};
            {
                ::fast_io::native_file changed{::fast_io::mnp::os_c_str(path),
                    ::fast_io::open_mode::out | ::fast_io::open_mode::trunc};
                ::fast_io::print(changed, "changed after owned typed parsing; no valid Wasm map remains");
                // Synchronous print completed; the owning native_file closes once
                // at this scope boundary before the mapped source checks below.
            }
            REQUIRE(source->file().source_cbegin() == begin && source->file().source_cend() == end &&
                source->file().source_size() == identity.source_byte_length);
            ::fast_io::print(::fast_io::out(), "PARSED_CHECKED same_owned_image_after_actual_file_overwrite=1 source_sha256=", digest,
                " source_bytes=", ::fast_io::mnp::dec(identity.source_byte_length), " ordered_source_module=0 source_only_identity=1\n");
        }
        REQUIRE(!parsed->seal_actual_initializer());
        REQUIRE(runtime::storage::active_runtime_registry().empty() && wasm::storage::all_module.empty());
        REQUIRE(::uwvm2::runtime::gc::published_initializer_serial.load(::std::memory_order_acquire) == before);
        auto original{parsed->function(1uz)}; REQUIRE(original && original->integer_lowering_supported());
        REQUIRE(!parsed->function(2uz)->integer_lowering_supported());
        REQUIRE(wasm::loader::construct_all_module_and_check_duplicate_module() == wasm::loader::load_and_check_modules_rtl::ok);
        REQUIRE(wasm::loader::check_import_exist_and_detect_cycles() == wasm::loader::load_and_check_modules_rtl::ok);
        runtime::initializer::initialize_runtime(true);
        REQUIRE(parsed->seal_actual_initializer() && parsed->matches_selected_source());
        auto const* module{source->initialized_main_module()}; REQUIRE(module);
        REQUIRE(module->local_defined_global_vec_storage.size() == 1uz && module->local_defined_table_vec_storage.size() == 1uz);
        auto const& global{module->local_defined_global_vec_storage[0uz].global};
        REQUIRE(global.kind == ::uwvm2::object::global::global_type::wasm_i32 && global.storage.i32 == 42);
        auto const& table{module->local_defined_table_vec_storage[0uz]}; REQUIRE(table.elems.size() == 1uz);
        REQUIRE(table.elems[0uz].type == runtime::storage::local_defined_table_elem_storage_type_t::func_ref_defined &&
            table.elems[0uz].storage.defined_ptr == ::std::addressof(module->local_defined_function_vec_storage[0uz]));
        REQUIRE(parsed->function(1uz).get() == original.get());
#if defined(UWVM_TEST_ORD_CHECKED_LAZY_CONSUMER) && UWVM_TEST_ORD_CHECKED_LAZY_CONSUMER == 1
        actual_lazy_record_consumer<::uwvm2test::uwvm_int_strict::k_test_byref_opt>(parsed, "byref");
        actual_lazy_record_consumer<::uwvm2test::uwvm_int_strict::k_test_tail_min_opt>(parsed, "musttail-no-cache");
#endif
        ::std::weak_ptr<full::full_source_instance const> weak{source}; source.reset();
        lib::reset_runtime_state_host_api(); REQUIRE(!parsed->matches_selected_source() && !parsed->seal_actual_initializer());
        REQUIRE(!weak.expired()); full::retire_selected_full_source_after_drain();
        parsed.reset(); original.reset(); REQUIRE(weak.expired());
        ::fast_io::print(::fast_io::out(), "PARSED_CHECKED valid_minted_before_effects=1 global_after_real_initializer=42 actual_table_initializer=1 original_records_retained=1 reset_denied=1 weak_final_release=1 whole_default_pipeline_complete=0 concurrent_reset_qualified=0\n");
    }
}
int main(int argc, char** argv)
{
    if(argc != 3 && argc != 4) { return 64; }
    // [actual argv[0..argc)] null terminal
    // [safe index1/2, and3 only after argc==4] no Wasm input pointer.
    invalid_before_effects(argv[2]);
    owned_image_quota_before_source_selection(argv[1]);
    valid_then_actual_effects(argv[1], argc == 4 ? argv[3] : nullptr); return 0;
}
