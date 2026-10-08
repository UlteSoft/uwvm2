// Real bounded-memory64 instantiation/growth regression; Linux cgroup only.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/object/memory/linear/impl.h>
#include <fast_io.h>
#include <cstddef>
#include <limits>
#include <utility>

#define CHECK(condition) do { if(!(condition)) { ::fast_io::io::perr("FAIL ", __LINE__, ": ", #condition, "\n"); ::fast_io::fast_terminate(); } } while(false)
namespace linear = ::uwvm2::object::memory::linear;
static_assert(sizeof(::std::size_t) >= 8);
#if !defined(UWVM_SUPPORT_MMAP)
#error This regression requires the actual mmap backend.
#endif

int main()
{
    for(bool shared : {false, true})
    {
        linear::mmap_memory_t memory{65536, linear::mmap_memory_status_t::wasm64};
        memory.sequentially_consistent_size = shared;
        memory.init_by_page_count(1, 3);
        CHECK(memory.reservation_limit_bytes == 3 * 65536);
        CHECK(memory.require_dynamic_determination_memory_size());
        CHECK(memory.get_acquire_reserved_space_ceil() < 1024 * 1024);
        CHECK(!memory.is_full_page_protection() && memory.get_page_size() == 1);
        CHECK(memory.memory_begin[0] == ::std::byte{});
        memory.memory_begin[0] = ::std::byte{0x71};
        ::std::size_t old{};
        CHECK(memory.try_grow_silently(1, ::std::numeric_limits<::std::size_t>::max(), &old) && old == 1);
        CHECK(memory.grow_strictly(1, ::std::numeric_limits<::std::size_t>::max(), &old) && old == 2);
        CHECK(memory.get_page_size() == 3 && memory.memory_begin[0] == ::std::byte{0x71});
        CHECK(memory.memory_begin[2 * 65536] == ::std::byte{});
        CHECK(!memory.try_grow_silently(1, ::std::numeric_limits<::std::size_t>::max(), &old));
        CHECK(!memory.grow_strictly(1, ::std::numeric_limits<::std::size_t>::max(), &old));
        CHECK(memory.get_page_size() == 3);
        CHECK(memory.try_grow_silently(0, 3 * 65536, &old) && old == 3);
        linear::mmap_memory_t moved{::std::move(memory)};
        CHECK(moved.reservation_limit_bytes == 3 * 65536 && moved.get_page_size() == 3);
        CHECK(moved.require_dynamic_determination_memory_size());
        CHECK(moved.sequentially_consistent_size == shared && memory.memory_begin == nullptr);
        memory = ::std::move(moved);
        CHECK(memory.reservation_limit_bytes == 3 * 65536 && memory.get_page_size() == 3);
        CHECK(moved.memory_begin == nullptr);
        memory.clear();
        memory.init_by_page_count(1, 1);
        CHECK(memory.reservation_limit_bytes == 65536 && memory.get_page_size() == 1);
        CHECK(memory.require_dynamic_determination_memory_size());
        CHECK(!memory.try_grow_silently(1, 2 * 65536));
    }
    {
        linear::mmap_memory_t empty{65536, linear::mmap_memory_status_t::wasm64};
        empty.init_by_page_count(0, 0);
        CHECK(empty.reservation_limit_bytes == 0 && empty.get_page_size() == 0);
        CHECK(empty.require_dynamic_determination_memory_size());
        CHECK(!empty.try_grow_silently(1, 65536));
        CHECK(empty.try_grow_silently(0, 0));
    }
    {
        linear::mmap_memory_t full_partial{65536, linear::mmap_memory_status_t::wasm64};
        full_partial.init_by_page_count(0);
        CHECK(full_partial.reservation_limit_bytes == linear::max_partial_protection_wasm64_length);
        CHECK(!full_partial.require_dynamic_determination_memory_size());
    }
    {
        linear::mmap_memory_t memory32{};
        memory32.init_by_page_count(0, 0);
        CHECK(memory32.is_full_page_protection());
        CHECK(!memory32.require_dynamic_determination_memory_size());
        CHECK(memory32.reservation_limit_bytes == ::std::numeric_limits<::std::size_t>::max());
        CHECK(memory32.get_acquire_reserved_space() > (1ULL << 33));
    }
    ::fast_io::io::println("PASS memory64 declared reservation: shared/unshared, grow limit, zero maximum, move, memory32 full guard");
}
