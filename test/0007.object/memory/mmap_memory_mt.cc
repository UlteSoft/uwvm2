/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT
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

// std
#include <cstddef>
#include <cstdint>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
// macro
#include <uwvm2/utils/macro/push_macros.h>

#ifndef UWVM_MODULE
// import
# include <uwvm2/imported/wasi/wasip1/memory/impl.h>
# include <uwvm2/object/memory/linear/mmap.h>
#else
# error "Module testing is not currently supported"
#endif

int main()
{
#if !defined(UWVM_SUPPORT_MMAP)
    // mmap backend is not available on this build target
    // treat as success to keep CI green on unsupported targets
#else

    using namespace ::uwvm2::imported::wasi::wasip1::memory;
    using ::uwvm2::object::memory::linear::mmap_memory_status_t;
    using ::uwvm2::object::memory::linear::mmap_memory_t;

    // wasm32 full-protection configuration by default
    mmap_memory_t mem{};
    mem.init_by_page_count(2u);

    if(mem.memory_begin == nullptr) { ::fast_io::fast_terminate(); }

    // Basic layout
    constexpr std::size_t static_offset{256u};
    constexpr std::size_t stride{16u};
    constexpr unsigned reader_threads{8u};

    // Prepare data within initial committed range
    for(unsigned i = 0; i < reader_threads; ++i)
    {
        std::size_t off{static_offset + static_cast<std::size_t>(i) * stride};
        auto* p = reinterpret_cast<std::uint32_t*>(mem.memory_begin + off);
        *p = 0x5AA50000u + i;
    }

    std::atomic<unsigned> mismatch_count{0u};

    // Two grow_silently threads to create contention; each performs small grows with delays
    fast_io::native_thread grow_a{[&mem]()
                                  {
                                      for(int k = 0; k < 4; ++k)
                                      {
                                          std::this_thread::sleep_for(std::chrono::milliseconds(3));
                                          mem.grow_silently(1u);
                                      }
                                  }};

    fast_io::native_thread grow_b{[&mem]()
                                  {
                                      for(int k = 0; k < 4; ++k)
                                      {
                                          std::this_thread::sleep_for(std::chrono::milliseconds(5));
                                          mem.grow_silently(1u);
                                      }
                                  }};

    // Concurrent readers repeatedly reading stable cells
    std::vector<fast_io::native_thread> readers;
    readers.reserve(reader_threads);
    for(unsigned i = 0; i < reader_threads; ++i)
    {
        readers.emplace_back(
            [&, i]()
            {
                std::uint32_t expected{static_cast<std::uint32_t>(0x5AA50000u + i)};
                std::size_t off{static_offset + static_cast<std::size_t>(i) * stride};
                for(int round = 0; round < 100000; ++round)
                {
                    auto const v = get_basic_wasm_type_from_memory<std::uint32_t>(mem, off);
                    if(v != expected)
                    {
                        mismatch_count.fetch_add(1u, std::memory_order_relaxed);
                        break;
                    }
                }
            });
    }

    for(auto& t: readers) { t.join(); }
    grow_a.join();
    grow_b.join();

    auto const mismatch_count_value = mismatch_count.load(std::memory_order_relaxed);

    if(mismatch_count_value != 0u)
    {
        ::fast_io::io::perr(::fast_io::u8err(), u8"mmap memory test errror\n");
        ::fast_io::fast_terminate();
    }

#endif
}

