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

#include <uwvm2/imported/wasi/wasip1/func/fd_prestat_dir_name.h>
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
    using ::uwvm2::imported::wasi::wasip1::abi::errno_t;
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

    // Case 0: negative fd → ebadf
    {
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_prestat_dir_name(env,
                                                                                    static_cast<wasi_posix_fd_t>(-1),
                                                                                    static_cast<wasi_void_ptr_t>(0u),
                                                                                    static_cast<wasi_size_t>(0u));
        if(ret != errno_t::ebadf) { ::fast_io::fast_terminate(); }
    }

    // Set fd=3 as preopened directory with name "/sandbox"
    auto& fde = *env.fd_storage.opens.index_unchecked(3uz).fd_p;
    fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);
    {
        auto& ds = fde.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack;
        ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
        entry.ptr->dir_stack.name = ::uwvm2::utils::container::u8string{u8"/sandbox"};
        ds.dir_stack.push_back(entry);
    }

    // Case 1: buffer too small → enametoolong
    {
        constexpr wasi_void_ptr_t buf{512u};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_prestat_dir_name(env, static_cast<wasi_posix_fd_t>(3), buf, static_cast<wasi_size_t>(3u));
        if(ret != errno_t::enametoolong) { ::fast_io::fast_terminate(); }
    }

    // Case 2: success and exact bytes copied, no NUL
    {
        constexpr wasi_void_ptr_t buf{600u};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_prestat_dir_name(env, static_cast<wasi_posix_fd_t>(3), buf, static_cast<wasi_size_t>(8u));
        if(ret != errno_t::esuccess) { ::fast_io::fast_terminate(); }

        if(std::memcmp(memory.memory_begin + buf, "/sandbox", 8uz) != 0) { ::fast_io::fast_terminate(); }
    }

    // Case 3: not preopened dir → enotdir
    {
        auto& fdx = *env.fd_storage.opens.index_unchecked(4uz).fd_p;
        // Construct a directory fd that is not a preopened root (dir_stack size != 1)
        fdx.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);
        auto& ds2 = fdx.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack;
        {
            ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t e1{};
            e1.ptr->dir_stack.name = ::uwvm2::utils::container::u8string{u8"/a"};
            ds2.dir_stack.push_back(e1);
        }
        {
            ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t e2{};
            e2.ptr->dir_stack.name = ::uwvm2::utils::container::u8string{u8"b"};
            ds2.dir_stack.push_back(e2);
        }
        constexpr wasi_void_ptr_t buf{700u};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_prestat_dir_name(env, static_cast<wasi_posix_fd_t>(4), buf, static_cast<wasi_size_t>(1u));
        if(ret != errno_t::enotdir) { ::fast_io::fast_terminate(); }
    }

    return 0;
}

