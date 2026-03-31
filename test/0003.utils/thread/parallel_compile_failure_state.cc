/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      OpenAI
 * @version     2.0.0
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

// std
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <thread>
#include <vector>

// macro
#include <uwvm2/utils/macro/push_macros.h>

#ifndef UWVM_MODULE
// import
# include <uwvm2/runtime/compiler/uwvm_int/compile_all_from_uwvm/translate.h>
#else
# error "Module testing is not currently supported"
#endif

namespace
{
#ifdef UWVM_CPP_EXCEPTIONS
    namespace compile_details = ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::details;
    namespace validation_error = ::uwvm2::validation::error;

    struct tagged_exception
    {
        ::std::uint_least32_t tag{};
    };

    [[nodiscard]] ::std::exception_ptr make_tagged_exception_ptr(::std::uint_least32_t tag)
    {
        try
        {
            throw tagged_exception{tag};
        }
        catch(...)
        {
            return ::std::current_exception();
        }
    }

    [[nodiscard]] bool extract_tag(::std::exception_ptr const& exception, ::std::uint_least32_t& tag) noexcept
    {
        try
        {
            ::std::rethrow_exception(exception);
        }
        catch(tagged_exception const& ex)
        {
            tag = ex.tag;
            return true;
        }
        catch(...)
        {
            return false;
        }
    }

    [[nodiscard]] bool run_exception_only_case()
    {
        compile_details::parallel_compile_failure_state failure_state{};
        validation_error::code_validation_error_impl local_err{};
        local_err.err_code = validation_error::code_validation_error_code::invalid_function_index;
        local_err.err_selectable.u32arr[0] = 7u;

        compile_details::publish_parallel_compile_failure(failure_state, local_err, make_tagged_exception_ptr(11u), false);

        if(!failure_state.failed.load(::std::memory_order_acquire)) [[unlikely]] { return false; }
        if(failure_state.has_err) [[unlikely]] { return false; }
        if(failure_state.err.err_code != validation_error::code_validation_error_code::ok) [[unlikely]] { return false; }
        if(!failure_state.exception) [[unlikely]] { return false; }

        ::std::uint_least32_t tag{};
        return extract_tag(failure_state.exception, tag) && tag == 11u;
    }

    [[nodiscard]] bool run_error_and_exception_case()
    {
        compile_details::parallel_compile_failure_state failure_state{};
        validation_error::code_validation_error_impl local_err{};
        local_err.err_code = validation_error::code_validation_error_code::invalid_function_index;
        local_err.err_selectable.u32arr[0] = 23u;

        compile_details::publish_parallel_compile_failure(failure_state, local_err, make_tagged_exception_ptr(23u), true);

        if(!failure_state.failed.load(::std::memory_order_acquire)) [[unlikely]] { return false; }
        if(!failure_state.has_err) [[unlikely]] { return false; }
        if(failure_state.err.err_code != validation_error::code_validation_error_code::invalid_function_index) [[unlikely]] { return false; }
        if(failure_state.err.err_selectable.u32arr[0] != 23u) [[unlikely]] { return false; }
        if(!failure_state.exception) [[unlikely]] { return false; }

        ::std::uint_least32_t tag{};
        return extract_tag(failure_state.exception, tag) && tag == 23u;
    }

    [[nodiscard]] bool run_concurrent_first_claim_case()
    {
        compile_details::parallel_compile_failure_state failure_state{};
        ::std::atomic_bool start{};

        constexpr ::std::size_t thread_count{8uz};
        ::std::vector<::std::thread> workers{};
        workers.reserve(thread_count);

        for(::std::size_t i{}; i != thread_count; ++i)
        {
            workers.emplace_back([&failure_state, &start, token = static_cast<::std::uint_least32_t>(i + 1uz)]() noexcept
                                 {
                                     validation_error::code_validation_error_impl local_err{};
                                     local_err.err_code = validation_error::code_validation_error_code::invalid_function_index;
                                     local_err.err_selectable.u32arr[0] = token;

                                     while(!start.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }

                                     compile_details::publish_parallel_compile_failure(failure_state, local_err, make_tagged_exception_ptr(token), true);
                                 });
        }

        start.store(true, ::std::memory_order_release);

        for(auto& worker: workers) { worker.join(); }

        if(!failure_state.failed.load(::std::memory_order_acquire)) [[unlikely]] { return false; }
        if(!failure_state.has_err) [[unlikely]] { return false; }
        if(failure_state.err.err_code != validation_error::code_validation_error_code::invalid_function_index) [[unlikely]] { return false; }
        if(!failure_state.exception) [[unlikely]] { return false; }

        ::std::uint_least32_t tag{};
        if(!extract_tag(failure_state.exception, tag)) [[unlikely]] { return false; }
        if(tag == 0u || tag > thread_count) [[unlikely]] { return false; }
        return failure_state.err.err_selectable.u32arr[0] == tag;
    }
#endif
}  // namespace

int main()
{
#ifdef UWVM_CPP_EXCEPTIONS
    if(!run_exception_only_case()) [[unlikely]] { return 1; }
    if(!run_error_and_exception_case()) [[unlikely]] { return 2; }
    if(!run_concurrent_first_claim_case()) [[unlikely]] { return 3; }
#endif

    return 0;
}
