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
    auto load_checked(char const* path, error& failure, ::uwvm2::validation::standard::wasm3::retained_plan_limits limits)
    {
        return checked::parsed_integer_source_plan::load_validate_after_drain(
            ::fast_io::u8concat_std(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(path))),
            ::fast_io::u8concat_std(u8"parsed-checked-before-init"), features(), failure, limits);
    }

    namespace validation = ::uwvm2::validation::standard::wasm3;
    using limits_type = validation::retained_plan_limits;
    using reason_type = validation::retained_plan_unavailability;
    auto operation_limits()
    {
        limits_type limits{};
        limits.function_operations = 64uz;
        limits.function_metadata_bytes = 65536uz;
        limits.module_operations = 128uz;
        limits.module_metadata_bytes = 262144uz;
        return limits;
    }
    void require_before_effects(checked::parsed_integer_source_plan::owner const& parsed,
        ::std::uint_least64_t serial, ::std::uint_least64_t generation)
    {
        REQUIRE(parsed && parsed->matches_selected_source());
        REQUIRE(parsed->source()->registry().empty() && !parsed->source()->initialized_from_actual_state());
        REQUIRE(!parsed->seal_actual_initializer());
        REQUIRE(runtime::storage::active_runtime_registry().empty() && wasm::storage::all_module.empty());
        REQUIRE(::uwvm2::runtime::gc::published_initializer_serial.load(::std::memory_order_acquire) == serial);
        REQUIRE(lib::observe_compiler_runtime_generation_host_api() == generation);
    }
    void reject_invalid_last(char const* path, char const* name)
    {
        drained_setup();
        auto const serial{::uwvm2::runtime::gc::published_initializer_serial.load(::std::memory_order_acquire)};
        auto const generation{lib::observe_compiler_runtime_generation_host_api()};
        error failure{}; checked::parsed_integer_source_plan::owner parsed{}; bool actual_error{};
        try { parsed = load_checked(path, failure, operation_limits()); }
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
        REQUIRE(::uwvm2::runtime::gc::published_initializer_serial.load(::std::memory_order_acquire) == serial);
        REQUIRE(lib::observe_compiler_runtime_generation_host_api() == generation);
        full::retire_selected_full_source_after_drain();
        ::fast_io::print(::fast_io::out(), "RETAINED_QUOTA invalid_last_after=", ::fast_io::mnp::os_c_str(name),
            " rejected_by_actual_validator=1 private_plan_minted=0 runtime_registry_empty=1\n");
    }
    void accepted(char const* path, char const* name, limits_type limits, ::std::size_t count,
        ::std::size_t limited_index, reason_type expected, bool index_complete, bool limit_hit)
    {
        drained_setup(); error failure{};
        auto const serial{::uwvm2::runtime::gc::published_initializer_serial.load(::std::memory_order_acquire)};
        auto const generation{lib::observe_compiler_runtime_generation_host_api()};
        auto parsed{load_checked(path, failure, limits)};
        REQUIRE(parsed && failure.err_code == error_code::ok && parsed->function_count() == count);
        require_before_effects(parsed, serial, generation);
        REQUIRE(parsed->complete_function_record_index() == index_complete && parsed->retention_limit_hit() == limit_hit);
        REQUIRE(parsed->retained_record_operations() <= limits.module_operations);
        REQUIRE(parsed->retained_record_metadata_bytes() <= limits.module_metadata_bytes);
        if(index_complete)
        {
            auto function{parsed->function(limited_index)};
            REQUIRE(function && function->validation_sealed() && !function->integer_lowering_supported());
            REQUIRE(function->unavailability() == expected && function->operations().empty());
            REQUIRE(function->parameters().empty() && function->local_runs().empty() && function->results().empty());
            REQUIRE(function->retained_metadata_bytes() == 0uz);
            if(count == 4uz)
            {
                REQUIRE(parsed->function(0uz)->integer_lowering_supported() && parsed->function(1uz)->integer_lowering_supported());
                REQUIRE(parsed->function(0uz)->operations().size() == 50uz && parsed->function(1uz)->operations().size() == 50uz);
                REQUIRE(parsed->retained_record_operations() == 100uz);
            }
        }
        else
        {
            REQUIRE(!parsed->function(count - 1uz));
            REQUIRE(parsed->retention_limit_hit());
        }
        ::std::weak_ptr<full::full_source_instance const> weak{parsed->source()};
        lib::reset_runtime_state_host_api(); REQUIRE(!parsed->matches_selected_source());
        full::retire_selected_full_source_after_drain(); parsed.reset(); REQUIRE(weak.expired());
        ::fast_io::print(::fast_io::out(), "RETAINED_QUOTA case=", ::fast_io::mnp::os_c_str(name),
            " valid_guest_accepted=1 all_body_validation_completed=1 payload_discarded=1 no_raw_fallback=1 runtime_effects=0\n");
    }
}
int main(int argc, char** argv)
{
    if(argc != 7) { return 64; }
    // Invalid cases run FIRST; a previously warmed adjacent function cannot
    // hide the continued authoritative pass beyond limit/unsupported states.
    reject_invalid_last(argv[1], "module_limit");
    reject_invalid_last(argv[2], "unsupported_instruction");
    auto limits{operation_limits()};
    accepted(argv[3], "function_operations", limits, 1uz, 0uz, reason_type::function_operations_limit, true, true);
    auto bytes{limits}; bytes.function_operations = 1024uz; bytes.function_metadata_bytes = 512uz;
    accepted(argv[3], "function_bytes", bytes, 1uz, 0uz, reason_type::function_metadata_bytes_limit, true, true);
    accepted(argv[4], "unsupported_then_many_nop", limits, 1uz, 0uz, reason_type::unsupported_instruction, true, false);
    accepted(argv[5], "module_operations", limits, 4uz, 2uz, reason_type::module_operations_limit, true, true);
    auto owner{limits}; owner.module_operations = 1024uz; owner.module_metadata_bytes = 1024uz;
    accepted(argv[6], "module_fixed_metadata", owner, 64uz, 0uz, reason_type::module_metadata_bytes_limit, false, true);
    return 0;
}
