// Actual internal root ABI + explicitly overlaid aggregate collector. This
// proves native scopes/pause contexts, not VM compiler maps or automatic GC.
#include <uwvm2/runtime/gc/frame_roots.h>
#include <uwvm2/utils/thread/collection_pause_domain.h>
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <fast_io.h>
#include <array>
#include <chrono>
#include <cstring>
#include <latch>
#include <memory>
#include <thread>

// Each product header restores its target-detection macros on exit. Derive
// actual target support again for this fixture's own conditional scope.
#include <uwvm2/utils/macro/push_macros.h>

namespace roots = ::uwvm2::runtime::gc;
namespace gc = ::uwvm2::uwvm::runtime::storage;
namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace threading = ::uwvm2::utils::thread;
namespace
{
    unsigned checks{}, collections{};
    ::std::size_t total_reclaimed{};
    void require(bool ok, unsigned line) noexcept
    {
        ++checks;
        if(!ok)
        {
            ::fast_io::io::perrln("FAIL native frame roots line ", line);
            ::fast_io::fast_terminate();
        }
    }
# define CHECK(condition) require((condition), __LINE__)
    t::recursive_type_section declarations()
    {
        t::recursive_type_section section{};
        section.type_count = 1u;
        t::recursive_group group{};
        t::sub_type type{};
        type.kind = t::composite_kind::struct_;
        t::field_type field{};
        field.storage.value.kind = t::value_kind::i32;
        field.mutable_ = true;
        type.fields.push_back(field);
        group.types.push_back(::std::move(type));
        section.groups.push_back(::std::move(group));
        return section;
    }
    gc::gc_reference make(gc::gc_object_store& store, ::std::uint32_t key)
    {
        gc::gc_reference value{};
        auto input{gc::gc_object_value::i32(key)};
        CHECK(store.struct_new(0u, &input, 1uz, value) == gc::gc_object_status::ok);
        return value;
    }
    struct snapshot
    {
        ::std::array<gc::gc_reference, 32> values{};
        ::std::size_t size{};
        bool operator()(gc::gc_reference reference) noexcept
        {
            if(size == values.size()) { return false; }
            // [0,size) initialized carriers][one free native slot] end
            // [safe                                           ] size < capacity.
            values[size++] = reference;
            return true;
        }
    };
    void collect(::std::shared_ptr<gc::gc_object_store> const& store,
                 snapshot const& live, ::std::size_t expected)
    {
        ::std::array cohort{store};
        ::std::size_t reclaimed{};
        CHECK(gc::gc_object_store::collect_exclusive_aggregate_domain(
            cohort.data(), cohort.size(), live.values.data(), live.size, reclaimed) == gc::gc_object_status::ok);
        CHECK(reclaimed == expected);
        ++collections;
        total_reclaimed += reclaimed;
    }
    void collect_current(::std::shared_ptr<gc::gc_object_store> const& store,
                         ::std::size_t expected, ::std::size_t frames, ::std::size_t carriers)
    {
        snapshot live{};
        auto result{roots::visit_quiescent_frame_roots(roots::current_root_frames(), live)};
        CHECK(result.status == roots::frame_root_status::ok && result.frames == frames && result.visited == carriers);
        CHECK(live.size == carriers);
        collect(store, live, expected);
    }
    struct propagation {};
    [[gnu::noinline]] void throwing_scope(::std::shared_ptr<gc::gc_object_store> const& store,
                                         gc::gc_reference const& reference)
    {
        roots::scoped_root_frame frame{{&reference, 1uz}};
        CHECK(frame && frame.publish(1uz));
        collect_current(store, 0uz, 2uz, 13uz);
        throw propagation{};
    }
    struct root_context { roots::root_frame const* head{}; };
}

int main()
{
    CHECK(roots::current_root_frames() == nullptr);
    auto section{declarations()};
    auto leases{::std::make_shared<gc::gc_lease_owner>()};
    auto store{::std::make_shared<gc::gc_object_store>(section, leases)};
    CHECK(store->valid());
    ::std::array<gc::gc_reference, 12> references{};
    for(::std::size_t index{}; index != references.size(); ++index)
    { references[index] = make(*store, static_cast<::std::uint32_t>(index + 11uz)); }
    auto garbage{make(*store, 999u)};
    {
        roots::scoped_root_frame outer{references};
        CHECK(outer && outer.publish(references.size()));
        CHECK(!outer.publish(references.size() + 1uz));
        collect_current(store, 1uz, 1uz, 12uz);
        bool caught{};
        try { throwing_scope(store, references[7]); }
        catch(propagation const&) { caught = true; }
        CHECK(caught && roots::current_root_frames() == &outer.record());
        gc::gc_object_value observed{};
        CHECK(store->struct_get(garbage, 0u, false, observed) == gc::gc_object_status::invalid_reference);
        for(::std::size_t index{}; index != references.size(); ++index)
        {
            CHECK(store->struct_get(references[index], 0u, false, observed) == gc::gc_object_status::ok);
            CHECK(observed.as<::std::uint32_t>() == index + 11uz);
        }
        roots::root_frame inner{};
        CHECK(roots::enter_root_frame(inner, ::std::as_bytes(::std::span{references}).data(), 2uz));
        CHECK(roots::publish_root_frame(inner, 2uz));
        CHECK(!roots::enter_root_frame(inner, nullptr, 0uz));
        CHECK(!roots::publish_root_frame(const_cast<roots::root_frame&>(outer.record()), 0uz));
        CHECK(!roots::leave_root_frame(const_cast<roots::root_frame&>(outer.record())));
        snapshot live{};
        auto const saved{references[1]};
        references[1].kind = static_cast<::uwvm2::object::global::wasm_ref_kind>(255u);
        auto result{roots::visit_quiescent_frame_roots(roots::current_root_frames(), live)};
        CHECK(result.status == roots::frame_root_status::invalid_reference && live.size == 0uz);
        references[1] = saved;
        auto const previous{inner.previous};
        // Private malformed native metadata: all referenced records remain
        // alive. Bound a cycle without ever scanning an arbitrary pointer.
        inner.previous = &inner;
        result = roots::visit_quiescent_frame_roots(&inner, live, 3uz);
        CHECK(result.status == roots::frame_root_status::invalid_frame && live.size == 0uz);
        inner.previous = previous;
        CHECK(roots::leave_root_frame(inner));
    }
    CHECK(roots::current_root_frames() == nullptr);
    collect_current(store, 12uz, 0uz, 0uz);

    // The generated-code ABI constructs a genuine C++ record in fresh native
    // byte storage; reference slots remain raw bytes throughout the bridge.
    auto abi_reference{make(*store, 44u)};
    alignas(roots::root_frame) ::std::array<::std::byte, sizeof(roots::root_frame)> raw_frame{};
    ::std::array<::std::byte, sizeof(gc::gc_reference)> raw_slots{};
    ::std::memcpy(raw_slots.data(), &abi_reference, sizeof(abi_reference));
    auto const frame_address{reinterpret_cast<::std::uintptr_t>(raw_frame.data())};
    auto const slots_address{reinterpret_cast<::std::uintptr_t>(raw_slots.data())};
    CHECK(roots::uwvm_gc_root_frame_enter_abi(0u, slots_address, 1uz) == 0u);
    CHECK(roots::uwvm_gc_root_frame_enter_abi(frame_address + 1u, slots_address, 1uz) == 0u);
    CHECK(roots::uwvm_gc_root_frame_enter_abi(frame_address, 0u, 1uz) == 0u);
    CHECK(roots::current_root_frames() == nullptr);
    CHECK(roots::uwvm_gc_root_frame_enter_abi(frame_address, slots_address, 1uz) == 1u);
    CHECK(roots::uwvm_gc_root_frame_publish_abi(frame_address, 2uz) == 0u);
    CHECK(roots::uwvm_gc_root_frame_publish_abi(frame_address, 1uz) == 1u);
    collect_current(store, 0uz, 1uz, 1uz);
    CHECK(roots::uwvm_gc_root_frame_leave_abi(frame_address) == 1u);
    CHECK(roots::current_root_frames() == nullptr);
    collect_current(store, 1uz, 0uz, 0uz);

#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    auto main_reference{make(*store, 51u)};
    ::std::array worker_references{make(*store, 61u), make(*store, 71u)};
    auto unrooted{make(*store, 999u)};
    threading::collection_pause_domain domain{3uz};
    ::std::latch ready{2}, release{1};
    auto participant{domain.enter()};
    CHECK(static_cast<bool>(participant));
    roots::scoped_root_frame main_frame{{&main_reference, 1uz}};
    CHECK(main_frame && main_frame.publish(1uz));
    root_context main_context{roots::current_root_frames()};
    auto worker = [&](gc::gc_reference reference) noexcept
    {
        auto enrolled{domain.enter()};
        if(!enrolled) { ::fast_io::fast_terminate(); }
        roots::scoped_root_frame frame{{&reference, 1uz}};
        if(!frame || !frame.publish(1uz)) { ::fast_io::fast_terminate(); }
        root_context context{roots::current_root_frames()};
        {
            auto blocking{enrolled.park_for_blocking(&context)};
            if(!blocking) { ::fast_io::fast_terminate(); }
            ready.count_down();
            release.wait();
        }
        // No root/header is changed until blocking scope honors pause release.
    };
    ::std::thread first{worker, worker_references[0]}, second{worker, worker_references[1]};
    ready.wait();
    auto ticket{domain.request_pause(participant, &main_context)};
    CHECK(static_cast<bool>(ticket));
    CHECK(domain.wait_until_paused(ticket, ::std::chrono::steady_clock::now() + ::std::chrono::seconds{3}) ==
          threading::collection_pause_result::paused);
    auto paused{domain.while_stopped(ticket, [&](auto const& stopped) noexcept
    {
        snapshot live{};
        ::std::size_t contexts{}, blocked{}, initiating{};
        stopped.for_each([&](auto const& entry) noexcept
        {
            if(entry.root_context == nullptr) { ::fast_io::fast_terminate(); }
            // [actual typed context owned by this fixture's enrolled thread]
            // [safe] stopped_view's mutex publication keeps its frame/span live.
            auto const* context{static_cast<root_context const*>(entry.root_context)};
            auto result{roots::visit_quiescent_frame_roots(context->head, live)};
            if(result.status != roots::frame_root_status::ok || result.frames != 1uz || result.visited != 1uz)
            { ::fast_io::fast_terminate(); }
            ++contexts;
            blocked += entry.blocking;
            initiating += entry.collecting;
        });
        CHECK(contexts == 3uz && blocked == 2uz && initiating == 1uz && live.size == 3uz);
        collect(store, live, 1uz);
    })};
    CHECK(paused == threading::collection_pause_result::paused);
    ticket.reset();
    release.count_down();
    first.join(); second.join();
    gc::gc_object_value observed{};
    for(auto reference: worker_references)
    { CHECK(store->struct_get(reference, 0u, false, observed) == gc::gc_object_status::ok); }
    CHECK(store->struct_get(unrooted, 0u, false, observed) == gc::gc_object_status::invalid_reference);
    // This fixture performs exactly six collections. main_frame leaves at
    // scope exit; participant is reset before the domain destructor drains
    // the enrolled set. No additional empty-root sweep is performed here.
    participant.reset();
#else
# error "native frame pause proof requires the actual supported thread infrastructure"
#endif
    CHECK(collections == 6u && total_reclaimed == 15uz);
    ::fast_io::io::println("PASS native frame roots: ", checks, " checks; ", collections,
        " actual collections; ", total_reclaimed,
        " reclaimed; native root ABI, 12 live references, C++ cleanup, strict LIFO and 2 parked worker contexts; no VM compiler or automatic GC qualification");
}
#undef CHECK
#include <uwvm2/utils/macro/pop_macros.h>
