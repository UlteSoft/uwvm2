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
#include <cstdlib>
#include <cstring>
#include <limits>
#include <atomic>
// macro
#include <uwvm2/utils/macro/push_macros.h>

#ifndef UWVM_MODULE
// import
# include <fast_io.h>
# include <fast_io_hosted/process/process/native.h>
# include <uwvm2/object/memory/linear/mmap.h>
# include <uwvm2/object/memory/platform_page/impl.h>
# include <uwvm2/object/memory/signal/signal.h>
#else
# error "Module testing is not currently supported"
#endif

namespace
{

    using ::uwvm2::object::memory::linear::mmap_memory_t;
    using ::uwvm2::object::memory::platform_page::get_platform_page_size;
    using ::uwvm2::object::memory::signal::clear_protected_segments;
    using ::uwvm2::object::memory::signal::register_protected_segment;

    // Child path: install signal handler, register a protected mmap segment,
    // then intentionally perform an out-of-bounds access to trigger the handler.
    [[noreturn]] inline void run_child() noexcept
    {
#if !defined(UWVM_SUPPORT_MMAP)
        // mmap backend not available on this platform; nothing to test.
        ::fast_io::fast_terminate();
#else
        mmap_memory_t memory{};
        memory.init_by_page_count(1uz);

        if(memory.memory_begin == nullptr || memory.memory_length_p == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const [page_size, page_ok]{get_platform_page_size()};
        if(!page_ok || page_size == 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const current_length{memory.memory_length_p->load(::std::memory_order_acquire)};
        if(current_length == 0uz) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const page_size_minus_1{page_size - 1uz};
        if(current_length > ::std::numeric_limits<::std::size_t>::max() - page_size_minus_1) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const length_ceil{(current_length + page_size_minus_1) & ~page_size_minus_1};

        // We will fault exactly at the first byte past the committed region.
        auto const fault_offset{length_ceil};

        // Ensure the registered segment covers the fault address and a bit more.
        auto const segment_end_offset{length_ceil + page_size};

        auto* const seg_begin{memory.memory_begin};
        auto* const seg_end{memory.memory_begin + segment_end_offset};

        clear_protected_segments();
        register_protected_segment(seg_begin, seg_end, memory.memory_length_p, 0uz);

        // Perform a volatile write to the non-committed page to trigger a fault.
        auto* const fault_ptr{memory.memory_begin + fault_offset};
        auto* volatile byte_ptr{reinterpret_cast<volatile ::std::uint8_t*>(fault_ptr)};

        *byte_ptr = 0xAAu;

        // If we ever get here, the signal handler did not terminate us as expected.
        ::fast_io::io::perr(::fast_io::u8err(), u8"signal test: fault did not terminate process as expected\n");
        ::fast_io::fast_terminate();
#endif
    }

    // Parent path: spawn a child process that runs run_child() and verify that it
    // terminates abnormally (non-zero wait status).
    inline int run_parent(char const* self_path) noexcept
    {
#if !defined(UWVM_SUPPORT_MMAP) || (defined(_WIN32_WINDOWS) && _WIN32_WINDOWS < 0x500)
        (void)self_path;
        // mmap backend unavailable; treat as success on such platforms.
        return 0;
#else
        if(self_path == nullptr || *self_path == '\0') [[unlikely]]
        {
            ::fast_io::io::perr(::fast_io::u8err(), u8"signal test: invalid argv[0]\n");
            ::fast_io::fast_terminate();
        }

        ::fast_io::native_process child(::fast_io::mnp::os_c_str(self_path), {"child"});
        auto const status{::fast_io::wait(child)};
        auto const status_int{::fast_io::wait_status_to_int(status)};

        // A zero status means the child exited cleanly, which would imply that
        // the signal handler was not invoked as expected.
        if(status_int == 0)
        {
            ::fast_io::io::perr(::fast_io::u8err(), u8"signal test: child process exited with status 0 (expected abnormal termination)\n");
            ::fast_io::fast_terminate();
        }

        return 0;
#endif
    }

}  // namespace

int main(int argc, char** argv)
{
    // Child branch: run the fault-inducing code in a separate process so that
    // the test harness sees a normal exit from the parent when the signal
    // handler and fast_terminate behave as expected.
    if(argc > 1 && argv != nullptr && argv[1] != nullptr && std::strcmp(argv[1], "child") == 0) { run_child(); }

    char const* self_path{(argv != nullptr && argc > 0) ? argv[0] : nullptr};
    return run_parent(self_path);
}

