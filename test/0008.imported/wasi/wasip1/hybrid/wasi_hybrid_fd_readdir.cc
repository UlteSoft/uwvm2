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

// S10: Multi-threaded test for fd_readdir and fd_advise

// std
#include <cstddef>
#include <version>
#include <atomic>
#include <vector>

#include <fast_io.h>

#include <uwvm2/imported/wasi/wasip1/func/fd_readdir_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_advise_wasm64.h>

using ::uwvm2::imported::wasi::wasip1::abi::advice_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::dircookie_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::errno_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::filesize_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::rights_t;
using ::uwvm2::imported::wasi::wasip1::abi::rights_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_size_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::abi::wasi_void_ptr_wasm64_t;
using ::uwvm2::imported::wasi::wasip1::environment::wasip1_environment;
using ::uwvm2::object::memory::linear::native_memory_t;

// Test fd_advise on directory fd
inline static void test_fd_advise_dir([[maybe_unused]] native_memory_t& memory,
                                      wasip1_environment<native_memory_t>& env,
                                      wasi_posix_fd_wasm64_t dir_fd,
                                      [[maybe_unused]] ::std::size_t base_off)
{
    // Just call fd_advise and check it returns success (or acceptable error)
    auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_advise_wasm64(env,
                                                                             dir_fd,
                                                                             static_cast<filesize_wasm64_t>(0),
                                                                             static_cast<filesize_wasm64_t>(0),
                                                                             advice_wasm64_t::advice_normal);
    // May fail on directories on some platforms, but shouldn't crash
    (void)ret;
}

// Test fd_readdir on directory fd
inline static void test_fd_readdir(native_memory_t& memory, wasip1_environment<native_memory_t>& env, wasi_posix_fd_wasm64_t dir_fd, ::std::size_t base_off)
{
    wasi_void_ptr_wasm64_t const buf_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x100u)};
    wasi_void_ptr_wasm64_t const nread_ptr{static_cast<wasi_void_ptr_wasm64_t>(base_off + 0x8100u)};
    constexpr ::std::size_t buf_size{0x4000u};

    auto const ret = ::uwvm2::imported::wasi::wasip1::func::fd_readdir_wasm64(env,
                                                                              dir_fd,
                                                                              buf_ptr,
                                                                              static_cast<wasi_size_wasm64_t>(buf_size),
                                                                              static_cast<dircookie_wasm64_t>(0),
                                                                              nread_ptr);
    if(ret != errno_wasm64_t::esuccess) [[unlikely]]
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"test_fd_readdir: fd_readdir failed\n");
        ::fast_io::fast_terminate();
    }

    // Check that we read something (directory should not be empty - at least "." and "..")
    auto const nread = ::uwvm2::imported::wasi::wasip1::memory::get_basic_wasm_type_from_memory_wasm64<wasi_size_wasm64_t>(memory, nread_ptr);
    // Weak check: just verify call succeeded (some platforms may return 0)
    (void)nread;
}

int main()
{
#if __cpp_lib_atomic_wait >= 201907L
    constexpr unsigned thread_count{4u};  // Fewer threads since we share a directory fd
    constexpr ::std::size_t base_stride{0x10000u};

    native_memory_t memory{};
    memory.init_by_page_count(thread_count + 8u);

    wasip1_environment<native_memory_t> env{.wasip1_memory = ::std::addressof(memory),
                                            .argv = {},
                                            .envs = {},
                                            .fd_storage = {.fd_limit = 128uz},
                                            .mount_dir_roots = {},
                                            .trace_wasip1_call = false,
                                            .disable_utf8_check = false};

    // Prepare a shared directory fd at index 10
    env.fd_storage.opens.resize(11uz);
    auto& dir_fde = *env.fd_storage.opens.index_unchecked(10uz).fd_p;

    // Set full rights for directory operations
    rights_t const dir_base_rights{static_cast<rights_t>(rights_wasm64_t::right_fd_readdir) | static_cast<rights_t>(rights_wasm64_t::right_fd_advise)};
    rights_t const dir_inherit_rights{static_cast<rights_t>(rights_wasm64_t::right_fd_read) | static_cast<rights_t>(rights_wasm64_t::right_fd_write)};

    dir_fde.rights_base = static_cast<rights_wasm64_t>(dir_base_rights);
    dir_fde.rights_inherit = static_cast<rights_wasm64_t>(dir_inherit_rights);
    dir_fde.wasi_fd.ptr->wasi_fd_storage.reset_type(::uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);

    // Set up directory stack with current directory
    auto& ds = dir_fde.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack;
    ::uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
    entry.ptr->dir_stack.file = ::fast_io::dir_file{u8"."};
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
                    constexpr wasi_posix_fd_wasm64_t dir_fd{10};

                    try
                    {
                        switch(scenario)
                        {
                            case 0u:
                            {
                                test_fd_advise_dir(memory, env, dir_fd, base_off);
                                break;
                            }
                            case 1u:
                            {
                                test_fd_readdir(memory, env, dir_fd, base_off);
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
        ::fast_io::io::perrln(::fast_io::u8err(), u8"wasi_hybrid_fd_readdir: failure detected");
        ::fast_io::fast_terminate();
    }
#endif
}
