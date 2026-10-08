// Real 32-bit address-space pressure, growth limits and signal ownership for Core 3 memories.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/object/memory/linear/mmap.h>
#include <uwvm2/utils/container/impl.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <sys/wait.h>
#include <unistd.h>

#if !defined(UWVM_SUPPORT_MMAP)
# error "This regression requires the native mmap memory implementation"
#endif
namespace linear = uwvm2::object::memory::linear;
namespace signals = uwvm2::object::memory::signal;
static void require(bool condition) { if(!condition) { std::abort(); } }
static void memory_fault(uwvm2::object::memory::error::mmap_memory_error_t const& error) noexcept
{
    ::_exit(error.memory_idx == 129 && error.memory_offset == 4 * 65536 && error.memory_length == 4 * 65536 ? 0 : 7);
}
int main()
{
    auto const baseline{signals::detail::segments.size()};
    {
        // Use the production container allocator, including its over-aligned
        // element path. Stack-only arrays would hide an allocation alignment bug.
        uwvm2::utils::container::vector<linear::mmap_memory_t> memories(130);
        for(std::size_t i{}; i != memories.size(); ++i)
        {
            auto& memory{memories[i]};
            require(reinterpret_cast<std::uintptr_t>(&memory) % alignof(linear::mmap_memory_t) == 0);
            memory.init_by_page_count(1, 4, i);
            require(memory.get_page_size() == 1);
            require(memory.require_dynamic_determination_memory_size() == (sizeof(std::size_t) == 4));
            if constexpr(sizeof(std::size_t) == 4)
            {
                require(memory.get_acquire_reserved_space_ceil() < 1024 * 1024);
            }
            // Each independently initialized memory must preserve its own bytes through growth/moves.
            std::memcpy(memory.memory_begin, &i, sizeof(i));
            require(memory.grow_strictly(1, 4 * 65536));
            require(memory.try_grow_silently(2, 4 * 65536));
            require(!memory.try_grow_silently(1, 4 * 65536));
            require(memory.get_page_size() == 4);
            if constexpr(sizeof(std::size_t) == 4)
            {
                // Even a caller accidentally passing SIZE_MAX cannot commit outside this reservation.
                require(!memory.try_grow_silently(1));
                require(!memory.grow_strictly(1));
            }
            std::size_t value{};
            std::memcpy(&value, memory.memory_begin, sizeof(value));
            require(value == i);
        }
        // Reallocation moves the records but keeps each reservation, length
        // atomic and signal registration alive. No compiled function exists yet.
        memories.reserve(300);
        for(std::size_t i{}; i != memories.size(); ++i)
        {
            auto const& memory{memories[i]};
            require(reinterpret_cast<std::uintptr_t>(&memory) % alignof(linear::mmap_memory_t) == 0);
            require(memory.get_page_size() == 4);
            std::size_t value{};
            std::memcpy(&value, memory.memory_begin, sizeof(value));
            require(value == i);
        }
        require(signals::detail::segments.size() == baseline + memories.size());
        linear::mmap_memory_t moved{std::move(memories[129])};
        linear::mmap_memory_t assigned{};
        assigned = std::move(moved);
        require(assigned.get_page_size() == 4);
        if constexpr(sizeof(std::size_t) == 4) { require(!assigned.try_grow_silently(1)); }
        // The registration follows the allocation and retains its original owner index after moves.
        auto child{::fork()};
        require(child >= 0);
        if(child == 0)
        {
            signals::detail::mmap_memory_out_of_bounds_func = memory_fault;
            auto const address{reinterpret_cast<std::uintptr_t>(assigned.memory_begin) + 4 * 65536};
            static_cast<void>(*reinterpret_cast<volatile unsigned char const*>(address));
            ::_exit(8);
        }
        int status{};
        require(::waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
        assigned.clear();
        assigned.init_by_page_count(0, 0, 129);
        require(assigned.get_page_size() == 0 && assigned.try_grow_silently(0, 0));
        require(!assigned.try_grow_silently(1, 0));
        if constexpr(sizeof(std::size_t) == 4) { require(!assigned.try_grow_silently(1)); }
    }
    require(signals::detail::segments.size() == baseline);
    std::puts("PASS 130 memory instances: bounded reservations, both grow paths, move/clear and index-129 signal ownership");
}
