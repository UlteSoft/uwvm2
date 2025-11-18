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
#include <cstdint>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/fd_renumber.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_close.h>

int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::errno_t;
    using ::uwvm2::imported::wasi::wasip1::abi::rights_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t;
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
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_renumber(env, static_cast<wasi_posix_fd_t>(-1), static_cast<wasi_posix_fd_t>(0));
        if(ret != errno_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber: expected ebadf for negative fd_from");
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
                = ::fast_io::native_file{u8"test_fd_renumber_same.tmp", ::fast_io::open_mode::out};
            fd.rights_base = static_cast<rights_t>(-1);
        }
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_renumber(env, static_cast<wasi_posix_fd_t>(1), static_cast<wasi_posix_fd_t>(1));
        if(ret != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber: expected esuccess for same valid fd");
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
                = ::fast_io::native_file{u8"test_fd_renumber_from2.tmp", ::fast_io::open_mode::out};
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
                = ::fast_io::native_file{u8"test_fd_renumber_to3.tmp", ::fast_io::open_mode::out};
            fd.rights_base = static_cast<rights_t>(-1);
        }
        auto const c0 = ::uwvm2::imported::wasi::wasip1::func::fd_close(env, static_cast<wasi_posix_fd_t>(3));
        if(c0 != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber: setup close on to slot failed");
            ::fast_io::fast_terminate();
        }
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_renumber(env, static_cast<wasi_posix_fd_t>(2), static_cast<wasi_posix_fd_t>(3));
        if(ret != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber: expected esuccess for replacing closed to slot");
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
                = ::fast_io::native_file{u8"test_fd_renumber_from4.tmp", ::fast_io::open_mode::out};
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
                = ::fast_io::native_file{u8"test_fd_renumber_to5.tmp", ::fast_io::open_mode::out};
            fd.rights_base = static_cast<rights_t>(-1);
        }
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_renumber(env, static_cast<wasi_posix_fd_t>(4), static_cast<wasi_posix_fd_t>(5));
        if(ret != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber: expected esuccess for replacing active to slot");
            ::fast_io::fast_terminate();
        }
    }

    // Case 4: from in renumber_map, to beyond opens -> emplace in renumber_map and normalize
    {
        constexpr wasi_posix_fd_t kFrom{1000};
        ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t from_fd{};
        // Make it active
        from_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        from_fd.fd_p->wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_renumber_from_map.tmp", ::fast_io::open_mode::out};
        from_fd.fd_p->rights_base = static_cast<rights_t>(-1);
        env.fd_storage.renumber_map.emplace(kFrom, ::std::move(from_fd));

        // choose to beyond opens size, not present in renumber_map
        constexpr wasi_posix_fd_t kTo{1002};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_renumber(env, kFrom, kTo);
        if(ret != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber: expected esuccess for map->map path");
            ::fast_io::fast_terminate();
        }
    }

    // Case 5: invalid from (neither in opens active nor in map) -> ebadf
    {
        constexpr wasi_posix_fd_t kBadFrom{77777};
        constexpr wasi_posix_fd_t kTo{0};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_renumber(env, kBadFrom, kTo);
        if(ret != errno_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber: expected ebadf for invalid from");
            ::fast_io::fast_terminate();
        }
    }

    // Case 6: negative fd_to -> ebadf
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_renumber(env, static_cast<wasi_posix_fd_t>(0), static_cast<wasi_posix_fd_t>(-1));
        if(ret != errno_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber: expected ebadf for negative fd_to");
            ::fast_io::fast_terminate();
        }
    }

    // Case 7: same-fd but closed in opens -> ebadf
    {
        // ensure fd 1 exists and then close it
        {
            auto& fd{*env.fd_storage.opens.index_unchecked(1uz).fd_p};
            fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
            fd.wasi_fd.ptr->wasi_fd_storage.storage
                .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                .file
#endif
                = ::fast_io::native_file{u8"test_fd_renumber_same_closed.tmp", ::fast_io::open_mode::out};
            fd.rights_base = static_cast<rights_t>(-1);
        }
        auto const c = ::uwvm2::imported::wasi::wasip1::func::fd_close(env, static_cast<wasi_posix_fd_t>(1));
        if(c != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber: setup close for same-fd case failed");
            ::fast_io::fast_terminate();
        }
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_renumber(env, static_cast<wasi_posix_fd_t>(1), static_cast<wasi_posix_fd_t>(1));
        if(ret != errno_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber: expected ebadf for same-fd closed");
            ::fast_io::fast_terminate();
        }
    }

    // Case 8: same-fd when index >= opens.size() and exists in renumber_map -> esuccess
    {
        constexpr wasi_posix_fd_t kSameMap{123456};
        ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t p{};
        p.fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        p.fd_p->wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_renumber_same_map.tmp", ::fast_io::open_mode::out};
        p.fd_p->rights_base = static_cast<rights_t>(-1);
        env.fd_storage.renumber_map.emplace(kSameMap, ::std::move(p));
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_renumber(env, kSameMap, kSameMap);
        if(ret != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber: expected esuccess for same-fd in renumber_map");
            ::fast_io::fast_terminate();
        }
    }

    // Case 9: same-fd when index >= opens.size() and not in renumber_map -> ebadf
    {
        constexpr wasi_posix_fd_t kSameNoMap{234567};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_renumber(env, kSameNoMap, kSameNoMap);
        if(ret != errno_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber: expected ebadf for same-fd not in map");
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
        // make fd 1 active then close
        {
            auto& fd{*env2.fd_storage.opens.index_unchecked(1uz).fd_p};
            fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
            fd.wasi_fd.ptr->wasi_fd_storage.storage
                .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                .file
#endif
                = ::fast_io::native_file{u8"test_fd_renumber_from_closed.tmp", ::fast_io::open_mode::out};
            fd.rights_base = static_cast<rights_t>(-1);
        }
        auto const c = ::uwvm2::imported::wasi::wasip1::func::fd_close(env2, static_cast<wasi_posix_fd_t>(1));
        if(c != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber: setup close for from-closed failed");
            ::fast_io::fast_terminate();
        }
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_renumber(env2, static_cast<wasi_posix_fd_t>(1), static_cast<wasi_posix_fd_t>(2));
        if(ret != errno_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber: expected ebadf for from already closed");
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
        // from active at 1
        {
            auto& fd{*env2.fd_storage.opens.index_unchecked(1uz).fd_p};
            fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
            fd.wasi_fd.ptr->wasi_fd_storage.storage
                .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                .file
#endif
                = ::fast_io::native_file{u8"test_fd_renumber_to_map_replace_from.tmp", ::fast_io::open_mode::out};
            fd.rights_base = static_cast<rights_t>(-1);
        }
        // to in renumber_map at a large index
        constexpr wasi_posix_fd_t kToMap{5000};
        {
            ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t q{};
            q.fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
            q.fd_p->wasi_fd.ptr->wasi_fd_storage.storage
                .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                .file
#endif
                = ::fast_io::native_file{u8"test_fd_renumber_to_map_replace_to.tmp", ::fast_io::open_mode::out};
            q.fd_p->rights_base = static_cast<rights_t>(-1);
            env2.fd_storage.renumber_map.emplace(kToMap, ::std::move(q));
        }
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_renumber(env2, static_cast<wasi_posix_fd_t>(1), kToMap);
        if(ret != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber: expected esuccess for to in renumber_map replace");
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
        // add renumber_map entries at open_size and open_size+1
        for(std::size_t i = 0; i < 2; ++i)
        {
            ::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_unique_ptr_t r{};
            r.fd_p->wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
            r.fd_p->wasi_fd.ptr->wasi_fd_storage.storage
                .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                .file
#endif
                = ::fast_io::native_file{u8"test_fd_renumber_norm.tmp", ::fast_io::open_mode::out};
            r.fd_p->rights_base = static_cast<rights_t>(-1);
            env2.fd_storage.renumber_map.emplace(static_cast<wasi_posix_fd_t>(open_size_before + i), ::std::move(r));
        }
        // from active at 0; to beyond opens and not in renumber_map
        {
            auto& fd{*env2.fd_storage.opens.index_unchecked(0uz).fd_p};
            fd.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
            fd.wasi_fd.ptr->wasi_fd_storage.storage
                .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
                .file
#endif
                = ::fast_io::native_file{u8"test_fd_renumber_norm_from.tmp", ::fast_io::open_mode::out};
            fd.rights_base = static_cast<rights_t>(-1);
        }
        constexpr wasi_posix_fd_t kToBeyond{1000000};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_renumber(env2, static_cast<wasi_posix_fd_t>(0), kToBeyond);
        if(ret != errno_t::esuccess || env2.fd_storage.opens.size() != open_size_before + 2)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_renumber: expected normalization to move 2 entries");
            ::fast_io::fast_terminate();
        }
    }
}
