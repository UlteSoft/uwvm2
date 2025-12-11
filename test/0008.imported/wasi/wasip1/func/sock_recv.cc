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

// keep tests minimal; avoid platform-specific headers

#if (!defined(__NEWLIB__) || defined(__CYGWIN__))

# include <uwvm2/imported/wasi/wasip1/func/sock_recv.h>
# include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>
# if defined(UWVM_IMPORT_WASI_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)

int main()
{
    using ::uwvm2::imported::wasi::wasip1::abi::errno_t;
    using ::uwvm2::imported::wasi::wasip1::abi::riflags_t;
    using ::uwvm2::imported::wasi::wasip1::abi::rights_t;
    using ::uwvm2::imported::wasi::wasip1::abi::roflags_t;
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

    // Prepare FD table for all test cases
    env.fd_storage.opens.resize(4uz);

    constexpr wasi_void_ptr_t IOVS_PTR{1024u};
    constexpr wasi_void_ptr_t NREAD_PTR{2048u};
    constexpr wasi_void_ptr_t ROFLAGS_PTR{4096u};

    // Case 0: negative fd -> ebadf
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_recv(env,
                                                                          static_cast<wasi_posix_fd_t>(-1),
                                                                          IOVS_PTR,
                                                                          static_cast<wasi_size_t>(0u),
                                                                          static_cast<riflags_t>(0),
                                                                          NREAD_PTR,
                                                                          ROFLAGS_PTR);
        if(ret != errno_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv: expected ebadf for negative fd");
            ::fast_io::fast_terminate();
        }
    }

    // Case 1: closed descriptor (close_pos != SIZE_MAX) -> ebadf
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(1uz).fd_p;
        fde.close_pos = 0u;

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_recv(env,
                                                                          static_cast<wasi_posix_fd_t>(1),
                                                                          IOVS_PTR,
                                                                          static_cast<wasi_size_t>(0u),
                                                                          static_cast<riflags_t>(0),
                                                                          NREAD_PTR,
                                                                          ROFLAGS_PTR);
        if(ret != errno_t::ebadf)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv: expected ebadf for closed descriptor");
            ::fast_io::fast_terminate();
        }
    }

    // Case 2: missing right_fd_read -> enotcapable
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(2uz).fd_p;
        fde.close_pos = static_cast<std::size_t>(-1);
        fde.rights_base = static_cast<rights_t>(0);
        fde.rights_inherit = static_cast<rights_t>(0);
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_recv(env,
                                                                          static_cast<wasi_posix_fd_t>(2),
                                                                          IOVS_PTR,
                                                                          static_cast<wasi_size_t>(0u),
                                                                          static_cast<riflags_t>(0),
                                                                          NREAD_PTR,
                                                                          ROFLAGS_PTR);
        if(ret != errno_t::enotcapable)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv: expected enotcapable when right_fd_read missing");
            ::fast_io::fast_terminate();
        }
    }

    // Case 3: zero iovs_len -> esuccess, nread=0, roflags=0
    {
        auto& fde = *env.fd_storage.opens.index_unchecked(0uz).fd_p;
        fde.close_pos = static_cast<std::size_t>(-1);
        fde.rights_base = ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_fd_read;
        fde.rights_inherit = ::uwvm2::imported::wasi::wasip1::abi::rights_t::right_fd_read;
        fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::file);

        auto const ret = ::uwvm2::imported::wasi::wasip1::func::sock_recv(env,
                                                                          static_cast<wasi_posix_fd_t>(0),
                                                                          IOVS_PTR,
                                                                          static_cast<wasi_size_t>(0u),
                                                                          static_cast<riflags_t>(0),
                                                                          NREAD_PTR,
                                                                          ROFLAGS_PTR);
        if(ret != errno_t::esuccess)
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv: zero-iov should return esuccess");
            ::fast_io::fast_terminate();
        }

        auto const n0 = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<wasi_size_t>(memory, NREAD_PTR);
        if(n0 != static_cast<wasi_size_t>(0u))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv: zero-iov nread should be 0");
            ::fast_io::fast_terminate();
        }

        using roflags_underlying_t = ::std::underlying_type_t<roflags_t>;
        auto const roflags_val = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm32<roflags_underlying_t>(memory, ROFLAGS_PTR);
        if(roflags_val != static_cast<roflags_underlying_t>(0u))
        {
            ::fast_io::io::perrln(::fast_io::u8err(), u8"sock_recv: zero-iov roflags should be 0");
            ::fast_io::fast_terminate();
        }
    }

    return 0;
}

# else

int main() {}

# endif

#else

int main() {}

#endif

