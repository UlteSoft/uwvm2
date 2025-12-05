/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT-5
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

#include <cstddef>
#include <cstdint>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/proc_raise.h>

namespace
{
    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    using errno_t = ::uwvm2::imported::wasi::wasip1::abi::errno_t;

    static wasm_i32 g_last_signal{};
    static bool g_raise_called{};
    static errno_t g_next_return_value{};

    errno_t test_proc_raise_callback(wasm_i32 code) noexcept
    {
        g_raise_called = true;
        g_last_signal = code;
        return g_next_return_value;
    }
}  // namespace

int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::errno_t;
    using ::uwvm2::imported::wasi::wasip1::abi::signal_t;
    using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
    using ::uwvm2::object::memory::linear::native_memory_t;

    native_memory_t memory{};
    memory.init_by_page_count(1uz);

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = {},
                                            .envs = {},
                                            .fd_storage = {},
                                            .mount_dir_roots = {},
                                            .wasip1_proc_exit_func_ptr = nullptr,
                                            .trace_wasip1_call = false};

    env.wasip1_proc_raise_func_ptr = &test_proc_raise_callback;

    constexpr signal_t signal_code{signal_t::sigterm};

    g_raise_called = false;
    g_last_signal = static_cast<wasm_i32>(0);
    g_next_return_value = errno_t::eafnosupport;

    auto const err = ::uwvm2::imported::wasi::wasip1::func::proc_raise(env, signal_code);

    if(!g_raise_called)
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"proc_raise: callback not invoked");
        ::fast_io::fast_terminate();
    }

    if(g_last_signal != static_cast<wasm_i32>(signal_code))
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"proc_raise: signal code mismatch");
        ::fast_io::fast_terminate();
    }

    if(err != static_cast<errno_t>(g_next_return_value))
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"proc_raise: errno mismatch");
        ::fast_io::fast_terminate();
    }
}
