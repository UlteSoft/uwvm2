/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      OpenAI
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
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <thread>

// macro
#include <uwvm2/utils/macro/push_macros.h>

#ifndef UWVM_MODULE
// import
# include <uwvm2/object/memory/linear/impl.h>
#else
# error "Module testing is not currently supported"
#endif

int main()
{
#if __cpp_lib_atomic_wait >= 201907L
    using memory_t = ::uwvm2::object::memory::linear::allocator_memory_t;

    memory_t memory{1uz};

    constexpr ::std::size_t initial_pages{64uz};
    constexpr ::std::size_t max_pages{4096uz};

    memory.init_by_page_count(initial_pages);

    ::std::atomic_bool start{};
    ::std::atomic_bool done{};
    ::std::atomic_bool failed{};

    auto const wait_for_start{[&]() noexcept
                              {
                                  while(!start.load(::std::memory_order_acquire)) { ::std::this_thread::yield(); }
                              }};

    ::std::thread grower{[&]() noexcept
                         {
                             wait_for_start();

                             for(::std::size_t i{initial_pages}; i != max_pages; ++i)
                             {
                                 if(!memory.grow_strictly(1uz, max_pages))
                                 {
                                     failed.store(true, ::std::memory_order_release);
                                     break;
                                 }

                                 if((i & 63uz) == 0uz) { ::std::this_thread::yield(); }
                             }

                             done.store(true, ::std::memory_order_release);
                         }};

    auto const snapshot_worker{[&]() noexcept
                               {
                                   wait_for_start();

                                   ::std::uint_least8_t byte{};

                                   while(!done.load(::std::memory_order_acquire))
                                   {
                                       bool const ok{
                                           ::uwvm2::object::memory::linear::with_memory_access_snapshot(memory,
                                                                                                       [&](::std::byte* begin,
                                                                                                           ::std::size_t byte_length) noexcept
                                                                                                       {
                                                                                                           if(begin == nullptr || byte_length == 0uz)
                                                                                                               [[unlikely]]
                                                                                                           {
                                                                                                               return false;
                                                                                                           }

                                                                                                           auto const tail{byte_length - 1uz};
                                                                                                           ::std::memcpy(::std::addressof(byte),
                                                                                                                         begin + tail,
                                                                                                                         sizeof(byte));

                                                                                                           byte ^= static_cast<::std::uint_least8_t>(tail);
                                                                                                           ::std::memcpy(begin, ::std::addressof(byte), sizeof(byte));
                                                                                                           return true;
                                                                                                       })};

                                       if(!ok) [[unlikely]]
                                       {
                                           failed.store(true, ::std::memory_order_release);
                                           break;
                                       }

                                       ::std::this_thread::yield();
                                   }
                               }};

    ::std::thread reader0{snapshot_worker};
    ::std::thread reader1{snapshot_worker};

    start.store(true, ::std::memory_order_release);

    grower.join();
    reader0.join();
    reader1.join();

    if(failed.load(::std::memory_order_acquire)) [[unlikely]] { return 1; }
    if(memory.get_page_size() != max_pages) [[unlikely]] { return 2; }
#endif

    return 0;
}
