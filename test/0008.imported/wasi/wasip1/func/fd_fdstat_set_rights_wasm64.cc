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

#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_set_rights_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_get_wasm64.h>
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

#include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>

#if defined(UWVM_IMPORT_WASI_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1_WASM64)
int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t;
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
    env.fd_storage.opens.resize(8uz);

    // Case 0: negative fd => ebadf
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_set_rights_wasm64(env,
                                                                                            static_cast<wasi_posix_fd_wasm64_t>(-1),
                                                                                            static_cast<rights_wasm64_t>(0),
                                                                                            static_cast<rights_wasm64_t>(0));
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_set_rights_wasm64: expected ebadf for negative fd");
            ::fast_io::fast_terminate();
        }
    }

    // Setup fd=4 with valid file and all rights
    auto& fd4 = *env.fd_storage.opens.index_unchecked(4uz).fd_p;
    fd4.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
    fd4.wasi_fd.ptr->wasi_fd_storage.storage
        .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
        .file
#endif
        = ::fast_io::native_file{u8"test_fd_fdstat_set_rights_wasm64.tmp",
                                 ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
    fd4.rights_base = static_cast<rights_wasm64_t>(-1);
    fd4.rights_inherit = static_cast<rights_wasm64_t>(-1);

    // Read back helper via fd_fdstat_get_wasm64
    auto read_rights = [&](wasi_posix_fd_wasm64_t fd, rights_wasm64_t& out_base, rights_wasm64_t& out_inh)
    {
        constexpr wasi_void_ptr_wasm64_t stat_ptr{4096u};
        auto const gr = ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_get_wasm64(env, fd, stat_ptr);
        if(gr != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_set_rights_wasm64: fd_fdstat_get expected esuccess");
            ::fast_io::fast_terminate();
        }
        using u_rights = ::std::underlying_type_t<rights_wasm64_t>;
        u_rights const base_u =
            ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<u_rights>(memory,
                                                                                                      static_cast<wasi_void_ptr_wasm64_t>(stat_ptr + 8u));
        u_rights const inh_u =
            ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<u_rights>(memory,
                                                                                                      static_cast<wasi_void_ptr_wasm64_t>(stat_ptr + 16u));
        out_base = static_cast<rights_wasm64_t>(base_u);
        out_inh = static_cast<rights_wasm64_t>(inh_u);
    };

    // Case 1: shrink base only (success)
    {
        rights_wasm64_t const new_base = rights_wasm64_t::right_fd_read;
        rights_wasm64_t const new_inh = static_cast<rights_wasm64_t>(-1);  // keep inheriting unchanged
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_set_rights_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(4), new_base, new_inh);
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_set_rights_wasm64: shrink base expected esuccess");
            ::fast_io::fast_terminate();
        }

        rights_wasm64_t rb{}, ri{};
        read_rights(static_cast<wasi_posix_fd_wasm64_t>(4), rb, ri);
        if(rb != new_base || ri != new_inh)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_set_rights_wasm64: shrink base verification failed");
            ::fast_io::fast_terminate();
        }
    }

    // Case 2: shrink inheriting only (success), base unchanged
    {
        rights_wasm64_t curr_base{}, curr_inh{};
        read_rights(static_cast<wasi_posix_fd_wasm64_t>(4), curr_base, curr_inh);

        rights_wasm64_t const new_base = curr_base;                       // unchanged
        rights_wasm64_t const new_inh = static_cast<rights_wasm64_t>(0);  // shrink to zero
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_set_rights_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(4), new_base, new_inh);
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_set_rights_wasm64: shrink inheriting expected esuccess");
            ::fast_io::fast_terminate();
        }

        rights_wasm64_t rb{}, ri{};
        read_rights(static_cast<wasi_posix_fd_wasm64_t>(4), rb, ri);
        if(rb != new_base || ri != new_inh)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_set_rights_wasm64: shrink inheriting verification failed");
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: attempt to increase base (should be enotcapable)
    {
        rights_wasm64_t curr_base{}, curr_inh{};
        read_rights(static_cast<wasi_posix_fd_wasm64_t>(4), curr_base, curr_inh);

        rights_wasm64_t const new_base = static_cast<rights_wasm64_t>(static_cast<::std::underlying_type_t<rights_wasm64_t>>(curr_base) |
                                                                      static_cast<::std::underlying_type_t<rights_wasm64_t>>(rights_wasm64_t::right_fd_write));
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_set_rights_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(4), new_base, curr_inh);
        if(ret != errno_wasm64_t::enotcapable)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_set_rights_wasm64: increase base expected enotcapable");
            ::fast_io::fast_terminate();
        }
    }

    // Case 4: attempt to increase inheriting (should be enotcapable)
    {
        rights_wasm64_t curr_base{}, curr_inh{};
        read_rights(static_cast<wasi_posix_fd_wasm64_t>(4), curr_base, curr_inh);

        rights_wasm64_t const inc_inh = rights_wasm64_t::right_fd_write;
        rights_wasm64_t const new_inh = static_cast<rights_wasm64_t>(static_cast<::std::underlying_type_t<rights_wasm64_t>>(curr_inh) |
                                                                     static_cast<::std::underlying_type_t<rights_wasm64_t>>(inc_inh));
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_set_rights_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(4), curr_base, new_inh);
        if(ret != errno_wasm64_t::enotcapable)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_set_rights_wasm64: increase inheriting expected enotcapable");
            ::fast_io::fast_terminate();
        }
    }

    // Case 5: inheriting may contain bits that base does not (allowed by spec)
    {
        auto& fd5 = *env.fd_storage.opens.index_unchecked(5uz).fd_p;
        fd5.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fd5.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_fdstat_set_rights_wasm64_case5.tmp",
                                     ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
        fd5.rights_base = static_cast<rights_wasm64_t>(0);
        fd5.rights_inherit = rights_wasm64_t::right_fd_write;  // inheriting not subset of base

        rights_wasm64_t const new_base = fd5.rights_base;    // unchanged
        rights_wasm64_t const new_inh = fd5.rights_inherit;  // unchanged

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_set_rights_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(5), new_base, new_inh);
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_set_rights_wasm64: inheriting-not-subset-of-base scenario should succeed");
            ::fast_io::fast_terminate();
        }

        rights_wasm64_t rb{}, ri{};
        read_rights(static_cast<wasi_posix_fd_wasm64_t>(5), rb, ri);
        if(rb != new_base || ri != new_inh)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_set_rights_wasm64: case5 verification failed");
            ::fast_io::fast_terminate();
        }
    }

    // Case 6: ebadf after fd_close
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(2uz).fd_p;
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fde.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_fdstat_set_rights_wasm64_close.tmp",
                                     ::fast_io::open_mode::out | ::fast_io::open_mode::creat | ::fast_io::open_mode::trunc};
        fde.rights_base = static_cast<rights_wasm64_t>(-1);
        fde.rights_inherit = static_cast<rights_wasm64_t>(-1);

        auto const closed = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(2));
        if(closed != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_set_rights_wasm64: close expected esuccess");
            ::fast_io::fast_terminate();
        }

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_fdstat_set_rights_wasm64(env,
                                                                                            static_cast<wasi_posix_fd_wasm64_t>(2),
                                                                                            static_cast<rights_wasm64_t>(0),
                                                                                            static_cast<rights_wasm64_t>(0));
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_fdstat_set_rights_wasm64: expected ebadf after fd_close");
            ::fast_io::fast_terminate();
        }
    }
}

#else

int main() {}

#endif
