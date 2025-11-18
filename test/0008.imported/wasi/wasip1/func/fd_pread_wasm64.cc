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

#include <uwvm2/imported/wasi/wasip1/func/fd_pread_wasm64.h>

int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;
    using ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t;
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

    env.fd_storage.opens.resize(8uz);

    // Case 0: negative fd → ebadf
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_pread_wasm64(env,
                                                                                static_cast<wasi_posix_fd_wasm64_t>(-1),
                                                                                static_cast<wasi_void_ptr_wasm64_t>(0u),
                                                                                static_cast<wasi_size_wasm64_t>(0u),
                                                                                static_cast<filesize_wasm64_t>(0u),
                                                                                static_cast<wasi_void_ptr_wasm64_t>(0u));
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread_wasm64: expected ebadf for negative fd");
            ::fast_io::fast_terminate();
        }
    }

    // Prepare a file with known content
    constexpr char const content[] = "HelloWorld";  // 10 bytes
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(4uz).fd_p;
        fde.rights_base = static_cast<rights_wasm64_t>(-1);
        fde.rights_inherit = static_cast<rights_wasm64_t>(-1);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fde.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_pread_wasm64_regular.tmp",
                                     ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
        // write test content into the native file handle
#if defined(_WIN32) && !defined(__CYGWIN__)
        ::fast_io::io::print(fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file, content);
#else
        ::fast_io::io::print(fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd, content);
#endif

        // WASM64 memory layout
        constexpr wasi_void_ptr_wasm64_t buf1{4096u};
        constexpr wasi_void_ptr_wasm64_t buf2{8192u};
        constexpr wasi_void_ptr_wasm64_t iovs_ptr{2048u};
        constexpr wasi_void_ptr_wasm64_t nread_ptr{12288u};

        // iov[0] = {buf1, 5}, iov[1] = {buf2, 5}
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, iovs_ptr, buf1);
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                        static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 8u),
                                                                                        static_cast<wasi_size_wasm64_t>(5u));
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 16u), buf2);
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                        static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 24u),
                                                                                        static_cast<wasi_size_wasm64_t>(5u));

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_pread_wasm64(env,
                                                                                static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                iovs_ptr,
                                                                                static_cast<wasi_size_wasm64_t>(2u),
                                                                                static_cast<filesize_wasm64_t>(0u),
                                                                                nread_ptr);
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread_wasm64: expected esuccess for regular file");
            ::fast_io::fast_terminate();
        }

        auto const nread = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_size_wasm64_t>(memory, nread_ptr);
        if(nread != static_cast<wasi_size_wasm64_t>(10u))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread_wasm64: nread should be 10");
            ::fast_io::fast_terminate();
        }

        if(std::memcmp(memory.memory_begin + static_cast<std::size_t>(buf1), "Hello", 5uz) != 0 ||
           std::memcmp(memory.memory_begin + static_cast<std::size_t>(buf2), "World", 5uz) != 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread_wasm64: buffers mismatch");
            ::fast_io::fast_terminate();
        }

        // Zero iovec
        constexpr wasi_void_ptr_wasm64_t nread0_ptr{13000u};
        auto const ret0 = ::uwvm2::imported::wasi::wasip1::func::fd_pread_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                 iovs_ptr,
                                                                                 static_cast<wasi_size_wasm64_t>(0u),
                                                                                 static_cast<filesize_wasm64_t>(0u),
                                                                                 nread0_ptr);
        if(ret0 != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread_wasm64: zero-iov should return esuccess");
            ::fast_io::fast_terminate();
        }
        auto const nread0 = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_size_wasm64_t>(memory, nread0_ptr);
        if(nread0 != static_cast<wasi_size_wasm64_t>(0u))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread_wasm64: zero-iov nread should be 0");
            ::fast_io::fast_terminate();
        }

        // EOF offset
        constexpr wasi_void_ptr_wasm64_t nread_eof_ptr{13200u};
        auto const reto = ::uwvm2::imported::wasi::wasip1::func::fd_pread_wasm64(env,
                                                                                 static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                 iovs_ptr,
                                                                                 static_cast<wasi_size_wasm64_t>(1u),
                                                                                 static_cast<filesize_wasm64_t>(9999u),
                                                                                 nread_eof_ptr);
        if(reto != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread_wasm64: EOF read should return esuccess");
            ::fast_io::fast_terminate();
        }
        auto const nreado = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_size_wasm64_t>(memory, nread_eof_ptr);
        if(nreado != static_cast<wasi_size_wasm64_t>(0u))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread_wasm64: EOF nread should be 0");
            ::fast_io::fast_terminate();
        }
    }

    // Rights check enotcapable
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(5uz).fd_p;
        // no rights granted by default
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fde.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_pread_wasm64_rights.tmp",
                                     ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
        // write initial data
#if defined(_WIN32) && !defined(__CYGWIN__)
        ::fast_io::io::print(fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file, "data");
#else
        ::fast_io::io::print(fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd, "data");
#endif

        constexpr wasi_void_ptr_wasm64_t iovs_ptr{4096u};
        constexpr wasi_void_ptr_wasm64_t buf{16384u};
        constexpr wasi_void_ptr_wasm64_t nread_ptr{20000u};

        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, iovs_ptr, buf);
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                        static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 8u),
                                                                                        static_cast<wasi_size_wasm64_t>(4u));

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_pread_wasm64(env,
                                                                                static_cast<wasi_posix_fd_wasm64_t>(5),
                                                                                iovs_ptr,
                                                                                static_cast<wasi_size_wasm64_t>(1u),
                                                                                static_cast<filesize_wasm64_t>(0u),
                                                                                nread_ptr);
        if(ret != errno_wasm64_t::enotcapable)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread_wasm64: expected enotcapable when rights missing");
            ::fast_io::fast_terminate();
        }
    }

#if !defined(_WIN32)
    // POSIX: directory → eisdir
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(6uz).fd_p;
        fde.rights_base = static_cast<rights_wasm64_t>(-1);
        fde.rights_inherit = static_cast<rights_wasm64_t>(-1);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fde.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
# if defined(_WIN32) && !defined(__CYGWIN__)
            .file
# endif
            = ::fast_io::native_file{u8".", ::fast_io::open_mode::in | ::fast_io::open_mode::directory};

        constexpr wasi_void_ptr_wasm64_t iovs_ptr{6144u};
        constexpr wasi_void_ptr_wasm64_t buf{24576u};
        constexpr wasi_void_ptr_wasm64_t nread_ptr{28672u};
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, iovs_ptr, buf);
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                        static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 8u),
                                                                                        static_cast<wasi_size_wasm64_t>(8u));

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_pread_wasm64(env,
                                                                                static_cast<wasi_posix_fd_wasm64_t>(6),
                                                                                iovs_ptr,
                                                                                static_cast<wasi_size_wasm64_t>(1u),
                                                                                static_cast<filesize_wasm64_t>(0u),
                                                                                nread_ptr);
        if(ret != errno_wasm64_t::eisdir)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread_wasm64: expected eisdir for directory fd on POSIX");
            ::fast_io::fast_terminate();
        }
    }
#endif

    return 0;
}

