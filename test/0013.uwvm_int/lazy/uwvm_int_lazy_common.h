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
    using strict::runtime_local_func_t;
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

    inline runtime_module_t const* g_lazy_call_runtime_module{};
    inline lazy_module_t* g_lazy_call_lazy_module{};
    inline lazy_compile_options_t* g_lazy_call_options{};
    inline runtime_module_t const* g_call_indirect_runtime_module{};
    inline lazy_module_t const* g_call_indirect_lazy_module{};
    inline ::std::size_t g_call_indirect_module_id{};

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

    template <optable::uwvm_interpreter_translate_option_t Opt>
    inline void runner_compile_local_function_if_needed(::std::size_t local_index) UWVM_THROWS
    {
        if(g_lazy_call_runtime_module == nullptr || g_lazy_call_lazy_module == nullptr || g_lazy_call_options == nullptr) [[unlikely]]
        {
            ::fast_io::fast_terminate();
        }
        if(local_index >= g_lazy_call_lazy_module->functions.size()) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const& fn{g_lazy_call_lazy_module->functions.index_unchecked(local_index)};
        if(fn.materialization_state.state.load(::std::memory_order_acquire) == lazy_compile_state_t::compiled) { return; }
        if(fn.primary_cu_index >= g_lazy_call_lazy_module->compile_units.size()) [[unlikely]] { ::fast_io::fast_terminate(); }

        ::uwvm2::validation::error::code_validation_error_impl err{};
        lazy::compile_cu_from_lazy_validator<Opt>(*g_lazy_call_runtime_module,
                                                  *g_lazy_call_lazy_module,
                                                  *g_lazy_call_options,
                                                  fn.primary_cu_index,
                                                  err);
        if(err.err_code != ::uwvm2::validation::error::code_validation_error_code::ok) [[unlikely]] { ::fast_io::fast_terminate(); }
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    inline void runner_call_bridge(::std::size_t wasm_module_id, ::std::size_t call_function, ::std::byte** stack_top_ptr) UWVM_THROWS
    {
        if(wasm_module_id != SIZE_MAX) [[unlikely]] { ::fast_io::fast_terminate(); }
        if(g_lazy_call_lazy_module == nullptr || stack_top_ptr == nullptr || *stack_top_ptr == nullptr) [[unlikely]]
        {
            ::fast_io::fast_terminate();
        }

        auto const* const info{reinterpret_cast<optable::compiled_defined_call_info const*>(call_function)};
        if(info == nullptr || info->runtime_func == nullptr || info->compiled_func == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const compiled_base{g_lazy_call_lazy_module->compiled.local_funcs.data()};
        auto const local_count{g_lazy_call_lazy_module->compiled.local_funcs.size()};
        if(compiled_base == nullptr || info->compiled_func < compiled_base || info->compiled_func >= compiled_base + local_count) [[unlikely]]
        {
            ::fast_io::fast_terminate();
        }
        auto const local_index{static_cast<::std::size_t>(info->compiled_func - compiled_base)};
        runner_compile_local_function_if_needed<Opt>(local_index);

        auto const param_bytes{info->param_bytes};
        auto const result_bytes{info->result_bytes};
        auto const top_addr{reinterpret_cast<::std::uintptr_t>(*stack_top_ptr)};
        if(top_addr < param_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto* const param_base{*stack_top_ptr - param_bytes};
        byte_vec packed_params(param_bytes);
        if(param_bytes != 0uz) { ::std::memcpy(packed_params.data(), param_base, param_bytes); }

        using Runner = interpreter_runner<Opt>;
        auto rr{Runner::run(g_lazy_call_lazy_module->compiled.local_funcs.index_unchecked(local_index),
                            *static_cast<runtime_local_func_t const*>(info->runtime_func),
                            packed_params,
                            nullptr,
                            nullptr)};
        if(rr.results.size() != result_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
        if(result_bytes != 0uz) { ::std::memcpy(param_base, rr.results.data(), result_bytes); }
        *stack_top_ptr = param_base + result_bytes;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    inline void runner_call_indirect_bridge(::std::size_t wasm_module_id,
                                            ::std::size_t type_index,
                                            ::std::size_t table_index,
                                            ::std::byte** stack_top_ptr) UWVM_THROWS
    {
        if(g_call_indirect_runtime_module == nullptr || g_call_indirect_lazy_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        if(wasm_module_id != g_call_indirect_module_id) [[unlikely]] { ::fast_io::fast_terminate(); }
        if(stack_top_ptr == nullptr || *stack_top_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using table_elem_type_t = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;

        auto const import_table_count{g_call_indirect_runtime_module->imported_table_vec_storage.size()};
        if(table_index < import_table_count) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const local_table_index{table_index - import_table_count};
        if(local_table_index >= g_call_indirect_runtime_module->local_defined_table_vec_storage.size()) [[unlikely]] { ::fast_io::fast_terminate(); }

        wasm_i32 selector_i32{};
        *stack_top_ptr -= sizeof(selector_i32);
        ::std::memcpy(::std::addressof(selector_i32), *stack_top_ptr, sizeof(selector_i32));
        auto const selector_u32{::std::bit_cast<::std::uint_least32_t>(selector_i32)};

        auto const type_begin{g_call_indirect_runtime_module->type_section_storage.type_section_begin};
        auto const type_end{g_call_indirect_runtime_module->type_section_storage.type_section_end};
        if(type_begin == nullptr || type_end == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const type_count{static_cast<::std::size_t>(type_end - type_begin)};
        if(type_index >= type_count) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const* const expected_ft_ptr{type_begin + type_index};

        auto const& table{g_call_indirect_runtime_module->local_defined_table_vec_storage.index_unchecked(local_table_index)};
        if(selector_u32 >= table.elems.size()) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const& elem{table.elems.index_unchecked(static_cast<::std::size_t>(selector_u32))};
        if(elem.type != table_elem_type_t::func_ref_defined) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const* const def_ptr{elem.storage.defined_ptr};
        if(def_ptr == nullptr || def_ptr->function_type_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        if(def_ptr->function_type_ptr != expected_ft_ptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const base{g_call_indirect_runtime_module->local_defined_function_vec_storage.data()};
        auto const local_count{g_call_indirect_runtime_module->local_defined_function_vec_storage.size()};
        if(base == nullptr || def_ptr < base || def_ptr >= base + local_count) [[unlikely]] { ::fast_io::fast_terminate(); }
        auto const local_index{static_cast<::std::size_t>(def_ptr - base)};

        auto const param_bytes{strict::abi_total_bytes(def_ptr->function_type_ptr->parameter.begin, def_ptr->function_type_ptr->parameter.end)};
        auto const result_bytes{strict::abi_total_bytes(def_ptr->function_type_ptr->result.begin, def_ptr->function_type_ptr->result.end)};
        auto const top_addr{reinterpret_cast<::std::uintptr_t>(*stack_top_ptr)};
        if(top_addr < param_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto* const param_base{*stack_top_ptr - param_bytes};
        byte_vec packed_params(param_bytes);
        if(param_bytes != 0uz) { ::std::memcpy(packed_params.data(), param_base, param_bytes); }
        if(g_lazy_call_runtime_module != nullptr && g_lazy_call_lazy_module != nullptr && g_lazy_call_options != nullptr)
        {
            runner_compile_local_function_if_needed<Opt>(local_index);
        }

        using Runner = interpreter_runner<Opt>;
        auto rr{Runner::run(g_call_indirect_lazy_module->compiled.local_funcs.index_unchecked(local_index),
                            g_call_indirect_runtime_module->local_defined_function_vec_storage.index_unchecked(local_index),
                            packed_params,
                            nullptr,
                            nullptr)};
        if(rr.results.size() != result_bytes) [[unlikely]] { ::fast_io::fast_terminate(); }
        if(result_bytes != 0uz) { ::std::memcpy(param_base, rr.results.data(), result_bytes); }
        *stack_top_ptr = param_base + result_bytes;
    }

    template <optable::uwvm_interpreter_translate_option_t Opt>
    struct runner_call_indirect_bridge_scope
    {
        explicit runner_call_indirect_bridge_scope(prepared_runtime const& prep,
                                                   lazy_module_t const& storage,
                                                   ::std::size_t module_id = 0uz) noexcept
        {
            g_call_indirect_runtime_module = prep.mod;
            g_call_indirect_lazy_module = ::std::addressof(storage);
            g_call_indirect_module_id = module_id;
            optable::call_indirect_func = runner_call_indirect_bridge<Opt>;
        }

        runner_call_indirect_bridge_scope(runner_call_indirect_bridge_scope const&) = delete;
        runner_call_indirect_bridge_scope& operator=(runner_call_indirect_bridge_scope const&) = delete;

        ~runner_call_indirect_bridge_scope() noexcept
        {
            g_call_indirect_runtime_module = nullptr;
            g_call_indirect_lazy_module = nullptr;
            g_call_indirect_module_id = 0uz;
            optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        }
    };

    template <optable::uwvm_interpreter_translate_option_t Opt>
    struct runner_lazy_call_bridge_scope
    {
        explicit runner_lazy_call_bridge_scope(prepared_runtime const& prep,
                                              lazy_module_t& storage,
                                              lazy_compile_options_t& options,
                                              ::std::size_t module_id = 0uz) noexcept
        {
            g_lazy_call_runtime_module = prep.mod;
            g_lazy_call_lazy_module = ::std::addressof(storage);
            g_lazy_call_options = ::std::addressof(options);
            g_call_indirect_runtime_module = prep.mod;
            g_call_indirect_lazy_module = ::std::addressof(storage);
            g_call_indirect_module_id = module_id;
            optable::call_func = runner_call_bridge<Opt>;
            optable::call_indirect_func = runner_call_indirect_bridge<Opt>;
        }

        runner_lazy_call_bridge_scope(runner_lazy_call_bridge_scope const&) = delete;
        runner_lazy_call_bridge_scope& operator=(runner_lazy_call_bridge_scope const&) = delete;

        ~runner_lazy_call_bridge_scope() noexcept
        {
            g_lazy_call_runtime_module = nullptr;
            g_lazy_call_lazy_module = nullptr;
            g_lazy_call_options = nullptr;
            g_call_indirect_runtime_module = nullptr;
            g_call_indirect_lazy_module = nullptr;
            g_call_indirect_module_id = 0uz;
            optable::call_func = +[](::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
            optable::call_indirect_func = +[](::std::size_t, ::std::size_t, ::std::size_t, ::std::byte**) { ::fast_io::fast_terminate(); };
        }
    };

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
