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

#include <uwvm2/imported/wasi/wasip1/func/environ_sizes_get.h>
#ifdef UWVM_DLLIMPORT
# error "UWVM_DLLIMPORT existed"
#endif

#ifdef UWVM_WASM_SUPPORT_WASM1
# error "UWVM_WASM_SUPPORT_WASM1 existed"
#endif

#ifdef UWVM_AES_RST_ALL
# error "UWVM_AES_RST_ALL existed"
#endif

#ifdef UWVM_COLOR_RST_ALL
# error "UWVM_COLOR_RST_ALL existed"
#endif

#ifdef UWVM_WIN32_TEXTATTR_RST_ALL
# error "UWVM_WIN32_TEXTATTR_RST_ALL existed"
#endif

#ifdef UWVM_IMPORT_WASI
# error "UWVM_IMPORT_WASI existed"
#endif

#ifdef UWVM_IMPORT_WASI_WASIP1
# error "UWVM_IMPORT_WASI_WASIP1 existed"
#endif
int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::errno_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t;
    using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
    using ::uwvm2::object::memory::linear::native_memory_t;
    using ::uwvm2::utils::container::u8string;
    using ::uwvm2::utils::container::u8string_view;
    using ::uwvm2::utils::container::vector;

    native_memory_t memory{};
    memory.init_by_page_count(1uz);

    // envs with KEY=VALUE shape, include empty value case and single-char key
    u8string e0{u8"A=1"};
    u8string e1{u8"EMPTY="};
    u8string e2{u8"Z=x"};
    vector<u8string_view> args;  // empty
    vector<u8string_view> envs;
    envs.push_back(u8string_view{e0.data(), e0.size()});
    envs.push_back(u8string_view{e1.data(), e1.size()});
    envs.push_back(u8string_view{e2.data(), e2.size()});

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = args,
                                            .envs = envs,
                                            .fd_storage = {},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false};

    constexpr wasi_void_ptr_t envc_ptr{1024u};
    constexpr wasi_void_ptr_t envbuf_size_ptr{2048u};

    auto const ret{::uwvm2::imported::wasi::wasip1::func::environ_sizes_get(env, envc_ptr, envbuf_size_ptr)};
    if(ret != errno_t::esuccess) { ::fast_io::fast_terminate(); }

    auto const got_envc{::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, envc_ptr)};
    auto const got_bufsz{::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, envbuf_size_ptr)};

    wasi_size_t const expect_envc{3u};
    wasi_size_t const expect_bufsz{static_cast<wasi_size_t>(e0.size() + 1uz + e1.size() + 1uz + e2.size() + 1uz)};

    if(got_envc != expect_envc)
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"environ_sizes_get: envc mismatch");
        ::fast_io::fast_terminate();
    }
    if(got_bufsz != expect_bufsz)
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"environ_sizes_get: env_buf_size mismatch");
        ::fast_io::fast_terminate();
    }

    // extra: empty envs should report 0/0
    env.envs.clear();
    auto const ret2{::uwvm2::imported::wasi::wasip1::func::environ_sizes_get(env, envc_ptr, envbuf_size_ptr)};
    if(ret2 != errno_t::esuccess) { ::fast_io::fast_terminate(); }

    auto const got_envc2{::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, envc_ptr)};
    auto const got_bufsz2{::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, envbuf_size_ptr)};
    if(got_envc2 != static_cast<wasi_size_t>(0u) || got_bufsz2 != static_cast<wasi_size_t>(0u))
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"environ_sizes_get: empty envs mismatch");
        ::fast_io::fast_terminate();
    }
}

