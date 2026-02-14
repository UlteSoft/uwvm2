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

#include <uwvm2/imported/wasi/wasip1/func/fd_prestat_get_wasm64.h>
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
        auto const ret =
            ::uwvm2::imported::wasi::wasip1::func::fd_prestat_get_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(-1), static_cast<wasi_void_ptr_wasm64_t>(0u));
        if(ret != errno_wasm64_t::ebadf) { ::fast_io::fast_terminate(); }
    }

    // Set fd=3 as preopened directory with name "/sandbox" (new fd model)
    auto& fde = *env.fd_storage.opens.index_unchecked(3uz).fd_p;
    fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);
    {
        auto& ds = fde.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack;
        ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
        entry.ptr->dir_stack.name = ::uwvm2::utils::container::u8string{u8"/sandbox"};
        ds.dir_stack.push_back(entry);
    }

    // Case 1: success write of prestat
    {
        constexpr wasi_void_ptr_wasm64_t buf{4096u};
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_prestat_get_wasm64(env, static_cast<wasi_posix_fd_wasm64_t>(3), buf);
        if(ret != errno_wasm64_t::esuccess) { ::fast_io::fast_terminate(); }

        // Verify pr_type and pr_name_len
        auto const pr_type = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<
            std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::preopentype_wasm64_t>>(memory, buf);
        auto const pr_name_len =
            ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t>(
                memory,
                static_cast<wasi_void_ptr_wasm64_t>(buf + 8u));

        if(pr_type != static_cast<std::underlying_type_t<::uwvm2::imported::wasi::wasip1::abi::preopentype_wasm64_t>>(
                          ::uwvm2::imported::wasi::wasip1::abi::preopentype_wasm64_t::preopentype_dir))
        {
            ::fast_io::fast_terminate();
        }

        if(pr_name_len != static_cast<wasi_size_wasm64_t>(8u)) { ::fast_io::fast_terminate(); }
    }

    // Case 2: not preopened dir → enotdir (dir but dir_stack size != 1)
    {
        auto& fdx = *env.fd_storage.opens.index_unchecked(4uz).fd_p;
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
        auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_prestat_get_wasm64(env,
                                                                                      static_cast<wasi_posix_fd_wasm64_t>(4),
                                                                                      static_cast<wasi_void_ptr_wasm64_t>(8192u));
        if(ret != errno_wasm64_t::enotdir) { ::fast_io::fast_terminate(); }
    }

    return 0;
}

#else

int main() {}

#endif
