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

#include <uwvm2/imported/wasi/wasip1/func/environ_sizes_get_wasm64.h>
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
    using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
    using ::uwvm2::object::memory::linear::native_memory_t;
    using ::uwvm2::utils::container::u8string;
    using ::uwvm2::utils::container::u8string_view;
    using ::uwvm2::utils::container::vector;

    native_memory_t memory{};
    memory.init_by_page_count(1uz);

    u8string e0{u8"HELLO=WASM64"};
    u8string e1{u8"EMPTY="};
    vector<u8string_view> args;  // empty
    vector<u8string_view> envs;
    envs.push_back(u8string_view{e0.data(), e0.size()});
    envs.push_back(u8string_view{e1.data(), e1.size()});

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = args,
                                            .envs = envs,
                                            .fd_storage = {},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false};

    constexpr wasi_void_ptr_wasm64_t envc_ptr{32768u};
    constexpr wasi_void_ptr_wasm64_t envbuf_size_ptr{35840u};

    auto const ret{::uwvm2::imported::wasi::wasip1::func::environ_sizes_get_wasm64(env, envc_ptr, envbuf_size_ptr)};
    if(ret != errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }

    auto const got_envc{::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_size_wasm64_t>(memory, envc_ptr)};
    auto const got_bufsz{::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_size_wasm64_t>(memory, envbuf_size_ptr)};

    wasi_size_wasm64_t const expect_envc{2u};
    wasi_size_wasm64_t const expect_bufsz{static_cast<wasi_size_wasm64_t>(e0.size() + 1uz + e1.size() + 1uz)};

    if(got_envc != expect_envc)
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"environ_sizes_get_wasm64: envc mismatch");
        ::fast_io::fast_terminate();
    }
    if(got_bufsz != expect_bufsz)
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"environ_sizes_get_wasm64: env_buf_size mismatch");
        ::fast_io::fast_terminate();
    }
}
