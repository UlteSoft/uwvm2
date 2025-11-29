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

#include <uwvm2/imported/wasi/wasip1/func/proc_exit.h>

namespace
{
    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;

    static wasm_i32 g_last_exit_code{};
    static bool g_exit_called{};

    void test_proc_exit_callback(wasm_i32 code) noexcept
    {
        g_exit_called = true;
        g_last_exit_code = code;
    }
}  // namespace

int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::exitcode_t;
    using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
    using ::uwvm2::object::memory::linear::native_memory_t;

    native_memory_t memory{};
    memory.init_by_page_count(1uz);

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = {},
                                            .envs = {},
                                            .fd_storage = {},
                                            .mount_dir_roots = {},
                                            .wasip1_proc_exit_func_ptr = &test_proc_exit_callback,
                                            .trace_wasip1_call = false};

    constexpr wasm_i32 expected_code{42};

    g_exit_called = false;
    g_last_exit_code = static_cast<wasm_i32>(0);

    auto const code = static_cast<exitcode_t>(expected_code);

    ::uwvm2::imported::wasi::wasip1::func::proc_exit(env, code);

    if(!g_exit_called)
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"proc_exit: callback not invoked");
        ::fast_io::fast_terminate();
    }

    if(g_last_exit_code != expected_code)
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"proc_exit: exit code mismatch");
        ::fast_io::fast_terminate();
    }
}
