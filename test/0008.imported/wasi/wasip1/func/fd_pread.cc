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
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_pread(env,
                                                                         static_cast<wasi_posix_fd_t>(-1),
                                                                         static_cast<wasi_void_ptr_t>(0u),
                                                                         static_cast<wasi_size_t>(0u),
                                                                         static_cast<filesize_t>(0u),
                                                                         static_cast<wasi_void_ptr_t>(0u));
        if(ret != errno_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread: expected ebadf for negative fd");
            ::fast_io::fast_terminate();
        }
    }

    // Prepare a file with known content for reading
    constexpr char const content[] = "HelloWorld";  // 10 bytes
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
            = ::fast_io::native_file{u8"test_fd_pread_regular.tmp",
                                     ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
        // write test content into the native file handle
#if defined(_WIN32) && !defined(__CYGWIN__)
        ::fast_io::io::print(fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file, content);
#else
        ::fast_io::io::print(fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd, content);
#endif

        // Layout in WASM32 memory
        // Buffers
        constexpr wasi_void_ptr_t buf1{512u};
        constexpr wasi_void_ptr_t buf2{1024u};
        // IOV array at 128
        constexpr wasi_void_ptr_t iovs_ptr{128u};
        // nread output at 2000
        constexpr wasi_void_ptr_t nread_ptr{2000u};

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

        // Read at offset 0
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_pread(env,
                                                                         static_cast<wasi_posix_fd_t>(4),
                                                                         iovs_ptr,
                                                                         static_cast<wasi_size_t>(2u),
                                                                         static_cast<filesize_t>(0u),
                                                                         nread_ptr);
        if(ret != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread: expected esuccess for regular file");
            ::fast_io::fast_terminate();
        }

        auto const nread = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, nread_ptr);
        if(nread != static_cast<wasi_size_t>(10u))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread: nread should be 10");
            ::fast_io::fast_terminate();
        }

        // Validate contents
        if(std::memcmp(memory.memory_begin + buf1, "Hello", 5uz) != 0 || std::memcmp(memory.memory_begin + buf2, "World", 5uz) != 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread: buffers mismatch");
            ::fast_io::fast_terminate();
        }

        // Case: zero iovecs → success, nread=0
        constexpr wasi_void_ptr_t nread0_ptr{2100u};
        auto const ret0 = ::uwvm2::imported::wasi::wasip1::func::fd_pread(env,
                                                                          static_cast<wasi_posix_fd_t>(4),
                                                                          iovs_ptr,
                                                                          static_cast<wasi_size_t>(0u),
                                                                          static_cast<filesize_t>(0u),
                                                                          nread0_ptr);
        if(ret0 != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread: zero-iov should return esuccess");
            ::fast_io::fast_terminate();
        }
        auto const nread0 = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, nread0_ptr);
        if(nread0 != static_cast<wasi_size_t>(0u))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread: zero-iov nread should be 0");
            ::fast_io::fast_terminate();
        }

        // Case: offset beyond EOF → success, nread=0
        constexpr wasi_void_ptr_t nread_eof_ptr{2120u};
        auto const reto = ::uwvm2::imported::wasi::wasip1::func::fd_pread(env,
                                                                          static_cast<wasi_posix_fd_t>(4),
                                                                          iovs_ptr,
                                                                          static_cast<wasi_size_t>(1u),
                                                                          static_cast<filesize_t>(9999u),
                                                                          nread_eof_ptr);
        if(reto != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread: EOF read should return esuccess");
            ::fast_io::fast_terminate();
        }
        auto const nreado = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, nread_eof_ptr);
        if(nreado != static_cast<wasi_size_t>(0u))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread: EOF nread should be 0");
            ::fast_io::fast_terminate();
        }
    }

    // Case: enotcapable when no read rights
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(5uz).fd_p;
        // no rights granted by default
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fde.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
#if defined(_WIN32) && !defined(__CYGWIN__)
            .file
#endif
            = ::fast_io::native_file{u8"test_fd_pread_rights.tmp",
                                     ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
        // write initial data
#if defined(_WIN32) && !defined(__CYGWIN__)
        ::fast_io::io::print(fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file, "data");
#else
        ::fast_io::io::print(fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd, "data");
#endif

        constexpr wasi_void_ptr_t iovs_ptr{128u};
        constexpr wasi_void_ptr_t buf{600u};
        constexpr wasi_void_ptr_t nread_ptr{2200u};

        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32(memory, iovs_ptr, buf);
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32(memory,
                                                                                        static_cast<wasi_void_ptr_t>(iovs_ptr + 4u),
                                                                                        static_cast<wasi_size_t>(4u));

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_pread(env,
                                                                         static_cast<wasi_posix_fd_t>(5),
                                                                         iovs_ptr,
                                                                         static_cast<wasi_size_t>(1u),
                                                                         static_cast<filesize_t>(0u),
                                                                         nread_ptr);
        if(ret != errno_t::enotcapable)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread: expected enotcapable when rights missing");
            ::fast_io::fast_terminate();
        }
    }

#if !defined(_WIN32)
    // POSIX-only: directory fd → eisdir
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(6uz).fd_p;
        fde.rights_base = static_cast<rights_t>(-1);
        fde.rights_inherit = static_cast<rights_t>(-1);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
        fde.wasi_fd.ptr->wasi_fd_storage.storage
            .file_fd
# if defined(_WIN32) && !defined(__CYGWIN__)
            .file
# endif
            = ::fast_io::native_file{u8".", ::fast_io::open_mode::in | ::fast_io::open_mode::directory};

        constexpr wasi_void_ptr_t iovs_ptr{256u};
        constexpr wasi_void_ptr_t buf{700u};
        constexpr wasi_void_ptr_t nread_ptr{2300u};
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32(memory, iovs_ptr, buf);
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm32(memory,
                                                                                        static_cast<wasi_void_ptr_t>(iovs_ptr + 4u),
                                                                                        static_cast<wasi_size_t>(8u));

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_pread(env,
                                                                         static_cast<wasi_posix_fd_t>(6),
                                                                         iovs_ptr,
                                                                         static_cast<wasi_size_t>(1u),
                                                                         static_cast<filesize_t>(0u),
                                                                         nread_ptr);
        if(ret != errno_t::eisdir)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_pread: expected eisdir for directory fd on POSIX");
            ::fast_io::fast_terminate();
        }
    }
#endif

    return 0;
}

