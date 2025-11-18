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
#include <cstring>
#include <type_traits>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/fd_seek.h>

int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::errno_t;
    using ::uwvm2::imported::wasi::wasip1::abi::filedelta_t;
    using ::uwvm2::imported::wasi::wasip1::abi::filesize_t;
    using ::uwvm2::imported::wasi::wasip1::abi::rights_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t;
    using ::uwvm2::imported::wasi::wasip1::abi::whence_t;
    using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
    using ::uwvm2::object::memory::linear::native_memory_t;

    native_memory_t memory{};
    memory.init_by_page_count(2uz);

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = {},
                                            .envs = {},
                                            .fd_storage = {},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false};

    env.fd_storage.opens.resize(10uz);

    // Case 0: negative fd -> ebadf
    {
        constexpr wasi_void_ptr_t new_off_ptr{0u};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_seek(env,
                                                                        static_cast<wasi_posix_fd_t>(-1),
                                                                        static_cast<filedelta_t>(0),
                                                                        whence_t::whence_set,
                                                                        new_off_ptr);
        if(ret != errno_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_seek: expected ebadf for negative fd");
            ::fast_io::fast_terminate();
        }
    }

    // Prepare regular file at fd=4
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(4uz).fd_p;
        fde.rights_base = static_cast<rights_t>(-1);
        fde.rights_inherit = static_cast<rights_t>(-1);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
#if defined(_WIN32) && !defined(__CYGWIN__)
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file =
            ::fast_io::native_file{u8"test_fd_seek_regular.tmp",
                                   ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
        auto& file_fd = fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file;
#else
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd =
            ::fast_io::native_file{u8"test_fd_seek_regular.tmp",
                                   ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
        auto& file_fd = fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd;
#endif
        ::fast_io::io::print(file_fd, "HelloWorld");  // 10 bytes

        // 1) set to 0 -> new offset 0
        constexpr wasi_void_ptr_t new_off0_ptr{1024u};
        {
            auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_seek(env,
                                                                            static_cast<wasi_posix_fd_t>(4),
                                                                            static_cast<filedelta_t>(0),
                                                                            whence_t::whence_set,
                                                                            new_off0_ptr);
            if(ret != errno_t::esuccess) { ::fast_io::fast_terminate(); }
            using u_filesize_t = ::std::underlying_type_t<filesize_t>;
            auto const new0 = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<u_filesize_t>(memory, new_off0_ptr);
            if(static_cast<unsigned long long>(new0) != 0ull) { ::fast_io::fast_terminate(); }
        }

        // 2) end + 0 -> new offset 10
        constexpr wasi_void_ptr_t new_off1_ptr{2048u};
        {
            auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_seek(env,
                                                                            static_cast<wasi_posix_fd_t>(4),
                                                                            static_cast<filedelta_t>(0),
                                                                            whence_t::whence_end,
                                                                            new_off1_ptr);
            if(ret != errno_t::esuccess) { ::fast_io::fast_terminate(); }
            using u_filesize_t = ::std::underlying_type_t<filesize_t>;
            auto const new1 = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<u_filesize_t>(memory, new_off1_ptr);
            if(static_cast<unsigned long long>(new1) != 10ull) { ::fast_io::fast_terminate(); }
        }

        // 3) set 5 -> new 5
        constexpr wasi_void_ptr_t new_off2_ptr{3072u};
        {
            auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_seek(env,
                                                                            static_cast<wasi_posix_fd_t>(4),
                                                                            static_cast<filedelta_t>(5),
                                                                            whence_t::whence_set,
                                                                            new_off2_ptr);
            if(ret != errno_t::esuccess) { ::fast_io::fast_terminate(); }
            using u_filesize_t = ::std::underlying_type_t<filesize_t>;
            auto const new2 = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<u_filesize_t>(memory, new_off2_ptr);
            if(static_cast<unsigned long long>(new2) != 5ull) { ::fast_io::fast_terminate(); }
        }

        // 4) cur + 2 -> new 7
        constexpr wasi_void_ptr_t new_off3_ptr{4096u};
        {
            auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_seek(env,
                                                                            static_cast<wasi_posix_fd_t>(4),
                                                                            static_cast<filedelta_t>(2),
                                                                            whence_t::whence_cur,
                                                                            new_off3_ptr);
            if(ret != errno_t::esuccess) { ::fast_io::fast_terminate(); }
            using u_filesize_t = ::std::underlying_type_t<filesize_t>;
            auto const new3 = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<u_filesize_t>(memory, new_off3_ptr);
            if(static_cast<unsigned long long>(new3) != 7ull) { ::fast_io::fast_terminate(); }
        }
    }

    // Rights check: no seek rights -> enotcapable
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(5uz).fd_p;
        // rights default 0
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
#if defined(_WIN32) && !defined(__CYGWIN__)
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file =
            ::fast_io::native_file{u8"test_fd_seek_rights.tmp",
                                   ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
#else
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd =
            ::fast_io::native_file{u8"test_fd_seek_rights.tmp",
                                   ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
#endif

        constexpr wasi_void_ptr_t new_off_ptr{5120u};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_seek(env,
                                                                        static_cast<wasi_posix_fd_t>(5),
                                                                        static_cast<filedelta_t>(0),
                                                                        whence_t::whence_set,
                                                                        new_off_ptr);
        if(ret != errno_t::enotcapable)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_seek: expected enotcapable when no rights");
            ::fast_io::fast_terminate();
        }
    }

    // Directory fd -> espipe (current implementation)
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(6uz).fd_p;
        fde.rights_base = static_cast<rights_t>(-1);
        fde.rights_inherit = static_cast<rights_t>(-1);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);

        constexpr wasi_void_ptr_t new_off_ptr{6144u};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_seek(env,
                                                                        static_cast<wasi_posix_fd_t>(6),
                                                                        static_cast<filedelta_t>(0),
                                                                        whence_t::whence_set,
                                                                        new_off_ptr);
        if(ret != errno_t::espipe)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_seek: expected eisdir for directory fd");
            ::fast_io::fast_terminate();
        }
    }

    return 0;
}

