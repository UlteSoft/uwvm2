namespace details
{
    using full_function_symbol_t = ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t;

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

    struct parallel_compile_failure_state
    {
        ::std::atomic_bool failed{};
        ::std::atomic_flag err_claim = ATOMIC_FLAG_INIT;
        ::uwvm2::validation::error::code_validation_error_impl err{};
    };

    template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
    inline ::uwvm2::utils::thread::scheduled_task
        make_compile_all_from_uwvm_local_func_task(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                                   ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option& options,
                                                   full_function_symbol_t& storage,
                                                   parallel_compile_failure_state& failure_state,
                                                   ::std::size_t compile_local_function_idx)
    {
#ifdef UWVM_CPP_EXCEPTIONS
        if(failure_state.failed.load(::std::memory_order_acquire)) { co_return; }
#endif

        ::uwvm2::validation::error::code_validation_error_impl local_err{};

#ifdef UWVM_CPP_EXCEPTIONS
        try
#endif
        {
            compile_all_from_uwvm_local_func<CompileOption>(curr_module, options, storage, compile_local_function_idx, local_err);
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

template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
inline ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t
    compile_all_from_uwvm(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                          [[maybe_unused]] ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option& options,
                          ::uwvm2::validation::error::code_validation_error_impl& err,
                          ::std::size_t extra_compile_threads) UWVM_THROWS
{
    ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t storage{};

    auto const local_func_count{curr_module.local_defined_function_vec_storage.size()};
    details::initialize_local_defined_call_info(curr_module, options, storage);

    auto const effective_extra_compile_threads{::uwvm2::utils::thread::clamp_extra_worker_count(local_func_count, extra_compile_threads)};

    if(effective_extra_compile_threads == 0uz)
    {
        for(::std::size_t local_function_idx{}; local_function_idx != local_func_count; ++local_function_idx)
        {
            details::compile_all_from_uwvm_local_func<CompileOption>(curr_module, options, storage, local_function_idx, err);
        }
    }
    else
    {
        ::uwvm2::utils::thread::scheduled_task_batch task_batch{local_func_count};

        details::parallel_compile_failure_state failure_state{};
        for(::std::size_t local_function_idx{}; local_function_idx != local_func_count; ++local_function_idx)
        {
            auto task{details::make_compile_all_from_uwvm_local_func_task<CompileOption>(curr_module, options, storage, failure_state, local_function_idx)};
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
}

template <::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_translate_option_t CompileOption>
inline ::uwvm2::runtime::compiler::uwvm_int::optable::uwvm_interpreter_full_function_symbol_t
    compile_all_from_uwvm_single_func(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& curr_module,
                                      [[maybe_unused]] ::uwvm2::runtime::compiler::uwvm_int::optable::compile_option& options,
                                      ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
{ return compile_all_from_uwvm<CompileOption>(curr_module, options, err, 0uz); }
