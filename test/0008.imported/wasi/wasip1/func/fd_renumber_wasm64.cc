/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT-5
 * @version     2.0.0
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

#include <uwvm2/imported/wasi/wasip1/func/fd_renumber_wasm64.h>
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

    // Prepare fd table with several entries
    env.fd_storage.opens.resize(6uz);

    // Case 0: negative fd -> ebadf
    {
        auto const ret =
            ::uwvm2::imported::wasi::wasip1::func::fd_renumber_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(-1), static_cast<wasi_posix_fd_wasm64_t>(0));
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber_wasm64: expected ebadf for negative fd_from");
            ::fast_io::fast_terminate();
        }
    }

    // Case 1: same fd (valid) -> esuccess
    {
        // init fd 1 active
        {
            auto& fd{*env.fd_storage.opens.index_unchecked(1uz).fd_p};
            fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
            fd.wasi_fd.ptr->wasi_fd_storage.storage
                .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                .file
#endif
                = ::fast_io::native_file{u8"test_fd_renumber_same_w64.tmp", ::fast_io::open_mode::out};
            fd.rights_base = static_cast<rights_t>(-1);
        }
        auto const ret =
            ::uwvm2::imported::wasi::wasip1::func::fd_renumber_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(1), static_cast<wasi_posix_fd_wasm64_t>(1));
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber_wasm64: expected esuccess for same valid fd");
            ::fast_io::fast_terminate();
        }
    }

    // Case 2: from in opens (active), to in opens (to is closed) -> replace closed slot and fix closes indices
    {
        // init from at 2 (active)
        {
            auto& fd{*env.fd_storage.opens.index_unchecked(2uz).fd_p};
            fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
            fd.wasi_fd.ptr->wasi_fd_storage.storage
                .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                .file
#endif
                = ::fast_io::native_file{u8"test_fd_renumber_from2_w64.tmp", ::fast_io::open_mode::out};
            fd.rights_base = static_cast<rights_t>(-1);
        }
        // mark to (3) as closed by calling fd_close on it
        {
            auto& fd{*env.fd_storage.opens.index_unchecked(3uz).fd_p};
            fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
            fd.wasi_fd.ptr->wasi_fd_storage.storage
                .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                .file
#endif
                = ::fast_io::native_file{u8"test_fd_renumber_to3_w64.tmp", ::fast_io::open_mode::out};
            fd.rights_base = static_cast<rights_t>(-1);
        }
        auto const c0 = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(3));
        if(c0 != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber_wasm64: setup close on to slot failed");
            ::fast_io::fast_terminate();
        }
        auto const ret =
            ::uwvm2::imported::wasi::wasip1::func::fd_renumber_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(2), static_cast<wasi_posix_fd_wasm64_t>(3));
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber_wasm64: expected esuccess for replacing closed to slot");
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: from in opens (active), to in opens (to is active) -> wait and replace, delayed destroy
    {
        {
            auto& fd{*env.fd_storage.opens.index_unchecked(4uz).fd_p};
            fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
            fd.wasi_fd.ptr->wasi_fd_storage.storage
                .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                .file
#endif
                = ::fast_io::native_file{u8"test_fd_renumber_from4_w64.tmp", ::fast_io::open_mode::out};
            fd.rights_base = static_cast<rights_t>(-1);
        }
        {
            auto& fd{*env.fd_storage.opens.index_unchecked(5uz).fd_p};
            fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
            fd.wasi_fd.ptr->wasi_fd_storage.storage
                .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                .file
#endif
                = ::fast_io::native_file{u8"test_fd_renumber_to5_w64.tmp", ::fast_io::open_mode::out};
            fd.rights_base = static_cast<rights_t>(-1);
        }
        auto const ret =
            ::uwvm2::imported::wasi::wasip1::func::fd_renumber_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(4), static_cast<wasi_posix_fd_wasm64_t>(5));
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber_wasm64: expected esuccess for replacing active to slot");
            ::fast_io::fast_terminate();
        }
    }

    // Case 4: from in renumber_map, to beyond opens -> emplace in renumber_map and normalize
    {
        constexpr wasi_posix_fd_wasm64_t kFrom{100000};
        ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t from_fd{};
        // Make it active
        from_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        from_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_renumber_from_map_w64.tmp", ::fast_io::open_mode::out};
        from_fd.fd_p->rights_base = static_cast<rights_t>(-1);
        env.fd_storage.renumber_map.emplace(kFrom, ::std::move(from_fd));

        // choose to beyond opens size, not present in renumber_map
        constexpr wasi_posix_fd_wasm64_t kTo{100002};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_renumber_wasm64(env, kFrom, kTo);
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber_wasm64: expected esuccess for map->map path");
            ::fast_io::fast_terminate();
        }
    }

    // Case 5: invalid from (neither in opens active nor in map) -> ebadf
    {
        constexpr wasi_posix_fd_wasm64_t kBadFrom{777770};
        constexpr wasi_posix_fd_wasm64_t kTo{0};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_renumber_wasm64(env, kBadFrom, kTo);
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber_wasm64: expected ebadf for invalid from");
            ::fast_io::fast_terminate();
        }
    }

    // Case 6: negative fd_to -> ebadf
    {
        auto const ret =
            ::uwvm2::imported::wasi::wasip1::func::fd_renumber_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(0), static_cast<wasi_posix_fd_wasm64_t>(-1));
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber_wasm64: expected ebadf for negative fd_to");
            ::fast_io::fast_terminate();
        }
    }

    // Case 7: same-fd but closed in opens -> ebadf
    {
        {
            auto& fd{*env.fd_storage.opens.index_unchecked(1uz).fd_p};
            fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
            fd.wasi_fd.ptr->wasi_fd_storage.storage
                .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                .file
#endif
                = ::fast_io::native_file{u8"test_fd_renumber_same_closed_w64.tmp", ::fast_io::open_mode::out};
            fd.rights_base = static_cast<rights_t>(-1);
        }
        auto const c = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(1));
        if(c != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber_wasm64: setup close for same-fd case failed");
            ::fast_io::fast_terminate();
        }
        auto const ret =
            ::uwvm2::imported::wasi::wasip1::func::fd_renumber_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(1), static_cast<wasi_posix_fd_wasm64_t>(1));
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber_wasm64: expected ebadf for same-fd closed");
            ::fast_io::fast_terminate();
        }
    }

    // Case 8: same-fd when index >= opens.size() and exists in renumber_map -> esuccess
    {
        constexpr wasi_posix_fd_wasm64_t kSameMap{123456000};
        ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t p{};
        p.fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        p.fd_p->wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_renumber_same_map_w64.tmp", ::fast_io::open_mode::out};
        p.fd_p->rights_base = static_cast<rights_t>(-1);
        env.fd_storage.renumber_map.emplace(kSameMap, ::std::move(p));
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_renumber_wasm64(env, kSameMap, kSameMap);
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber_wasm64: expected esuccess for same-fd in renumber_map");
            ::fast_io::fast_terminate();
        }
    }

    // Case 9: same-fd when index >= opens.size() and not in renumber_map -> ebadf
    {
        constexpr wasi_posix_fd_wasm64_t kSameNoMap{234567000};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_renumber_wasm64(env, kSameNoMap, kSameNoMap);
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber_wasm64: expected ebadf for same-fd not in map");
            ::fast_io::fast_terminate();
        }
    }

    // Case 10: from in opens but already closed -> ebadf
    {
        wasip1_environment<native_memory_t> env2{.wasip1_memory = ::std::addressof(memory),
                                                 .argv = {},
                                                 .envs = {},
                                                 .fd_storage = {},
                                                 .mount_dir_roots = {},
                                                 .trace_wasip1_call = false};
        env2.fd_storage.opens.resize(4uz);
        {
            auto& fd{*env2.fd_storage.opens.index_unchecked(1uz).fd_p};
            fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
            fd.wasi_fd.ptr->wasi_fd_storage.storage
                .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                .file
#endif
                = ::fast_io::native_file{u8"test_fd_renumber_from_closed_w64.tmp", ::fast_io::open_mode::out};
            fd.rights_base = static_cast<rights_t>(-1);
        }
        auto const c = ::uwvm2::imported::wasi::wasip1::func::fd_close_wasm64(env2, static_cast<wasi_posix_fd_wasm64_t>(1));
        if(c != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber_wasm64: setup close for from-closed failed");
            ::fast_io::fast_terminate();
        }
        auto const ret =
            ::uwvm2::imported::wasi::wasip1::func::fd_renumber_wasm64(env2, static_cast<wasi_posix_fd_wasm64_t>(1), static_cast<wasi_posix_fd_wasm64_t>(2));
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber_wasm64: expected ebadf for from already closed");
            ::fast_io::fast_terminate();
        }
    }

    // Case 11: to beyond opens and exists in renumber_map -> replacement path
    {
        wasip1_environment<native_memory_t> env2{.wasip1_memory = ::std::addressof(memory),
                                                 .argv = {},
                                                 .envs = {},
                                                 .fd_storage = {},
                                                 .mount_dir_roots = {},
                                                 .trace_wasip1_call = false};
        env2.fd_storage.opens.resize(3uz);
        {
            auto& fd{*env2.fd_storage.opens.index_unchecked(1uz).fd_p};
            fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
            fd.wasi_fd.ptr->wasi_fd_storage.storage
                .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                .file
#endif
                = ::fast_io::native_file{u8"test_fd_renumber_to_map_replace_from_w64.tmp", ::fast_io::open_mode::out};
            fd.rights_base = static_cast<rights_t>(-1);
        }
        constexpr wasi_posix_fd_wasm64_t kToMap{500000};
        {
            ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t q{};
            q.fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
            q.fd_p->wasi_fd.ptr->wasi_fd_storage.storage
                .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                .file
#endif
                = ::fast_io::native_file{u8"test_fd_renumber_to_map_replace_to_w64.tmp", ::fast_io::open_mode::out};
            q.fd_p->rights_base = static_cast<rights_t>(-1);
            env2.fd_storage.renumber_map.emplace(kToMap, ::std::move(q));
        }
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_renumber_wasm64(env2, static_cast<wasi_posix_fd_wasm64_t>(1), kToMap);
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber_wasm64: expected esuccess for to in renumber_map replace");
            ::fast_io::fast_terminate();
        }
    }

    // Case 12: normalization loop moves contiguous renumber_map entries at opens.size()
    {
        wasip1_environment<native_memory_t> env2{.wasip1_memory = ::std::addressof(memory),
                                                 .argv = {},
                                                 .envs = {},
                                                 .fd_storage = {},
                                                 .mount_dir_roots = {},
                                                 .trace_wasip1_call = false};
        env2.fd_storage.opens.resize(4uz);
        auto const open_size_before = env2.fd_storage.opens.size();
        for(std::size_t i = 0; i < 2; ++i)
        {
            ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t r{};
            r.fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
            r.fd_p->wasi_fd.ptr->wasi_fd_storage.storage
                .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                .file
#endif
                = ::fast_io::native_file{u8"test_fd_renumber_norm_w64.tmp", ::fast_io::open_mode::out};
            r.fd_p->rights_base = static_cast<rights_t>(-1);
            env2.fd_storage.renumber_map.emplace(static_cast<wasi_posix_fd_wasm64_t>(open_size_before + i), ::std::move(r));
        }
        {
            auto& fd{*env2.fd_storage.opens.index_unchecked(0uz).fd_p};
            fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
            fd.wasi_fd.ptr->wasi_fd_storage.storage
                .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                .file
#endif
                = ::fast_io::native_file{u8"test_fd_renumber_norm_from_w64.tmp", ::fast_io::open_mode::out};
            fd.rights_base = static_cast<rights_t>(-1);
        }
        constexpr wasi_posix_fd_wasm64_t kToBeyond{100000000};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_renumber_wasm64(env2, static_cast<wasi_posix_fd_wasm64_t>(0), kToBeyond);
        if(ret != errno_wasm64_t::esuccess || env2.fd_storage.opens.size() != open_size_before + 2)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber_wasm64: expected normalization to move 2 entries");
            ::fast_io::fast_terminate();
        }
    }
}
