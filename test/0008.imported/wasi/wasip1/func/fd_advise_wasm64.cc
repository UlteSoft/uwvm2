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

#include <uwvm2/imported/wasi/wasip1/func/fd_advise_wasm64.h>

// #include <uwvm2/imported/wasi/wasip1/func/fd_close_wasm64.h>
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
    using ::uwvm2::imported::wasi::wasip1::abi::advice_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t;
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

    // Prepare fd table: ensure indices [0..4] exist with valid entries
    env.fd_storage.opens.resize(5uz);

    // Case 1: success with valid rights (default constructed rights_base == all)
    {
        // ensure target fd has a valid native handle
        env.fd_storage.opens.index_unchecked(3uz).fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(
            ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        env.fd_storage.opens.index_unchecked(3uz)
            .fd_p->wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test.log", ::fast_io::open_mode::out};
        env.fd_storage.opens.index_unchecked(3uz).fd_p->rights_base = static_cast<rights_t>(-1);
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_advise_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 static_cast<filesize_wasm64_t>(0),
                                                                                 static_cast<filesize_wasm64_t>(0),
                                                                                 advice_wasm64_t::advice_normal);
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_advise_wasm64: expected esuccess for valid rights");
            ::fast_io::fast_terminate();
        }
    }

    // Case 2: enotcapable when rights do not include right_fd_advise
    {
        env.fd_storage.opens.index_unchecked(4uz).fd_p->rights_base = static_cast<rights_t>(0);
        // ensure the target fd has a valid native handle to avoid platform traps on fadvise
        env.fd_storage.opens.index_unchecked(4uz).fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(
            ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        env.fd_storage.opens.index_unchecked(4uz)
            .fd_p->wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test.log", ::fast_io::open_mode::out};

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_advise_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                 static_cast<filesize_wasm64_t>(0),
                                                                                 static_cast<filesize_wasm64_t>(0),
                                                                                 advice_wasm64_t::advice_normal);
        if(ret != errno_wasm64_t::enotcapable)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_advise_wasm64: expected enotcapable when rights missing");
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: ebadf for negative fd
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_advise_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(-1),
                                                                                 static_cast<filesize_wasm64_t>(0),
                                                                                 static_cast<filesize_wasm64_t>(0),
                                                                                 advice_wasm64_t::advice_normal);
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_advise_wasm64: expected ebadf for negative fd");
            ::fast_io::fast_terminate();
        }
    }

    // Case 4: einval for invalid advice enum
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_advise_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(3),
                                                                                 static_cast<filesize_wasm64_t>(0),
                                                                                 static_cast<filesize_wasm64_t>(0),
                                                                                 static_cast<advice_wasm64_t>(99));
        if(ret != errno_wasm64_t::einval)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_advise_wasm64: expected einval for invalid advice");
            ::fast_io::fast_terminate();
        }
    }

    // Case 5: ebadf after fd has been closed (simulate by setting close_pos)
    {
        auto& fd2 = *env.fd_storage.opens.index_unchecked(2uz).fd_p;
        fd2.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fd2.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_close_then_advise_wasm64.tmp", ::fast_io::open_mode::out};
        fd2.rights_base = static_cast<rights_t>(-1);
        fd2.close_pos = 0uz;
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_advise_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(2),
                                                                                 static_cast<filesize_wasm64_t>(0),
                                                                                 static_cast<filesize_wasm64_t>(0),
                                                                                 advice_wasm64_t::advice_normal);
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_advise_wasm64: expected ebadf after fd_close");
            ::fast_io::fast_terminate();
        }
    }
}

#else

int main() {}

#endif
