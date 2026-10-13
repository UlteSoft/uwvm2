// Actual bundled FastIO allocation-transfer API check only. No native engine
// or restored-world publication is manufactured by this component.
#include <atomic>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <uwvm2/utils/container/impl.h>

int main()
{
    using slots = ::uwvm2::utils::container::vector<::std::uintptr_t>;
    static_assert(::std::is_nothrow_move_constructible_v<slots>);
    static_assert(::std::is_nothrow_move_assignable_v<slots>);
    slots pending{}, record{};
    // This is the same explicit zero initialization used by the private engine.
    pending.resize(4u, 0u);
    if(pending.size() != 4u || pending.data() == nullptr) { return 1; }
    for(auto const value : pending) { if(value != 0u) { return 2; } }
    auto* const emitted_base{pending.data()};
    if(reinterpret_cast<::std::uintptr_t>(emitted_base) %
        ::std::atomic_ref<::std::uintptr_t>::required_alignment != 0u) { return 3; }
    // [owned four-slot allocation] end
    // [safe] move ownership; the retained pointer does not change or advance.
    record = ::std::move(pending);
    if(!pending.empty() || record.size() != 4u || record.data() != emitted_base) { return 4; }
    // [four live complete slots][index 1 < size] end
    // [safe] update exactly the transferred cell; keep its owner through reads.
    ::std::atomic_ref<::std::uintptr_t>{record.index_unchecked(1u)}.store(37u, ::std::memory_order_release);
    if(::std::atomic_ref<::std::uintptr_t>{emitted_base[1u]}.load(::std::memory_order_acquire) != 37u) { return 5; }
    slots moved{::std::move(record)};
    if(!record.empty() || moved.data() != emitted_base || moved.size() != 4u) { return 6; }
    ::std::atomic_ref<::std::uintptr_t>{moved.index_unchecked(1u)}.store(42u, ::std::memory_order_release);
    if(::std::atomic_ref<::std::uintptr_t>{emitted_base[1u]}.load(::std::memory_order_acquire) != 42u) { return 7; }
    slots empty{}, empty_record{};
    empty_record = ::std::move(empty);
    if(!empty.empty() || !empty_record.empty()) { return 8; }
    return 0;
}
