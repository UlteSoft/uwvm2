#pragma once

#include "../strict/uwvm_int_translate_strict_common.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

#include <uwvm2/runtime/compiler/uwvm_int/compile_cu_from_lazy_validator/impl.h>
#include <uwvm2/runtime/lib/uwvm_runtime.h>
#include <uwvm2/uwvm/runtime/runtime_mode/impl.h>

namespace uwvm2test::uwvm_int_lazy
{
    namespace strict = ::uwvm2test::uwvm_int_strict;
    namespace lazy = ::uwvm2::runtime::compiler::uwvm_int::compile_cu_from_lazy_validator;
    namespace optable = ::uwvm2::runtime::compiler::uwvm_int::optable;

    using strict::byte_vec;
    using strict::func_body;
    using strict::func_type;
    using strict::interpreter_runner;
    using strict::k_block_empty;
    using strict::k_val_i32;
    using strict::load_i32;
    using strict::module_builder;
    using strict::pack_i32;
    using strict::prepare_runtime_from_wasm;
    using strict::prepared_runtime;
    using strict::runtime_module_t;
    using strict::u8;
    using strict::wasm_op;

    using lazy_module_t = lazy::lazy_module_storage_t;
    using lazy_split_config_t = lazy::lazy_split_config;
    using lazy_compile_options_t = lazy::lazy_compile_options;
    using lazy_validation_mode_t = lazy::lazy_validation_mode;
    using lazy_compile_state_t = ::uwvm2::utils::thread::lazy_compile_state;

    inline void configure_unexpected_traps() noexcept
    {
        static auto trap_unexpected = []() noexcept { ::fast_io::fast_terminate(); };
        optable::unreachable_func = +trap_unexpected;
        optable::trap_invalid_conversion_to_integer_func = +trap_unexpected;
        optable::trap_integer_divide_by_zero_func = +trap_unexpected;
        optable::trap_integer_overflow_func = +trap_unexpected;
        optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
    }

    [[nodiscard]] inline lazy::parser_module_storage_t const*
        find_validator_module_storage(::uwvm2::utils::container::u8string_view module_name) noexcept
    {
        auto const it{::uwvm2::uwvm::wasm::storage::all_module.find(module_name)};
        if(it == ::uwvm2::uwvm::wasm::storage::all_module.end()) { return nullptr; }

        using module_type_t = ::uwvm2::uwvm::wasm::type::module_type_t;
        auto const& am{it->second};
        if(am.type != module_type_t::exec_wasm && am.type != module_type_t::preloaded_wasm) { return nullptr; }

        auto const wf{am.module_storage_ptr.wf};
        if(wf == nullptr || wf->binfmt_ver != 1u) { return nullptr; }
        return ::std::addressof(wf->wasm_module_storage.wasm_binfmt_ver1_storage);
    }

    [[nodiscard]] inline lazy_split_config_t small_code_size_split_config() noexcept
    {
        return lazy_split_config_t{.eu_policy = lazy::lazy_execution_unit_split_policy_t::structured_control,
                                   .cu_policy = lazy::lazy_compile_unit_split_policy_t::code_size,
                                   .cu_code_size = 5uz};
    }

    [[nodiscard]] inline lazy_module_t initialize_lazy_storage(runtime_module_t const& rt, lazy_split_config_t cfg)
    {
        ::uwvm2::validation::error::code_validation_error_impl err{};
        optable::compile_option cop{};
        auto storage{lazy::initialize_lazy_module_storage(rt, cop, err, cfg)};
        if(err.err_code != ::uwvm2::validation::error::code_validation_error_code::ok) [[unlikely]] { ::fast_io::fast_terminate(); }
        return storage;
    }

    [[nodiscard]] inline lazy_compile_options_t make_lazy_options(::uwvm2::utils::container::u8string_view module_name,
                                                                  lazy_validation_mode_t mode,
                                                                  ::std::size_t module_id = 0uz) noexcept
    {
        optable::compile_option cop{};
        cop.curr_wasm_id = module_id;

        lazy_compile_options_t options{};
        options.compile_options = cop;
        options.validation_mode = mode;
        options.validator_module_storage = mode == lazy_validation_mode_t::validate_on_lazy_compile ? find_validator_module_storage(module_name) : nullptr;
        return options;
    }

    inline void configure_lazy_runtime(::std::size_t worker_count, ::std::size_t scheduling_size) noexcept
    {
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;

        mode::global_runtime_mode = mode::runtime_mode_t::lazy_compile;
        mode::global_runtime_compiler = mode::runtime_compiler_t::uwvm_interpreter_only;
        mode::global_runtime_compile_threads_resolved = worker_count;
        mode::global_runtime_scheduling_policy = mode::runtime_scheduling_policy_t::code_size;
        mode::global_runtime_scheduling_size = scheduling_size;
    }
}  // namespace uwvm2test::uwvm_int_lazy
