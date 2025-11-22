/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      Claude Sonnect 4.5
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

// S12: Multi-threaded test for fd_prestat_get and fd_prestat_dir_name

// std
#include <cstddef>
#include <cstring>
#include <version>
#include <atomic>
#include <vector>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/fd_prestat_get_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_prestat_dir_name_wasm64.h>

using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::preopentype_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::rights_t;
using ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
using ::uwvm2::object::memory::linear::native_memory_t;

inline static void read_bytes64(native_memory_t& memory, wasi_void_ptr_wasm64_t p, void* dest, ::std::size_t n)
{
    ::uwvm2::imported::wasi::wasip1::memory::read_all_from_memory_wasm64(memory,
                                                                         p,
                                                                         reinterpret_cast<::std::byte*>(dest),
                                                                         reinterpret_cast<::std::byte*>(dest) + n);
}

// Test fd_prestat_get
inline static void
    test_fd_prestat_get(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t preopen_fd, ::std::size_t base_off)
{
    wasi_void_ptr_wasm64_t const prestat_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x100u)};

    auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_prestat_get_wasm64(env, preopen_fd, prestat_ptr);
    if(ret != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_prestat_get: fd_prestat_get failed\n");
        ::fast_io::fast_terminate();
    }

    // Check that type is preopentype_dir (value 0)
    auto const type_val = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<::std::uint8_t>(memory, prestat_ptr);
    if(type_val != 0u) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_prestat_get: prestat type is not preopentype_dir\n");
        ::fast_io::fast_terminate();
    }

    // pr_name_len is at offset 8 (after tag(1) + padding(7))
    auto const name_len = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_size_wasm64_t>(
        memory,
        static_cast<wasi_void_ptr_wasm64_t>(prestat_ptr + 8u));
    // Name length should be reasonable (1 for ".")
    if(name_len == 0u || name_len > 1024u) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_prestat_get: prestat name_len out of range\n");
        ::fast_io::fast_terminate();
    }
}

// Test fd_prestat_dir_name
inline static void
    test_fd_prestat_dir_name(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t preopen_fd, ::std::size_t base_off)
{
    wasi_void_ptr_wasm64_t const name_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x100u)};
    constexpr wasi_size_wasm64_t name_len{256u};

    auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_prestat_dir_name_wasm64(env, preopen_fd, name_ptr, name_len);
    if(ret != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_prestat_dir_name: fd_prestat_dir_name failed\n");
        ::fast_io::fast_terminate();
    }

    // Read the directory name (should be ".")
    char dir_name[256];
    read_bytes64(memory, name_ptr, dir_name, 1u);
    if(dir_name[0] != '.') [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_prestat_dir_name: directory name is not '.'\n");
        ::fast_io::fast_terminate();
    }
}

int main()
{
#if __cpp_lib_atomic_wait >= 201907L
    constexpr unsigned thread_count{4u};
    constexpr ::std::size_t base_stride{0x10000u};

    native_memory_t memory{};
    memory.init_by_page_count(thread_count + 4u);

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = {},
                                            .envs = {},
                                            .fd_storage = {.fd_limit = 128uz},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false,
                                            .disable_utf8_check = false};

    // Prepare a preopened directory fd at index 11
    env.fd_storage.opens.resize(12uz);
    auto& preopen_fde = *env.fd_storage.opens.index_unchecked(11uz).fd_p;

    // Set rights for preopened directory
    rights_t const dir_base_rights{static_cast<rights_t>(rights_wasm64_t::right_fd_readdir) | static_cast<rights_t>(rights_wasm64_t::right_path_open) |
                                   static_cast<rights_t>(rights_wasm64_t::right_fd_filestat_get)};
    rights_t const dir_inherit_rights{static_cast<rights_t>(rights_wasm64_t::right_fd_read) | static_cast<rights_t>(rights_wasm64_t::right_fd_write)};

    preopen_fde.rights_base = static_cast<rights_wasm64_t>(dir_base_rights);
    preopen_fde.rights_inherit = static_cast<rights_wasm64_t>(dir_inherit_rights);
    preopen_fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);

    // Set up directory stack with current directory
    auto& ds = preopen_fde.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack;
    ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
    entry.ptr->dir_stack.file = ::fast_io::dir_file{u8"."};
    entry.ptr->dir_stack.name = u8".";
    ds.dir_stack.push_back(::std::move(entry));

    std::atomic<bool> failure{false};

    ::std::vector<fast_io::native_thread> threads;
    threads.reserve(thread_count);

    for(unsigned tid = 0; tid < thread_count; ++tid)
    {
        threads.emplace_back(
            [&memory, &env, &failure, tid]()
            {
                constexpr unsigned iterations{64u};
                for(unsigned i = 0; i < iterations && !failure.load(::std::memory_order_relaxed); ++i)
                {
                    auto const base_off = static_cast<::std::size_t>((tid + 1u) * base_stride);
                    auto const scenario = (tid + i) % 2u;
                    constexpr wasi_posix_fd_wasm64_t preopen_fd{11};

                    try
                    {
                        switch(scenario)
                        {
                            case 0u:
                            {
                                test_fd_prestat_get(memory, env, preopen_fd, base_off);
                                break;
                            }
                            case 1u:
                            {
                                test_fd_prestat_dir_name(memory, env, preopen_fd, base_off);
                                break;
                            }
                            default:
                            {
                                break;
                            }
                        }
                    }
                    catch(...)
                    {
                        failure.store(true, ::std::memory_order_relaxed);
                        return;
                    }
                }
            });
    }

    for(auto& t: threads) { t.join(); }

    if(failure.load(::std::memory_order_relaxed))
    {
        ::fast_io::io::perrln(::fast_io::u8err(), u8"wasi_hybrid_fd_prestat: failure detected");
        ::fast_io::fast_terminate();
    }
#endif
}
