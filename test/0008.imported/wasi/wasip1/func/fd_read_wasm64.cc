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

// keep tests minimal; avoid platform-specific headers

// no third-party test headers

#include <uwvm2/imported/wasi/wasip1/func/fd_read_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_pwrite_wasm64.h>
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

    // Prepare FD table
    env.fd_storage.opens.resize(8uz);

    // Case 0: negative fd → ebadf (fast path)
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_read_wasm64(env,
                                                                               static_cast<wasi_posix_fd_wasm64_t>(-1),
                                                                               static_cast<wasi_void_ptr_wasm64_t>(0u),
                                                                               static_cast<wasi_size_wasm64_t>(0u),
                                                                               static_cast<wasi_void_ptr_wasm64_t>(0u));
        if(ret != errno_wasm64_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_read_wasm64: expected ebadf for negative fd");
            ::fast_io::fast_terminate();
        }
    }

    // Case 1: regular file read two buffers from beginning; prepare content by pwrite_wasm64
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(4uz).fd_p;
        fde.rights_base = static_cast<rights_wasm64_t>(-1);
        fde.rights_inherit = static_cast<rights_wasm64_t>(-1);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
#if defined(_WIN32) && !defined(__CYGWIN__)
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file =
            ::fast_io::native_file{u8"test_fd_read_wasm64_regular.tmp",
                                   ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
#else
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd =
            ::fast_io::native_file{u8"test_fd_read_wasm64_regular.tmp",
                                   ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat};
#endif

        // Write content using fd_pwrite_wasm64 at offset 0
        constexpr char const hello[] = "Hello";
        constexpr char const world[] = "World";
        constexpr wasi_void_ptr_wasm64_t wbuf1{100u};
        constexpr wasi_void_ptr_wasm64_t wbuf2{200u};
        ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm64(memory,
                                                                            wbuf1,
                                                                            reinterpret_cast<::std::byte const*>(hello),
                                                                            reinterpret_cast<::std::byte const*>(hello) + 5);
        ::uwvm2::imported::wasi::wasip1::memory::write_all_to_memory_wasm64(memory,
                                                                            wbuf2,
                                                                            reinterpret_cast<::std::byte const*>(world),
                                                                            reinterpret_cast<::std::byte const*>(world) + 5);

        constexpr wasi_void_ptr_wasm64_t wiovs_ptr{300u};
        constexpr wasi_void_ptr_wasm64_t nwritten_ptr{320u};
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, wiovs_ptr, wbuf1);
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                        static_cast<wasi_void_ptr_wasm64_t>(wiovs_ptr + 8u),
                                                                                        static_cast<wasi_size_wasm64_t>(5u));
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, static_cast<wasi_void_ptr_wasm64_t>(wiovs_ptr + 16u), wbuf2);
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                        static_cast<wasi_void_ptr_wasm64_t>(wiovs_ptr + 24u),
                                                                                        static_cast<wasi_size_wasm64_t>(5u));

        auto const wret = ::uwvm2::imported::wasi::wasip1::func::fd_pwrite_wasm64(env,
                                                                                  static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                  wiovs_ptr,
                                                                                  static_cast<wasi_size_wasm64_t>(2u),
                                                                                  static_cast<filesize_wasm64_t>(0u),
                                                                                  nwritten_ptr);
        if(wret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_read_wasm64 test prepare: fd_pwrite_wasm64 failed");
            ::fast_io::fast_terminate();
        }

        // Read back with fd_read_wasm64
        constexpr wasi_void_ptr_wasm64_t rbuf1{512u};
        constexpr wasi_void_ptr_wasm64_t rbuf2{1024u};
        constexpr wasi_void_ptr_wasm64_t iovs_ptr{128u};
        constexpr wasi_void_ptr_wasm64_t nread_ptr{2000u};

        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, iovs_ptr, static_cast<wasi_void_ptr_wasm64_t>(rbuf1));
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                        static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 8u),
                                                                                        static_cast<wasi_size_wasm64_t>(5u));
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                        static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 16u),
                                                                                        static_cast<wasi_void_ptr_wasm64_t>(rbuf2));
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                        static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 24u),
                                                                                        static_cast<wasi_size_wasm64_t>(5u));

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_read_wasm64(env,
                                                                               static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                               iovs_ptr,
                                                                               static_cast<wasi_size_wasm64_t>(2u),
                                                                               nread_ptr);
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_read_wasm64: expected esuccess for regular file");
            ::fast_io::fast_terminate();
        }

        auto const nread = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_size_wasm64_t>(memory, nread_ptr);
        if(nread != static_cast<wasi_size_wasm64_t>(10u))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_read_wasm64: nread should be 10");
            ::fast_io::fast_terminate();
        }

        if(std::memcmp(memory.memory_begin + rbuf1, "Hello", 5uz) != 0 || std::memcmp(memory.memory_begin + rbuf2, "World", 5uz) != 0)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_read_wasm64: buffers mismatch");
            ::fast_io::fast_terminate();
        }
    }

    // Case 2: zero iovecs → success, nread=0
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(5uz).fd_p;
        fde.rights_base = static_cast<rights_wasm64_t>(-1);
        fde.rights_inherit = static_cast<rights_wasm64_t>(-1);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
#if defined(_WIN32) && !defined(__CYGWIN__)
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file = ::fast_io::native_file{
            u8"test_fd_read_wasm64_zero.tmp",
            ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat };
#else
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd = ::fast_io::native_file{
            u8"test_fd_read_wasm64_zero.tmp",
            ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat };
#endif

        constexpr wasi_void_ptr_wasm64_t iovs_ptr{128u};
        constexpr wasi_void_ptr_wasm64_t nread0_ptr{4100u};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_read_wasm64(env,
                                                                               static_cast<wasi_posix_fd_wasm64_t>(5),
                                                                               iovs_ptr,
                                                                               static_cast<wasi_size_wasm64_t>(0u),
                                                                               nread0_ptr);
        if(ret != errno_wasm64_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_read_wasm64: zero-iov should return esuccess");
            ::fast_io::fast_terminate();
        }
        auto const n0 = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_size_wasm64_t>(memory, nread0_ptr);
        if(n0 != static_cast<wasi_size_wasm64_t>(0u))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_read_wasm64: zero-iov nread should be 0");
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: enotcapable when no read rights
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(6uz).fd_p;
        // rights default 0
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
#if defined(_WIN32) && !defined(__CYGWIN__)
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file = ::fast_io::native_file{
            u8"test_fd_read_wasm64_rights.tmp",
            ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat };
        auto& file_fd = fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file;
#else
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd = ::fast_io::native_file{
            u8"test_fd_read_wasm64_rights.tmp",
            ::fast_io::open_mode::out | ::fast_io::open_mode::in | ::fast_io::open_mode::trunc | ::fast_io::open_mode::creat };
        auto& file_fd = fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd;
#endif
        ::fast_io::io::print(file_fd, "data");

        constexpr wasi_void_ptr_wasm64_t iovs_ptr{5200u};
        constexpr wasi_void_ptr_wasm64_t buf{5000u};
        constexpr wasi_void_ptr_wasm64_t nread_ptr{5300u};
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, iovs_ptr, buf);
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                        static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 8u),
                                                                                        static_cast<wasi_size_wasm64_t>(4u));

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_read_wasm64(env,
                                                                               static_cast<wasi_posix_fd_wasm64_t>(6),
                                                                               iovs_ptr,
                                                                               static_cast<wasi_size_wasm64_t>(1u),
                                                                               nread_ptr);
        if(ret != errno_wasm64_t::enotcapable)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_read_wasm64: expected enotcapable when rights missing");
            ::fast_io::fast_terminate();
        }
    }

#if !defined(_WIN32)
    // POSIX-only: directory fd → eisdir
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(7uz).fd_p;
        fde.rights_base = static_cast<rights_wasm64_t>(-1);
        fde.rights_inherit = static_cast<rights_wasm64_t>(-1);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);
# if defined(_WIN32) && !defined(__CYGWIN__)
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd.file = ::fast_io::native_file{u8".", ::fast_io::open_mode::in | ::fast_io::open_mode::directory};
# else
        fde.wasi_fd.ptr->wasi_fd_storage.storage.file_fd = ::fast_io::native_file{u8".", ::fast_io::open_mode::in | ::fast_io::open_mode::directory};
# endif

        constexpr wasi_void_ptr_wasm64_t iovs_ptr{6200u};
        constexpr wasi_void_ptr_wasm64_t buf{6000u};
        constexpr wasi_void_ptr_wasm64_t nread_ptr{6300u};
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory, iovs_ptr, buf);
        ::uwvm2::imported::wasi::wasip1::memory::store_basic_wasm_type_to_memory_wasm64(memory,
                                                                                        static_cast<wasi_void_ptr_wasm64_t>(iovs_ptr + 8u),
                                                                                        static_cast<wasi_size_wasm64_t>(1u));

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_read_wasm64(env,
                                                                               static_cast<wasi_posix_fd_wasm64_t>(7),
                                                                               iovs_ptr,
                                                                               static_cast<wasi_size_wasm64_t>(1u),
                                                                               nread_ptr);
        if(ret != errno_wasm64_t::eisdir)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"fd_read_wasm64: expected eisdir for directory fd on POSIX");
            ::fast_io::fast_terminate();
        }
    }
#endif

    return 0;
}

#else

int main() {}

#endif
