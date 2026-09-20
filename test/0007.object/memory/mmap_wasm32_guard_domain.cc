#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/object/memory/linear/mmap.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>

#if defined(UWVM_SUPPORT_MMAP)
namespace
{
    namespace memory = ::uwvm2::object::memory::linear;
    namespace memory_signal = ::uwvm2::object::memory::signal;
    static_assert(memory::wasm32_full_protection_front_guard_u64 == 0u);
    static_assert(memory::wasm32_full_protection_usable_u64 == (1ull << 32u));
    static_assert(memory::wasm32_full_protection_back_guard_u64 == (1ull << 32u));
    static_assert(memory::wasm32_max_effective_offset + 63u < memory::max_full_protection_wasm32_length + memory::mmap_guard_max_access_size);

    consteval bool check_unsigned_domain() noexcept
    {
        constexpr ::std::uint_least64_t operands[]{0u, 0x7fffffffu, 0x80000000u, 0xffffffffu};
        constexpr ::std::size_t widths[]{1u, 2u, 4u, 8u, 16u, 64u};
        for(auto address: operands)
        {
            for(auto offset: operands)
            {
                for(auto width: widths)
                {
                    bool const covered{memory::wasm32_full_protection_covers_access(address + offset, width)};
                    if(covered != (sizeof(::std::size_t) >= sizeof(::std::uint_least64_t) &&
                                   sizeof(::std::uintptr_t) >= sizeof(::std::uint_least64_t))) { return false; }
                    if(address + offset + width > memory::max_full_protection_wasm32_length + memory::mmap_guard_max_access_size) { return false; }
                }
            }
        }
        return !memory::wasm32_full_protection_covers_access(memory::wasm32_max_effective_offset + 1u, 1u) &&
               !memory::wasm32_full_protection_covers_access(0u, memory::mmap_guard_max_access_size + 1u);
    }
    static_assert(check_unsigned_domain());
}
#endif

#if defined(UWVM_SUPPORT_MMAP) && !defined(_WIN32) && !defined(__CYGWIN__)
#include <sys/wait.h>
#include <unistd.h>

namespace
{
    // Set before fork; only lock-free operations are used by the signal callback in the child.
    ::std::atomic_uint_least64_t expected_fault_first{};
    ::std::atomic_uint_least64_t expected_fault_last{};
    ::std::atomic_uint_least64_t expected_memory_length{};
    // A write-only internal atomic can be dead-store eliminated together with all of the tested reads before _exit.
    // A volatile sink makes the complete memcpy/checksum observable even when no other thread reads the result.
    volatile ::std::uint_least64_t read_checksum{};

    [[nodiscard]] ::std::byte const* memory_address(memory::mmap_memory_t const& mem,
                                                    ::std::uint_least64_t offset) noexcept
    {
        return reinterpret_cast<::std::byte const*>(
            reinterpret_cast<::std::uintptr_t>(mem.memory_begin) + static_cast<::std::uintptr_t>(offset));
    }

    void on_memory_fault(::uwvm2::object::memory::error::mmap_memory_error_t const& error) noexcept
    {
        bool const correct{error.memory_offset >= expected_fault_first.load(::std::memory_order_relaxed) &&
                           error.memory_offset <= expected_fault_last.load(::std::memory_order_relaxed) &&
                           error.memory_length == expected_memory_length.load(::std::memory_order_relaxed)};
        ::_exit(correct ? 0 : 101);
    }

    [[nodiscard]] bool check_access(memory::mmap_memory_t const& mem, ::std::uint_least64_t offset, ::std::size_t width, bool expect_fault)
    {
        auto expected_first{offset};
#if defined(UWVM_TEST_QEMU_USER_SIGNAL_PAGE_GRANULARITY)
        // QEMU-user reports si_addr at host-page granularity on some targets (observed on ppc64le and s390x), whereas
        // native Linux reports the precise faulting byte. Keep native tests byte-exact; the explicit cross-test mode
        // accepts only the containing page's lower boundary through the actual access's last byte.
        auto const [fault_page_size, page_ok]{::uwvm2::object::memory::platform_page::get_platform_page_size()};
        if(!page_ok || fault_page_size == 0u) { return false; }
        expected_first -= expected_first % fault_page_size;
#endif
        expected_fault_first.store(expect_fault ? expected_first : ::std::numeric_limits<::std::uint_least64_t>::max(),
                                   ::std::memory_order_relaxed);
        expected_fault_last.store(offset + width - 1u, ::std::memory_order_relaxed);
        expected_memory_length.store(mem.memory_length_p->load(::std::memory_order_acquire), ::std::memory_order_relaxed);
        auto const child{::fork()};
        if(child < 0) { return false; }
        if(child == 0)
        {
            memory_signal::detail::mmap_memory_out_of_bounds_func = on_memory_fault;
            ::std::array<::std::byte, memory::mmap_guard_max_access_size> bytes{};
            // memcpy must preserve unaligned access semantics. Consume every byte so all requested bytes are read.
            // Form the address through uintptr_t: the reservation is virtual address space, not a C++ array object whose
            // formal pointer-arithmetic extent is eight GiB.
            ::std::memcpy(bytes.data(), memory_address(mem, offset), width);
            ::std::uint_least64_t checksum{};
            for(::std::size_t index{}; index != width; ++index) { checksum += ::std::to_integer<unsigned>(bytes[index]); }
            read_checksum = checksum;
            ::_exit(expect_fault ? 102 : 0);
        }
        int status{};
        return ::waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }

    [[nodiscard]] bool check_registered_memory(memory::mmap_memory_t const& mem)
    {
        for(auto const& segment: memory_signal::detail::segments)
        {
            if(segment.begin == mem.memory_begin && segment.end == mem.reserved_begin + mem.get_acquire_reserved_space_ceil() &&
               segment.length_p == mem.memory_length_p) { return true; }
        }
        return false;
    }

    [[nodiscard]] int check_live_guard_layout()
    {
        if constexpr(sizeof(::std::size_t) < sizeof(::std::uint_least64_t) || !::std::atomic_uint_least64_t::is_always_lock_free) { return 0; }
        auto const [platform_page, page_ok]{::uwvm2::object::memory::platform_page::get_platform_page_size()};
        if(!page_ok || platform_page == 0u) { return 1; }
        auto const baseline_segments{memory_signal::detail::segments.size()};
        {
            // Use an OS-page-sized Wasm custom page to prove exact protection without assuming the host page is 4 KiB.
            memory::mmap_memory_t original{platform_page, memory::mmap_memory_status_t::wasm32};
            original.init_by_page_count(0u);
            if(original.require_dynamic_determination_memory_size() || original.memory_begin != original.reserved_begin ||
               !check_registered_memory(original) || !check_access(original, 0u, 1u, true)) { return 2; }

            auto const base{original.memory_begin};
            if(!original.grow_strictly(1u) || original.memory_begin != base || !check_access(original, 0u, 1u, false) ||
               !check_access(original, platform_page - 8u, 8u, false) ||
               !check_access(original, platform_page - 7u, 8u, true) ||
               !check_access(original, platform_page, 1u, true)) { return 3; }

            constexpr ::std::uint_least64_t operands[]{0u, 0x7fffffffu, 0x80000000u, 0xffffffffu};
            constexpr ::std::size_t widths[]{1u, 2u, 4u, 8u, 16u, 64u};
            for(auto address: operands)
            {
                for(auto offset: operands)
                {
                    for(auto width: widths)
                    {
                        if(!check_access(original, address + offset, width, address + offset >= platform_page)) { return 4; }
                    }
                }
            }
            // Explicitly probe the last byte of the proven maximum-width interval, including the trailing guard.
            if(!check_access(original, memory::wasm32_max_effective_offset + 63u, 1u, true)) { return 5; }

            // Move construction/assignment transfer the VMA and live-length slot without invalidating signal registration.
            memory::mmap_memory_t moved{::std::move(original)};
            if(original.memory_begin != nullptr || original.reserved_begin != nullptr || moved.memory_begin != base ||
               !check_registered_memory(moved)) { return 6; }
            memory::mmap_memory_t assigned{};
            assigned = ::std::move(moved);
            if(moved.memory_begin != nullptr || !check_registered_memory(assigned)) { return 7; }
            if(!assigned.try_grow_silently(1u) || assigned.memory_begin != base ||
               !check_access(assigned, platform_page, 1u, false) || !check_access(assigned, platform_page * 2u, 1u, true)) { return 8; }
            assigned.clear();
            if(memory_signal::detail::segments.size() != baseline_segments || assigned.memory_begin != nullptr) { return 9; }
            assigned.init_by_page_count(1u);
            if(!check_registered_memory(assigned) || !check_access(assigned, platform_page, 1u, true)) { return 10; }
        }
        if(memory_signal::detail::segments.size() != baseline_segments) { return 11; }
        {
            // Byte-sized custom pages and wasm64 must not be reclassified as the full wasm32 fast path.
            memory::mmap_memory_t byte_pages{1u, memory::mmap_memory_status_t::wasm32};
            if(platform_page > 1u && !byte_pages.require_dynamic_determination_memory_size()) { return 12; }
            memory::mmap_memory_t memory64{platform_page, memory::mmap_memory_status_t::wasm64};
            if(memory64.is_full_page_protection()) { return 13; }
        }
        return 0;
    }
}
#endif

int main()
{
#if defined(UWVM_SUPPORT_MMAP) && !defined(_WIN32) && !defined(__CYGWIN__)
    return check_live_guard_layout();
#else
    return 0;
#endif
}

#include <uwvm2/utils/macro/pop_macros.h>
