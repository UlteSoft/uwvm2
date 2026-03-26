enum class compile_task_split_policy_t : unsigned
{
    function_count,
    code_size
};

struct compile_task_split_config
{
    compile_task_split_policy_t policy{compile_task_split_policy_t::code_size};
    ::std::size_t split_size{4096uz};
    bool adjust_for_default_policy{true};
};

namespace details
{
    using full_function_symbol_t = ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t;

    struct local_function_task_group
    {
        ::std::size_t begin_index{};
        ::std::size_t end_index{};
    };

    inline constexpr void initialize_local_defined_call_info(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                             ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option const& options,
                                                             full_function_symbol_t& storage) noexcept
    {
        auto const import_func_count{curr_module.imported_function_vec_storage.size()};
        auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};

        storage.local_funcs.clear();
        storage.local_funcs.resize(local_func_count);
        storage.local_defined_call_info.clear();
        storage.local_defined_call_info.resize(local_func_count);

        for(::std::size_t i{}; i != local_func_count; ++i)
        {
            auto& info{storage.local_defined_call_info.index_unchecked(i)};
            info.module_id = options.curr_wasm_id;
            info.function_index = import_func_count + i;
        }
    }

    [[nodiscard]] inline ::std::size_t
        calculate_local_function_task_unit(::uwvm2::uwvm::runtime::storage::local_defined_function_storage_t const& local_func,
                                           ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_policy_t split_policy) noexcept
    {
        if(split_policy == ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_policy_t::function_count) { return 1uz; }

        auto const& body{local_func.wasm_code_ptr->body};
        auto const code_begin{reinterpret_cast<::std::byte const*>(body.code_begin)};
        auto const code_end{reinterpret_cast<::std::byte const*>(body.code_end)};
        return static_cast<::std::size_t>(code_end - code_begin);
    }

    [[nodiscard]] inline ::uwvm2::utils::container::vector<local_function_task_group>
        build_local_function_task_groups(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                         ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_config split_config)
    {
        ::uwvm2::utils::container::vector<local_function_task_group> task_groups{};

        auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};
        if(local_func_count == 0uz) { return task_groups; }

        task_groups.reserve(local_func_count);

        auto const split_size{split_config.split_size == 0uz ? 1uz : split_config.split_size};

        ::std::size_t group_begin_index{};
        ::std::size_t current_group_weight{};

        for(::std::size_t local_function_idx{}; local_function_idx != local_func_count; ++local_function_idx)
        {
            auto const task_unit{
                calculate_local_function_task_unit(curr_module.local_defined_function_vec_storage.index_unchecked(local_function_idx), split_config.policy)};

            if(task_unit > (::std::numeric_limits<::std::size_t>::max() - current_group_weight)) [[unlikely]]
            {
                current_group_weight = ::std::numeric_limits<::std::size_t>::max();
            }
            else
            {
                current_group_weight += task_unit;
            }

            if(current_group_weight >= split_size)
            {
                task_groups.push_back_unchecked({.begin_index = group_begin_index, .end_index = local_function_idx + 1uz});
                group_begin_index = local_function_idx + 1uz;
                current_group_weight = 0uz;
            }
        }

        if(group_begin_index != local_func_count) { task_groups.push_back_unchecked({.begin_index = group_begin_index, .end_index = local_func_count}); }

        return task_groups;
    }

    [[nodiscard]] inline ::std::size_t calculate_total_local_function_task_weight(
        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
        ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_policy_t split_policy) noexcept
    {
        ::std::size_t total_weight{};

        for(auto const& local_func: curr_module.local_defined_function_vec_storage)
        {
            auto const task_unit{calculate_local_function_task_unit(local_func, split_policy)};
            if(task_unit > (::std::numeric_limits<::std::size_t>::max() - total_weight)) [[unlikely]] { return ::std::numeric_limits<::std::size_t>::max(); }
            total_weight += task_unit;
        }

        return total_weight;
    }

    [[nodiscard]] inline ::std::size_t
        calculate_local_function_task_group_count(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                  ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_config split_config) noexcept
    {
        auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};
        if(local_func_count == 0uz) { return 0uz; }

        auto const split_size{split_config.split_size == 0uz ? 1uz : split_config.split_size};

        ::std::size_t task_group_count{};
        ::std::size_t current_group_weight{};

        for(::std::size_t local_function_idx{}; local_function_idx != local_func_count; ++local_function_idx)
        {
            auto const task_unit{
                calculate_local_function_task_unit(curr_module.local_defined_function_vec_storage.index_unchecked(local_function_idx), split_config.policy)};

            if(task_unit > (::std::numeric_limits<::std::size_t>::max() - current_group_weight)) [[unlikely]]
            {
                current_group_weight = ::std::numeric_limits<::std::size_t>::max();
            }
            else
            {
                current_group_weight += task_unit;
            }

            if(current_group_weight >= split_size)
            {
                ++task_group_count;
                current_group_weight = 0uz;
            }
        }

        if(current_group_weight != 0uz) { ++task_group_count; }
        return task_group_count;
    }

    [[nodiscard]] inline bool
        should_run_local_functions_serially(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                            ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_config split_config,
                                            ::std::size_t extra_compile_threads) noexcept
    {
        auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};
        if(extra_compile_threads == 0uz || local_func_count <= 1uz) { return true; }

        auto const split_size{split_config.split_size == 0uz ? 1uz : split_config.split_size};

        if(split_config.policy == ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::compile_task_split_policy_t::function_count)
        {
            return local_func_count <= split_size;
        }

        auto const total_task_weight{calculate_total_local_function_task_weight(curr_module, split_config.policy)};
        return total_task_weight <= split_size;
    }

    inline constexpr void aggregate_local_function_storage(full_function_symbol_t& storage) noexcept
    {
        storage.local_count = 0uz;
        storage.local_bytes_max = 0uz;
        storage.local_bytes_zeroinit_end = 0uz;
        storage.operand_stack_max = 0uz;
        storage.operand_stack_byte_max = 0uz;

        for(auto const& local_func: storage.local_funcs)
        {
            storage.local_count = ::std::max(storage.local_count, local_func.local_count);
            storage.local_bytes_max = ::std::max(storage.local_bytes_max, local_func.local_bytes_max);
            storage.local_bytes_zeroinit_end = ::std::max(storage.local_bytes_zeroinit_end, local_func.local_bytes_zeroinit_end);
            storage.operand_stack_max = ::std::max(storage.operand_stack_max, local_func.operand_stack_max);
            storage.operand_stack_byte_max = ::std::max(storage.operand_stack_byte_max, local_func.operand_stack_byte_max);
        }
    }

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
    inline void compile_all_from_uwvm_local_func(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                 [[maybe_unused]] ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option& options,
                                                 full_function_symbol_t& storage,
                                                 ::std::size_t compile_local_function_idx,
                                                 ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS{
#define UWVM_COMPILE_SINGLE_LOCAL_FUNCTION 1
#include "single_func_context.h"
#include "single_func_emit_helpers.h"
#include "single_func_validation_dispatch.h"
#undef UWVM_COMPILE_SINGLE_LOCAL_FUNCTION
    }

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
    inline void compile_all_from_uwvm_local_func_group(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                       [[maybe_unused]] ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option& options,
                                                       full_function_symbol_t& storage,
                                                       local_function_task_group task_group,
                                                       ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        for(::std::size_t local_function_idx{task_group.begin_index}; local_function_idx != task_group.end_index; ++local_function_idx)
        {
            compile_all_from_uwvm_local_func<CompileOption>(curr_module, options, storage, local_function_idx, err);
        }
    }

    struct parallel_compile_failure_state
    {
        ::std::atomic_bool failed{};
        ::std::atomic_flag err_claim = ATOMIC_FLAG_INIT;
        ::uwvm2::validation::error::code_validation_error_impl err{};
    };

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
    inline ::uwvm2::utils::thread::scheduled_task
        make_compile_all_from_uwvm_local_func_group_task(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                         ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option& options,
                                                         full_function_symbol_t& storage,
                                                         parallel_compile_failure_state& failure_state,
                                                         local_function_task_group task_group)
    {
#ifdef UWVM_CPP_EXCEPTIONS
        if(failure_state.failed.load(::std::memory_order_acquire)) { co_return; }
#endif

        ::uwvm2::validation::error::code_validation_error_impl local_err{};

#ifdef UWVM_CPP_EXCEPTIONS
        try
#endif
        {
            compile_all_from_uwvm_local_func_group<CompileOption>(curr_module, options, storage, task_group, local_err);
        }
#ifdef UWVM_CPP_EXCEPTIONS
        catch(::fast_io::error const&)
        {
            if(!failure_state.err_claim.test_and_set(::std::memory_order_acq_rel)) { failure_state.err = local_err; }
            failure_state.failed.store(true, ::std::memory_order_release);
        }
#endif

        co_return;
    }
}  // namespace details

inline constexpr ::std::size_t default_small_module_code_size_threshold{512uz * 1024uz};
inline constexpr ::std::size_t default_target_task_groups_per_compile_thread{4uz};
inline constexpr ::std::size_t default_target_task_groups_per_adjusted_compile_thread{4uz};
inline constexpr ::std::size_t aggressive_target_task_groups_per_adjusted_compile_thread{5uz};

[[nodiscard]] inline compile_task_split_config
    resolve_effective_compile_task_split_config(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                compile_task_split_config split_config,
                                                ::std::size_t extra_compile_threads) noexcept
{
    if(extra_compile_threads == 0uz || !split_config.adjust_for_default_policy || split_config.policy != compile_task_split_policy_t::code_size)
    {
        return split_config;
    }

    auto const total_code_size{details::calculate_total_local_function_task_weight(curr_module, split_config.policy)};
    if(total_code_size <= split_config.split_size || total_code_size > default_small_module_code_size_threshold) { return split_config; }

    auto const total_compile_threads{extra_compile_threads + 1uz};
    auto const target_task_group_count{total_compile_threads * default_target_task_groups_per_compile_thread};
    if(target_task_group_count == 0uz) [[unlikely]] { return split_config; }

    auto const adaptive_split_size{total_code_size / target_task_group_count + static_cast<::std::size_t>(total_code_size % target_task_group_count != 0uz)};

    if(adaptive_split_size > split_config.split_size) { split_config.split_size = adaptive_split_size; }
    return split_config;
}

[[nodiscard]] inline ::std::size_t resolve_effective_adaptive_extra_compile_threads(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                                                    compile_task_split_config split_config,
                                                                                    ::std::size_t extra_compile_threads_upper_bound,
                                                                                    ::std::size_t target_task_groups_per_adjusted_compile_thread,
                                                                                    bool split_was_adjusted) noexcept
{
    auto const useful_extra_compile_threads{
        ::uwvm2::utils::thread::clamp_extra_worker_count(details::calculate_local_function_task_group_count(curr_module, split_config),
                                                         extra_compile_threads_upper_bound)};
    if(useful_extra_compile_threads == 0uz || !split_was_adjusted || target_task_groups_per_adjusted_compile_thread == 0uz)
    {
        return useful_extra_compile_threads;
    }

    auto const task_group_count{details::calculate_local_function_task_group_count(curr_module, split_config)};
    if(task_group_count <= 1uz) { return 0uz; }

    auto const adjusted_total_compile_threads{task_group_count / target_task_groups_per_adjusted_compile_thread +
                                              static_cast<::std::size_t>(task_group_count % target_task_groups_per_adjusted_compile_thread != 0uz)};
    auto const adjusted_extra_compile_threads{adjusted_total_compile_threads > 1uz ? adjusted_total_compile_threads - 1uz : 0uz};
    return adjusted_extra_compile_threads < useful_extra_compile_threads ? adjusted_extra_compile_threads : useful_extra_compile_threads;
}

template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
inline ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t
    compile_all_from_uwvm(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                          [[maybe_unused]] ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option& options,
                          ::uwvm2::validation::error::code_validation_error_impl& err,
                          ::std::size_t extra_compile_threads,
                          compile_task_split_config split_config = {}) UWVM_THROWS
{
    ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t storage{};

    auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};
    details::initialize_local_defined_call_info(curr_module, options, storage);

    split_config = resolve_effective_compile_task_split_config(curr_module, split_config, extra_compile_threads);

    if(details::should_run_local_functions_serially(curr_module, split_config, extra_compile_threads))
    {
        for(::std::size_t local_function_idx{}; local_function_idx != local_func_count; ++local_function_idx)
        {
            details::compile_all_from_uwvm_local_func<CompileOption>(curr_module, options, storage, local_function_idx, err);
        }
        details::aggregate_local_function_storage(storage);

        using wasm_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;

#include "single_func_call_info.h"

        return storage;
    }

    auto const task_groups{details::build_local_function_task_groups(curr_module, split_config)};
    auto const effective_extra_compile_threads{::uwvm2::utils::thread::clamp_extra_worker_count(task_groups.size(), extra_compile_threads)};

    if(effective_extra_compile_threads == 0uz)
    {
        for(auto const& task_group: task_groups)
        {
            details::compile_all_from_uwvm_local_func_group<CompileOption>(curr_module, options, storage, task_group, err);
        }
    }
    else
    {
        ::uwvm2::utils::thread::scheduled_task_batch task_batch{task_groups.size()};

        details::parallel_compile_failure_state failure_state{};
        for(auto const& task_group: task_groups)
        {
            auto task{details::make_compile_all_from_uwvm_local_func_group_task<CompileOption>(curr_module, options, storage, failure_state, task_group)};
            ::std::construct_at(task_batch.handles.buffer + task_batch.handle_count, task.release());
            ++task_batch.handle_count;
        }

        ::uwvm2::utils::thread::native_thread_pool thread_pool{};
        thread_pool.run(task_batch, effective_extra_compile_threads);

#ifdef UWVM_CPP_EXCEPTIONS
        if(failure_state.failed.load(::std::memory_order_acquire))
        {
            err = failure_state.err;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
#endif
    }

    details::aggregate_local_function_storage(storage);

    using wasm_value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;

#include "single_func_call_info.h"

    return storage;
}

template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
inline ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t
    compile_all_from_uwvm_single_func(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                      [[maybe_unused]] ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option& options,
                                      ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
{ return compile_all_from_uwvm<CompileOption>(curr_module, options, err, 0uz); }
