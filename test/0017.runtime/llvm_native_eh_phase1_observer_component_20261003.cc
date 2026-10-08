// Genuine native C++/foreign-unwind component, not a Wasm VM qualification.
// Keeper-only compilation/execution with the actual paired C++ EH provider.
// No local native test. Existing full diagnostic capture is not replaced.
#include "llvm_native_eh_phase1_observer_api_20261003.h"
#include <unwind.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <type_traits>
#include <typeinfo>

#if !defined(__linux__) || !defined(__x86_64__) || !defined(__clang__)
#error "Initial component admission is native Clang Linux x86_64 ELF"
#endif

#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PHASE1_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_PHASE1_OBSERVER == 1
#define UWVM_EH_COMPONENT_OBSERVER_ON 1
#else
#define UWVM_EH_COMPONENT_OBSERVER_ON 0
#endif

// Actual propagation APIs intentionally may unwind. Only cleanup/capture and
// management declarations use noexcept; falsely adding it here is incorrect.
extern "C" void uwvm_eh_observer_plain_c_chain_v1(
    unsigned, void *, void (*)(void *))
    __asm__("uwvm_eh_observer_plain_c_chain_v1");
extern "C" _Unwind_Reason_Code uwvm_eh_component_raise_foreign_v1(
    _Unwind_Exception *) __asm__("_Unwind_RaiseException");
extern "C" _Unwind_Reason_Code uwvm_eh_component_backtrace_v1(
    _Unwind_Trace_Fn, void*) noexcept __asm__("_Unwind_Backtrace");
extern "C" decltype(_Unwind_GetIPInfo(static_cast<_Unwind_Context*>(nullptr), static_cast<int*>(nullptr)))
    uwvm_eh_component_ip_info_v1(_Unwind_Context*, int*) noexcept __asm__("_Unwind_GetIPInfo");
extern "C" decltype(_Unwind_GetCFA(static_cast<_Unwind_Context*>(nullptr)))
    uwvm_eh_component_cfa_v1(_Unwind_Context*) noexcept __asm__("_Unwind_GetCFA");
extern "C" decltype(_Unwind_GetRegionStart(static_cast<_Unwind_Context*>(nullptr)))
    uwvm_eh_component_region_v1(_Unwind_Context*) noexcept __asm__("_Unwind_GetRegionStart");

namespace eh_observer_component_20261003
{
    constexpr ::std::size_t capacity{64uz}, workers{4uz}, rounds{32uz};
    ::std::atomic<::std::uint32_t> check_count{}, case_count{}, frame_count{}, plain_frame_count{},
        matched_plain_frame_count{}, finish_count{};
    // Initialized before arm; executable-local trivial TLS proves callback
    // thread identity without OS calls, allocation, locking or std::thread IO.
    constinit thread_local ::std::uint32_t worker_cookie{};

    void require(bool condition, unsigned line) noexcept
    {
        check_count.fetch_add(1u, ::std::memory_order_relaxed);
        if(!condition)
        {
            ::fast_io::io::perrln("FAIL native_eh_phase1_component line=", ::fast_io::mnp::dec(line));
            ::fast_io::fast_terminate();
        }
    }
#define EH_OBSERVER_CHECK(...) ::eh_observer_component_20261003::require(static_cast<bool>((__VA_ARGS__)), __LINE__)

    struct tag_instance final { ::std::uint32_t serial{}; };
    using tag_root = ::std::shared_ptr<tag_instance const>;
    struct guest_exception final
    {
        tag_root tag;
        ::std::uint32_t payload{};
    };
    struct unknown_exception final { ::std::uint32_t payload{}; };

    struct original_cfi_record final
    { ::std::uintptr_t ip{}, cfa{}, region{}; };
    struct original_cfi_capture final
    {
        ::std::array<original_cfi_record, capacity> frames{};
        ::std::size_t size{};
        static _Unwind_Reason_Code callback(_Unwind_Context* native, void* opaque) noexcept
        {
            if(native == nullptr || opaque == nullptr) { return _URC_END_OF_STACK; }
            // [actual provider callback context] [owned original capture ...] end
            // [safe                            ] both are live for this synchronous
            // call; only public ABI getters are used, not a private cursor cast.
            auto* owner{static_cast<original_cfi_capture*>(opaque)};
            if(owner->size == owner->frames.size()) { return _URC_END_OF_STACK; }
            int before{};
            auto ip{static_cast<::std::uintptr_t>(uwvm_eh_component_ip_info_v1(native, ::std::addressof(before)))};
            if(ip != 0u && before == 0) { --ip; } // Integer PC normalization only.
            // [frames.begin ... size] [unused capacity ... end]
            // [safe                 ] select only after the capacity check.
            owner->frames[owner->size] = {ip,
                static_cast<::std::uintptr_t>(uwvm_eh_component_cfa_v1(native)),
                static_cast<::std::uintptr_t>(uwvm_eh_component_region_v1(native))};
            ++owner->size;
            return _URC_NO_REASON;
        }
        void capture() noexcept
        {
            // [this complete owned builder] synchronous provider call only;
            // [safe                       ] the callback never retains it.
            static_cast<void>(uwvm_eh_component_backtrace_v1(callback, this));
        }
    };

    struct observation final
    {
        ::std::array<uwvm_eh_observer_frame_v1, capacity> frames{};
        original_cfi_capture original{};
        uwvm_eh_observer_finish_v1 finish{};
        ::std::uint64_t generation{};
        ::std::size_t size{};
        ::std::uint32_t cookie{worker_cookie}, faults{}, finished{};

        static void frame_callback(void* opaque, ::std::uint64_t generation,
                                   uwvm_eh_observer_frame_v1 record) noexcept
        {
            if(opaque == nullptr) { return; }
            // [actual host-owned observation ...] end; constructed before arm,
            // [safe                             ] live outside the try/catch.
            // ^^ callback receives the same provider-authenticated host context.
            auto* owner{static_cast<observation*>(opaque)};
            if(owner->cookie != worker_cookie || generation == 0u || generation != owner->generation ||
               owner->finished != 0u || record.index != owner->size ||
               record.actual_native_header == 0u || record.actual_thrown_object == 0u ||
               record.cfa == 0u || record.ip_before_instruction > 1u || record.has_personality > 1u)
            { owner->faults |= 1u; return; }
            if(owner->size == owner->frames.size()) { owner->faults |= 2u; return; }
            if(owner->size != 0uz &&
               (record.actual_native_header != owner->frames[0uz].actual_native_header ||
                record.actual_thrown_object != owner->frames[0uz].actual_thrown_object))
            { owner->faults |= 4u; return; }
            // [frames.begin ... size] [unused capacity ... end]
            // [safe                 ] capacity checked before selecting the slot;
            // integer records borrow no provider cursor or exception object.
            owner->frames[owner->size] = record;
            ++owner->size;
        }
        static void finish_callback(void* opaque, ::std::uint64_t generation,
                                    uwvm_eh_observer_finish_v1 record) noexcept
        {
            if(opaque == nullptr) { return; }
            // [same complete host observation] first native search has returned.
            // [safe                          ] cleanup has not retired its owner.
            auto* owner{static_cast<observation*>(opaque)};
            if(owner->cookie != worker_cookie || generation != owner->generation ||
               owner->finished != 0u || record.reported_frames != owner->size ||
               record.bound_exceeded > 1u || record.metadata_failure > 1u ||
               record.native_phase1_reason != static_cast<int>(_URC_NO_REASON))
            { owner->faults |= 8u; }
            if(owner->size != 0uz &&
               (record.actual_native_header != owner->frames[0uz].actual_native_header ||
                record.actual_thrown_object != owner->frames[0uz].actual_thrown_object))
            { owner->faults |= 16u; }
            owner->finish = record;
            ++owner->finished;
        }
        observation()
        {
#if UWVM_EH_COMPONENT_OBSERVER_ON
            // [actual pinned C++ typeinfo] [this complete host object] pinned callbacks
            // [safe                    ] no pointer is guest supplied; destruction
            // disarms the minted generation on every normal/unwind exit.
            generation = uwvm_eh_observer_arm_v1(
                ::std::addressof(typeid(guest_exception)), this, frame_callback, finish_callback);
            EH_OBSERVER_CHECK(generation != 0u);
#endif
        }
        observation(observation const&) = delete;
        observation& operator=(observation const&) = delete;
        ~observation()
        {
#if UWVM_EH_COMPONENT_OBSERVER_ON
            uwvm_eh_observer_disarm_v1(generation);
#endif
        }
        void verify(bool observed, bool overflow, unsigned minimum_plain = 0u) const noexcept
        {
#if UWVM_EH_COMPONENT_OBSERVER_ON
            EH_OBSERVER_CHECK(faults == 0u);
            if(!observed) { EH_OBSERVER_CHECK(size == 0uz && finished == 0u); return; }
            EH_OBSERVER_CHECK(finished == 1u && size != 0uz && size <= capacity);
            EH_OBSERVER_CHECK(finish.metadata_failure == 0u && finish.bound_exceeded == overflow);
            if(overflow) { EH_OBSERVER_CHECK(size == capacity); }
            auto const plain_region{reinterpret_cast<::std::uintptr_t>(uwvm_eh_observer_plain_c_chain_v1)};
            unsigned plain{}, matched{};
            ::std::uintptr_t last_cfa{};
            for(::std::size_t i{}; i != size; ++i)
            {
                auto const& frame{frames[i]};
                if(frame.region_start != plain_region) { continue; }
                EH_OBSERVER_CHECK(frame.has_personality == 0u && frame.cfa != last_cfa);
                last_cfa = frame.cfa;
                ++plain;
                auto ip{frame.raw_ip};
                if(ip != 0u && frame.ip_before_instruction == 0u) { --ip; }
                for(::std::size_t j{}; j != original.size; ++j)
                {
                    auto const& prior{original.frames[j]};
                    if(prior.region == frame.region_start && prior.cfa == frame.cfa)
                    { EH_OBSERVER_CHECK(prior.ip == ip); ++matched; break; }
                }
            }
            EH_OBSERVER_CHECK(plain >= minimum_plain);
            // Deep physical bounds can include different internal helper counts.
            // Require a genuine original-CFI overlap, not invented full coverage.
            EH_OBSERVER_CHECK(matched >= minimum_plain);
            frame_count.fetch_add(static_cast<::std::uint32_t>(size), ::std::memory_order_relaxed);
            plain_frame_count.fetch_add(plain, ::std::memory_order_relaxed);
            matched_plain_frame_count.fetch_add(matched, ::std::memory_order_relaxed);
            finish_count.fetch_add(1u, ::std::memory_order_relaxed);
#else
            // The OFF controller invokes no observer API or replacement stub;
            // all genuine native throw/catch/cleanup cases below still execute.
            static_cast<void>(observed); static_cast<void>(overflow); static_cast<void>(minimum_plain);
            EH_OBSERVER_CHECK(size == 0uz && finished == 0u && generation == 0u);
#endif
        }
    };

    struct throw_request final
    { tag_root tag; ::std::uint32_t payload{}; original_cfi_capture* original{}; };
    [[gnu::noinline]] void throw_guest_leaf(void* opaque)
    {
        EH_OBSERVER_CHECK(opaque != nullptr);
        // [host-owned request ...] end; request is held outside the real catch.
        // [safe                  ] static type established by the actual caller.
        auto const& request{*static_cast<throw_request const*>(opaque)};
        if(request.original != nullptr)
        {
            // [request-owned capture builder] held outside this real try/catch.
            // [safe                        ] capture before native frame removal.
            request.original->capture();
        }
        throw guest_exception{request.tag, request.payload};
    }

    void caught_chain(tag_root const& tag, unsigned depth, ::std::uint32_t payload, bool stale_disarm = false)
    {
        observation capture{};
#if UWVM_EH_COMPONENT_OBSERVER_ON
        // [live capture + actual typeinfo] check a busy arm cannot overwrite it.
        // [safe                          ] no second callback ownership escapes.
        EH_OBSERVER_CHECK(uwvm_eh_observer_arm_v1(::std::addressof(typeid(guest_exception)),
            ::std::addressof(capture), observation::frame_callback, observation::finish_callback) == 0u);
#endif
        throw_request request{tag, payload, ::std::addressof(capture.original)};
        bool caught{};
        try { uwvm_eh_observer_plain_c_chain_v1(depth, ::std::addressof(request), throw_guest_leaf); }
        catch(guest_exception const& ex)
        { caught = ex.tag.get() == tag.get() && ex.payload == payload; }
        EH_OBSERVER_CHECK(caught);
        capture.verify(true, depth > 64u, depth > 64u ? 48u : depth + 1u);
        if(stale_disarm)
        {
            observation next{};
#if UWVM_EH_COMPONENT_OBSERVER_ON
            EH_OBSERVER_CHECK(next.generation > capture.generation);
            uwvm_eh_observer_disarm_v1(capture.generation);
#endif
            try { throw guest_exception{tag, payload + 1u}; }
            catch(guest_exception const& ex) { EH_OBSERVER_CHECK(ex.payload == payload + 1u && ex.tag.get() == tag.get()); }
            next.verify(true, false);
        }
        case_count.fetch_add(1u, ::std::memory_order_relaxed);
    }

    struct cleanup_probe final
    {
        ::std::uint32_t& destroyed;
        ~cleanup_probe() { ++destroyed; }
    };
    [[gnu::noinline]] void tag_mismatch_middle(throw_request& request, tag_root const& unrelated,
                                             ::std::uint32_t& cleanup, ::std::uint32_t& mismatches)
    {
        try
        {
            cleanup_probe local{cleanup};
            uwvm_eh_observer_plain_c_chain_v1(3u, ::std::addressof(request), throw_guest_leaf);
        }
        catch(guest_exception const& ex)
        {
            EH_OBSERVER_CHECK(ex.tag.get() != unrelated.get());
            ++mismatches;
            throw; // Genuine __cxa_rethrow, not a new throw/capture or raw resume.
        }
    }
    void tag_mismatch_rethrow(tag_root const& tag, tag_root const& unrelated)
    {
        observation capture{};
        throw_request request{tag, 0x12345678u, ::std::addressof(capture.original)};
        ::std::uint32_t cleanup{}, mismatches{};
        bool caught{};
        try { tag_mismatch_middle(request, unrelated, cleanup, mismatches); }
        catch(guest_exception const& ex) { caught = ex.tag.get() == tag.get() && ex.payload == request.payload; }
        EH_OBSERVER_CHECK(caught && cleanup == 1u && mismatches == 1u);
        capture.verify(true, false, 4u); // Exactly one finish, despite restarted search.
        case_count.fetch_add(1u, ::std::memory_order_relaxed);
    }

    void unknown_type_declines(tag_root const& tag)
    {
        observation capture{};
        bool caught{};
        try { throw unknown_exception{73u}; }
        catch(unknown_exception const& ex) { caught = ex.payload == 73u; }
        EH_OBSERVER_CHECK(caught);
        capture.verify(false, false);
        // Registration was consumed on mismatching genuine C++ type. A later
        // unarmed guest throw must not retroactively use its old context.
        try { throw guest_exception{tag, 79u}; }
        catch(guest_exception const& ex) { EH_OBSERVER_CHECK(ex.payload == 79u); }
        capture.verify(false, false);
        { observation next{}; }
        case_count.fetch_add(1u, ::std::memory_order_relaxed);
    }

    struct alignas(2uz * sizeof(void*)) foreign_exception final
    {
        // This is a fresh custom-language public _Unwind_Exception allocation,
        // not a guessed C++ private header. Only the two language-owned fields
        // are initialized here; real RaiseException sets its own private state.
        _Unwind_Exception header;
        ::std::uint32_t* cleanup;
        explicit foreign_exception(::std::uint32_t& count)
        {
            // [actual host cleanup counter] declared outside the catch scope.
            // [safe                       ] it outlives foreign header deletion.
            cleanup = ::std::addressof(count);
            header.exception_class = 0x5557564d414f4231ull; // UWVMAOB1, foreign to C++.
            // [actual pinned cleanup entry] accepts only this language's header.
            // [safe                       ] object survives actual propagation.
            header.exception_cleanup = release;
        }
        static void release(_Unwind_Reason_Code, _Unwind_Exception* native) noexcept
        {
            if(native == nullptr) { return; }
            // [fresh language-owned standard-layout object: header at offset 0]
            // [safe                                                         ] the
            // real provider calls this object's registered cleanup once. This
            // is our public foreign layout, never a C++/provider-private cast.
            auto* owner{reinterpret_cast<foreign_exception*>(native)};
            ++*owner->cleanup;
            delete owner;
        }
    };
    static_assert(::std::is_standard_layout_v<foreign_exception>);
    static_assert(offsetof(foreign_exception, header) == 0uz);

    void genuine_foreign_declines()
    {
        observation capture{};
        ::std::uint32_t cleanup{};
        auto owner{::std::make_unique<foreign_exception>(cleanup)};
        bool caught_foreign{}, incorrectly_typed{};
        try
        {
            // [actual freshly allocated foreign header] ownership is transferred
            // [safe                                  ] to its public ABI cleanup;
            // no TLS singleton/header reuse. It remains live across unwinding.
            auto* raw{owner.release()};
            auto const reason{uwvm_eh_component_raise_foreign_v1(::std::addressof(raw->header))};
            // Return means the genuine provider failed to find catch(...).
            // This failure branch still destroys our exclusively owned object.
            foreign_exception::release(reason, ::std::addressof(raw->header));
            EH_OBSERVER_CHECK(false);
        }
        catch(guest_exception const&) { incorrectly_typed = true; }
        catch(...) { caught_foreign = true; }
        EH_OBSERVER_CHECK(caught_foreign && !incorrectly_typed && cleanup == 1u);
        capture.verify(false, false);
        { observation next{}; }
        case_count.fetch_add(1u, ::std::memory_order_relaxed);
    }

    void no_throw_disarm()
    {
        { observation canceled{}; canceled.verify(false, false); }
        { observation next{}; next.verify(false, false); }
        case_count.fetch_add(1u, ::std::memory_order_relaxed);
    }
}

int main()
{
    using namespace eh_observer_component_20261003;
    worker_cookie = 1u;
    auto const tag{::std::make_shared<tag_instance const>(tag_instance{1u})};
    auto const unrelated{::std::make_shared<tag_instance const>(tag_instance{2u})};
    EH_OBSERVER_CHECK(tag.get() != unrelated.get());
    caught_chain(tag, 4u, 41u, true);
    caught_chain(tag, 96u, 43u);
    tag_mismatch_rethrow(tag, unrelated);
    unknown_type_declines(tag);
    genuine_foreign_declines();
    no_throw_disarm();
    ::std::barrier start{static_cast<::std::ptrdiff_t>(workers)};
    ::std::array<::std::thread, workers> threads{};
    for(::std::size_t i{}; i != workers; ++i)
    {
        threads[i] = ::std::thread{[i, &start]
        {
            worker_cookie = static_cast<::std::uint32_t>(i + 2uz);
            auto const own{::std::make_shared<tag_instance const>(tag_instance{worker_cookie})};
            start.arrive_and_wait();
            for(::std::uint32_t round{}; round != rounds; ++round)
            {
                caught_chain(own, 3u, (worker_cookie << 16u) + round, round == 0u);
                if(round % 8u == 0u) { unknown_type_declines(own); }
            }
        }};
    }
    for(auto& thread: threads) { thread.join(); }
    EH_OBSERVER_CHECK(case_count.load() == 6u + workers * (rounds + rounds / 8uz));
    EH_OBSERVER_CHECK(finish_count.load() == (UWVM_EH_COMPONENT_OBSERVER_ON ? 4u + workers * (rounds + 1uz) : 0uz));
    ::fast_io::io::println("{\"family\":\"native-eh-phase1-observer-component\",\"observer_enabled\":",
        UWVM_EH_COMPONENT_OBSERVER_ON ? "true" : "false", ",\"checks\":", ::fast_io::mnp::dec(check_count.load()),
        ",\"cases\":", ::fast_io::mnp::dec(case_count.load()), ",\"workers\":", ::fast_io::mnp::dec(workers),
        ",\"rounds\":", ::fast_io::mnp::dec(rounds), ",\"reported_frames\":", ::fast_io::mnp::dec(frame_count.load()),
        ",\"handler0_c_frames\":", ::fast_io::mnp::dec(plain_frame_count.load()),
        ",\"original_cfi_matched_frames\":", ::fast_io::mnp::dec(matched_plain_frame_count.load()),
        ",\"first_search_finishes\":", ::fast_io::mnp::dec(finish_count.load()),
        ",\"first_search_prefix_only\":true,\"complete_original_trace_observed\":false,"
        "\"diagnostic_replacement\":false,\"vm_qualified\":false,\"performance_qualified\":false}");
}

#undef EH_OBSERVER_CHECK
#undef UWVM_EH_COMPONENT_OBSERVER_ON
