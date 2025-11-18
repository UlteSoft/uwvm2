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

#include <uwvm2/imported/wasi/wasip1/func/fd_filestat_set_times.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_filestat_get.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_close.h>

int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::errno_t;
    using ::uwvm2::imported::wasi::wasip1::abi::fstflags_t;
    using ::uwvm2::imported::wasi::wasip1::abi::rights_t;
    using ::uwvm2::imported::wasi::wasip1::abi::timestamp_t;
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

    env.fd_storage.opens.resize(6uz);

    // Case 0: negative fd → ebadf
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_set_times(env,
                                                                                      static_cast<wasi_posix_fd_t>(-1),
                                                                                      static_cast<timestamp_t>(0),
                                                                                      static_cast<timestamp_t>(0),
                                                                                      static_cast<fstflags_t>(0));
        if(ret != errno_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_set_times: expected ebadf for negative fd");
            ::fast_io::fast_terminate();
        }
    }

    // Prepare a regular file at fd=4 with full rights (new fd model)
    {
        auto& fd4 = *env.fd_storage.opens.index_unchecked(4uz).fd_p;
        fd4.rights_base = static_cast<rights_t>(-1);
        fd4.rights_inherit = static_cast<rights_t>(-1);
        fd4.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fd4.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_filestat_set_times_regular.tmp",
                                     ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
    }

    // Case 1: conflict flags atim_now+atim → einval
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_set_times(
            env,
            static_cast<wasi_posix_fd_t>(4),
            static_cast<timestamp_t>(123u),
            static_cast<timestamp_t>(0u),
            static_cast<fstflags_t>(fstflags_t::filestat_set_atim_now | fstflags_t::filestat_set_atim));
        if(ret != errno_t::einval)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_set_times: expected einval for conflict flags atim_now+atim");
            ::fast_io::fast_terminate();
        }
    }

    // Case 2: set mtim_now only → esuccess
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_set_times(env,
                                                                                      static_cast<wasi_posix_fd_t>(4),
                                                                                      static_cast<timestamp_t>(0u),
                                                                                      static_cast<timestamp_t>(0u),
                                                                                      static_cast<fstflags_t>(fstflags_t::filestat_set_mtim_now));
        if(ret == errno_t::enosys)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_set_times: enosys");
            return 0;
        }
        else if(ret != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_set_times: expected esuccess for mtim_now");
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: set atim/mtim to specific values → esuccess
#if defined(__MSDOS__) || defined(__DJGPP__)
    // On MSDOS/DJGPP, skip atime-specific testing; verify only mtim.
    {
        timestamp_t const mt = static_cast<timestamp_t>(1'000'000'000ull * 20ull + 456ull);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_set_times(env,
                                                                                      static_cast<wasi_posix_fd_t>(4),
                                                                                      static_cast<timestamp_t>(0u),
                                                                                      mt,
                                                                                      static_cast<fstflags_t>(fstflags_t::filestat_set_mtim));
        if(ret != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_set_times: expected esuccess for setting explicit mtim (DOS)");
            ::fast_io::fast_terminate();
        }

        // Verify via fd_filestat_get with 100ns precision alignment
        constexpr ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t stat_ptr{4096u};
        auto const gr = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_get(env, static_cast<wasi_posix_fd_t>(4), stat_ptr);
        if(gr != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_set_times: fd_filestat_get expected esuccess");
            ::fast_io::fast_terminate();
        }

        using u_timestamp = ::std::underlying_type_t<timestamp_t>;
        auto const got_mt = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<u_timestamp>(
            memory,
            static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>(stat_ptr + 48u));

        auto const q100 = [](u_timestamp ns) constexpr -> u_timestamp { return static_cast<u_timestamp>((ns / 1'000'000'000u) * 1'000'000'000u); };

        if(q100(got_mt) != q100(static_cast<u_timestamp>(mt)))
        {
            ::fast_io::io::perrln(::fast_io::u8err(),
                                  u8"fd_filestat_set_times: mtim mismatch (with precision, DOS): ",
                                  q100(got_mt),
                                  u8" ",
                                  q100(static_cast<u_timestamp>(mt)));
            ::fast_io::fast_terminate();
        }
    }
#else
    {
        timestamp_t const at = static_cast<timestamp_t>(1'000'000'000ull * 10ull + 123ull);
        timestamp_t const mt = static_cast<timestamp_t>(1'000'000'000ull * 20ull + 456ull);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_set_times(
            env,
            static_cast<wasi_posix_fd_t>(4),
            at,
            mt,
            static_cast<fstflags_t>(fstflags_t::filestat_set_atim | fstflags_t::filestat_set_mtim));
        if(ret != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_set_times: expected esuccess for setting explicit atim/mtim");
            ::fast_io::fast_terminate();
        }

        // Verify via fd_filestat_get with 100ns precision alignment
        constexpr ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t stat_ptr{4096u};
        auto const gr = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_get(env, static_cast<wasi_posix_fd_t>(4), stat_ptr);
        if(gr != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_set_times: fd_filestat_get expected esuccess");
            ::fast_io::fast_terminate();
        }

        using u_timestamp = ::std::underlying_type_t<timestamp_t>;
        auto const got_at = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<u_timestamp>(
            memory,
            static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>(stat_ptr + 40u));
        auto const got_mt = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<u_timestamp>(
            memory,
            static_cast<::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t>(stat_ptr + 48u));

# if defined(_WIN32) || defined(__CYGWIN__)
        auto const q100 = [](u_timestamp ns) constexpr -> u_timestamp { return static_cast<u_timestamp>((ns / 100u) * 100u); };
# else
        auto const q100 = [](u_timestamp ns) constexpr -> u_timestamp { return static_cast<u_timestamp>(ns); };
# endif

        if(q100(got_at) != q100(static_cast<u_timestamp>(at)) || q100(got_mt) != q100(static_cast<u_timestamp>(mt)))
        {
            ::fast_io::io::perrln(::fast_io::u8err(),
                                  u8"fd_filestat_set_times: atim/mtim mismatch (with precision): ",
                                  q100(got_at),
                                  u8" ",
                                  q100(static_cast<u_timestamp>(at)),
                                  u8" ",
                                  q100(got_mt),
                                  u8" ",
                                  q100(static_cast<u_timestamp>(mt)));
            ::fast_io::fast_terminate();
        }
    }
#endif

    // Case 4: rights missing → enotcapable
    {
        auto& fd5 = *env.fd_storage.opens.index_unchecked(5uz).fd_p;
        fd5.rights_base = static_cast<rights_t>(0);
        fd5.rights_inherit = static_cast<rights_t>(0);
        fd5.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fd5.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_filestat_set_times_rights.tmp",
                                     ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_set_times(env,
                                                                                      static_cast<wasi_posix_fd_t>(5),
                                                                                      static_cast<timestamp_t>(0u),
                                                                                      static_cast<timestamp_t>(0u),
                                                                                      static_cast<fstflags_t>(fstflags_t::filestat_set_mtim_now));
        if(ret != errno_t::enotcapable)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_set_times: expected enotcapable when rights missing");
            ::fast_io::fast_terminate();
        }
    }

    // Case 5: ebadf after fd_close
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(2uz).fd_p;
        fde.rights_base = static_cast<rights_t>(-1);
        fde.rights_inherit = static_cast<rights_t>(-1);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fde.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_filestat_set_times_close.tmp",
                                     ::fast_io::open_mode::out | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};

        auto const closed = ::uwvm2::imported::wasi::wasip1::func::fd_close(env, static_cast<wasi_posix_fd_t>(2));
        if(closed != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_set_times: close expected esuccess");
            ::fast_io::fast_terminate();
        }

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_filestat_set_times(env,
                                                                                      static_cast<wasi_posix_fd_t>(2),
                                                                                      static_cast<timestamp_t>(0u),
                                                                                      static_cast<timestamp_t>(0u),
                                                                                      static_cast<fstflags_t>(0));
        if(ret != errno_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_filestat_set_times: expected ebadf after fd_close");
            ::fast_io::fast_terminate();
        }
    }
}

