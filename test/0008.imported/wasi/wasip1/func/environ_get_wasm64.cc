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

#include <uwvm2/imported/wasi/wasip1/func/environ_get_wasm64.h>
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

#include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>

#if defined(UWVM_IMPORT_WASI_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1_WASM64)
int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
    using ::uwvm2::object::memory::linear::native_memory_t;
    using ::uwvm2::utils::container::u8string;
    using ::uwvm2::utils::container::u8string_view;
    using ::uwvm2::utils::container::vector;

    native_memory_t memory{};
    memory.init_by_page_count(1uz);

    u8string e0{u8"K=1"};
    u8string e1{u8"V=中文"};
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

    constexpr wasi_void_ptr_wasm64_t environ_ptr{8192u};
    constexpr wasi_void_ptr_wasm64_t environ_buf_ptr{16384u};

    auto const ret{::uwvm2::imported::wasi::wasip1::func::environ_get_wasm64(env, environ_ptr, environ_buf_ptr)};
    if(ret != errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }

    auto const p0{::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_void_ptr_wasm64_t>(memory, environ_ptr)};
    auto const p1{::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_void_ptr_wasm64_t>(
        memory,
        static_cast<wasi_void_ptr_wasm64_t>(environ_ptr + sizeof(wasi_void_ptr_wasm64_t)))};
    auto const pnull{::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_void_ptr_wasm64_t>(
        memory,
        static_cast<wasi_void_ptr_wasm64_t>(environ_ptr + 2u * sizeof(wasi_void_ptr_wasm64_t)))};

    auto const check_cstr = [&](wasi_void_ptr_wasm64_t off, u8string const& expect) -> bool
    {
        for(std::size_t i{}; i < expect.size(); ++i)
        {
            auto const ch =
                ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<unsigned char>(memory,
                                                                                                               static_cast<wasi_void_ptr_wasm64_t>(off + i));
            if(ch != static_cast<unsigned char>(expect[i])) { return false; }
        }
        auto const zero = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<unsigned char>(
            memory,
            static_cast<wasi_void_ptr_wasm64_t>(off + expect.size()));
        return zero == 0u;
    };

    if(!check_cstr(p0, e0)) { ::fast_io::fast_terminate(); }
    if(!check_cstr(p1, e1)) { ::fast_io::fast_terminate(); }
    if(pnull != static_cast<wasi_void_ptr_wasm64_t>(0u)) { ::fast_io::fast_terminate(); }
}

#else

int main() {}

#endif
