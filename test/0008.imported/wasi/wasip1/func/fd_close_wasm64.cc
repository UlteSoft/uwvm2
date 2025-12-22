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

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/fd_close_wasm64.h>
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
    using ::uwvm2::imported::wasi::wasip1::abi::rights_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
    using ::uwvm2::object::memory::linear::native_memory_t;

    native_memory_t memory{};
    memory.init_by_page_count(1uz);

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = {},
                                            .envs = {},
                                            .fd_storage = {},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false};

    // Prepare fd table
    env.fd_storage.opens.resize(3uz);

    // Case 1: ebadf when fd is negative
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(-1));
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_close_wasm64: expected ebadf for negative fd");
            ::fast_io::fast_terminate();
        }
    }

    // Case 2: esuccess when closing a valid opened fd
    {
        auto& fd1{*env.fd_storage.opens.index_unchecked(1uz).fd_p};
        fd1.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fd1.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_close_wasm64.tmp", ::fast_io::open_mode::out};
        fd1.rights_base = static_cast<rights_t>(-1);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(1));
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_close_wasm64: expected esuccess for valid close");
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: double close should return ebadf
    {
        auto& fd2{*env.fd_storage.opens.index_unchecked(2uz).fd_p};
        fd2.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fd2.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_close_wasm64.tmp", ::fast_io::open_mode::out};
        fd2.rights_base = static_cast<rights_t>(-1);

        auto const first = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(2));
        if(first != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_close_wasm64: first close expected esuccess");
            ::fast_io::fast_terminate();
        }

        auto const second = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(2));
        if(second != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_close_wasm64: second close expected ebadf");
            ::fast_io::fast_terminate();
        }
    }

    // Case 4: renumber_map path — fd index out of opens range, exists in renumber_map → esuccess
    {
        constexpr wasi_posix_fd_wasm64_t kOutOfRangeFd{54321};
        env.fd_storage.renumber_map.emplace(kOutOfRangeFd, ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t{});
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, kOutOfRangeFd);
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_close_wasm64: renumber_map path expected esuccess");
            ::fast_io::fast_terminate();
        }
    }
}

