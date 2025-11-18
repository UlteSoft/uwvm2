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
#include <cstring>

#include <fast_io.h>

#if __has_include(<fcntl.h>)
# include <fcntl.h>
#endif
#if ((defined(_WIN32) && !defined(__WINE__) && !defined(__BIONIC__)) && !defined(__CYGWIN__))
# if __has_include(<io.h>)
#  include <io.h>
# endif
#endif

#if __has_include(<third-parties/bizwen/include/bizwen/deque.hpp>)
# include <third-parties/bizwen/include/bizwen/deque.hpp>
#endif

#include <uwvm2/imported/wasi/wasip1/func/fd_pwrite.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_pread.h>

int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::errno_t;
    using ::uwvm2::imported::wasi::wasip1::abi::filesize_t;
    using ::uwvm2::imported::wasi::wasip1::abi::rights_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_size_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_t;
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

    // Prepare FD table
    env.fd_storage.opens.resize(8uz);

    // Case 0: negative fd → ebadf (fast path, no memory access)
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_pwrite(env,
                                                                          static_cast<wasi_posix_fd_t>(-1),
                                                                          static_cast<wasi_void_ptr_t>(0u),
                                                                          static_cast<wasi_size_t>(0u),
                                                                          static_cast<filesize_t>(0u),
                                                                          static_cast<wasi_void_ptr_t>(0u));
        if(ret != errno_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pwrite: expected ebadf for negative fd");
            ::fast_io::fast_terminate();
        }
    }

    // Case 1: regular file pwrite two buffers at offset 0; verify content by pread
    {
        // Use fd=4
        auto& fde = *env.fd_storage.opens.index_unchecked(4uz).fd_p;
        fde.rights_base = static_cast<rights_t>(-1);
        fde.rights_inherit = static_cast<rights_t>(-1);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fde.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_pwrite_regular.tmp",
                                     ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};

        // Source buffers in WASM32 memory
        constexpr char const hello[] = "Hello";  // 5
        constexpr char const world[] = "World";  // 5
        constexpr wasi_void_ptr_t buf1{512u};
        constexpr wasi_void_ptr_t buf2{1024u};

        ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32(memory,
                                                                            buf1,
                                                                            reinterpret_cast<::std::byte const*>(hello),
                                                                            reinterpret_cast<::std::byte const*>(hello) + 5);
        ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32(memory,
                                                                            buf2,
                                                                            reinterpret_cast<::std::byte const*>(world),
                                                                            reinterpret_cast<::std::byte const*>(world) + 5);

        // ciovecs at 128
        constexpr wasi_void_ptr_t iovs_ptr{128u};
        constexpr wasi_void_ptr_t nwritten_ptr{2000u};

        // iov[0] = {buf1, 5}, iov[1] = {buf2, 5}
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32(memory, iovs_ptr, static_cast<wasi_void_ptr_t>(buf1));
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32(memory,
                                                                                        static_cast<wasi_void_ptr_t>(iovs_ptr + 4u),
                                                                                        static_cast<wasi_size_t>(5u));
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32(memory,
                                                                                        static_cast<wasi_void_ptr_t>(iovs_ptr + 8u),
                                                                                        static_cast<wasi_void_ptr_t>(buf2));
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32(memory,
                                                                                        static_cast<wasi_void_ptr_t>(iovs_ptr + 12u),
                                                                                        static_cast<wasi_size_t>(5u));

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_pwrite(env,
                                                                          static_cast<wasi_posix_fd_t>(4),
                                                                          iovs_ptr,
                                                                          static_cast<wasi_size_t>(2u),
                                                                          static_cast<filesize_t>(0u),
                                                                          nwritten_ptr);
        if(ret != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pwrite: expected esuccess for regular file");
            ::fast_io::fast_terminate();
        }

        auto const nwritten = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, nwritten_ptr);
        if(nwritten != static_cast<wasi_size_t>(10u))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pwrite: nwritten should be 10");
            ::fast_io::fast_terminate();
        }

        // Read back to verify content using fd_pread
        constexpr wasi_void_ptr_t read_iovs_ptr{3000u};
        constexpr wasi_void_ptr_t out1{3500u};
        constexpr wasi_void_ptr_t out2{3600u};
        constexpr wasi_void_ptr_t nread_ptr{3700u};

        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32(memory, read_iovs_ptr, out1);
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32(memory,
                                                                                        static_cast<wasi_void_ptr_t>(read_iovs_ptr + 4u),
                                                                                        static_cast<wasi_size_t>(5u));
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32(memory, static_cast<wasi_void_ptr_t>(read_iovs_ptr + 8u), out2);
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32(memory,
                                                                                        static_cast<wasi_void_ptr_t>(read_iovs_ptr + 12u),
                                                                                        static_cast<wasi_size_t>(5u));

        auto const rret = ::uwvm2::imported::wasi::wasip1::func::fd_pread(env,
                                                                          static_cast<wasi_posix_fd_t>(4),
                                                                          read_iovs_ptr,
                                                                          static_cast<wasi_size_t>(2u),
                                                                          static_cast<filesize_t>(0u),
                                                                          nread_ptr);
        if(rret != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread (verify): expected esuccess");
            ::fast_io::fast_terminate();
        }
        auto const nread = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, nread_ptr);
        if(nread != static_cast<wasi_size_t>(10u))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pwrite verify: nread should be 10");
            ::fast_io::fast_terminate();
        }
        if(std::memcmp(memory.memory_begin + out1, "Hello", 5uz) != 0 || std::memcmp(memory.memory_begin + out2, "World", 5uz) != 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pwrite verify: buffers mismatch");
            ::fast_io::fast_terminate();
        }
    }

    // Case 2: zero iovecs → success, nwritten=0
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(5uz).fd_p;
        fde.rights_base = static_cast<rights_t>(-1);
        fde.rights_inherit = static_cast<rights_t>(-1);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fde.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_pwrite_zero.tmp",
                                     ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};

        constexpr wasi_void_ptr_t iovs_ptr{128u};
        constexpr wasi_void_ptr_t nwritten0_ptr{4100u};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_pwrite(env,
                                                                          static_cast<wasi_posix_fd_t>(5),
                                                                          iovs_ptr,
                                                                          static_cast<wasi_size_t>(0u),
                                                                          static_cast<filesize_t>(0u),
                                                                          nwritten0_ptr);
        if(ret != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pwrite: zero-iov should return esuccess");
            ::fast_io::fast_terminate();
        }
        auto const nw0 = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, nwritten0_ptr);
        if(nw0 != static_cast<wasi_size_t>(0u))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pwrite: zero-iov nwritten should be 0");
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: enotcapable when no write/seek rights
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(6uz).fd_p;
        // rights default 0
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fde.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_pwrite_rights.tmp",
                                     ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};

        constexpr wasi_void_ptr_t buf{5000u};
        constexpr char const data[] = "X";
        ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32(memory,
                                                                            buf,
                                                                            reinterpret_cast<::std::byte const*>(data),
                                                                            reinterpret_cast<::std::byte const*>(data) + 1);

        constexpr wasi_void_ptr_t iovs_ptr{5200u};
        constexpr wasi_void_ptr_t nwritten_ptr{5300u};
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32(memory, iovs_ptr, buf);
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32(memory,
                                                                                        static_cast<wasi_void_ptr_t>(iovs_ptr + 4u),
                                                                                        static_cast<wasi_size_t>(1u));

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_pwrite(env,
                                                                          static_cast<wasi_posix_fd_t>(6),
                                                                          iovs_ptr,
                                                                          static_cast<wasi_size_t>(1u),
                                                                          static_cast<filesize_t>(0u),
                                                                          nwritten_ptr);
        if(ret != errno_t::enotcapable)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pwrite: expected enotcapable when rights missing");
            ::fast_io::fast_terminate();
        }
    }

#if !defined(_WIN32)
    // POSIX-only: directory fd → eisdir
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(7uz).fd_p;
        fde.rights_base = static_cast<rights_t>(-1);
        fde.rights_inherit = static_cast<rights_t>(-1);
        // For POSIX, keeping type as file with a directory handle is sufficient because
        // the implementation fstat's and returns eisdir for directories.
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fde.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
# if defined(_WIN32) && !defined(__CYGWIN__)
            .file
# endif
            = ::fast_io::native_file{u8".", ::fast_io::open_mode::in | ::fast_io::open_mode::directory};

        constexpr wasi_void_ptr_t buf{6000u};
        constexpr char const data[] = "Y";
        ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm32(memory,
                                                                            buf,
                                                                            reinterpret_cast<::std::byte const*>(data),
                                                                            reinterpret_cast<::std::byte const*>(data) + 1);

        constexpr wasi_void_ptr_t iovs_ptr{6200u};
        constexpr wasi_void_ptr_t nwritten_ptr{6300u};
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32(memory, iovs_ptr, buf);
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32(memory,
                                                                                        static_cast<wasi_void_ptr_t>(iovs_ptr + 4u),
                                                                                        static_cast<wasi_size_t>(1u));

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_pwrite(env,
                                                                          static_cast<wasi_posix_fd_t>(7),
                                                                          iovs_ptr,
                                                                          static_cast<wasi_size_t>(1u),
                                                                          static_cast<filesize_t>(0u),
                                                                          nwritten_ptr);
        if(ret != errno_t::eisdir)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pwrite: expected eisdir for directory fd on POSIX");
            ::fast_io::fast_terminate();
        }
    }
#endif

    return 0;
}

