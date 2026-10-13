/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <climits>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <exception>
# include <limits>
# include <memory>
# include <span>
# include <type_traits>
# include <uwvm2/object/global/ref.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::gc
{
    using root_reference = ::uwvm2::object::global::wasm_global_ref_t;
    enum class frame_root_status : unsigned char
    { ok, invalid_frame, invalid_reference, rejected_reference, size_overflow };
    struct frame_root_result
    {
        frame_root_status status{frame_root_status::ok};
        ::std::size_t frames{}, visited{};
    };

    // A compiler-owned native record, never a guest linear-memory address.
    // Slots are bytes: LLVM integer allocas and packed interpreter values must
    // not be treated as existing C++ root_reference objects. Read a complete
    // carrier with memcpy into a genuinely constructed native value instead.
    // The high capacity bit records ownership without enlarging the four-word
    // ABI. It is not a guest reference tag or a pointer authentication scheme.
    struct root_frame
    {
        root_frame const* previous{};
        ::std::byte const* slots{};
        ::std::size_t capacity_and_active{};
        ::std::size_t live_count{};
    };
    static_assert(::std::is_standard_layout_v<root_frame> && ::std::is_trivially_copyable_v<root_frame>);
    static_assert(sizeof(root_frame) == 4uz * sizeof(void*));
    static_assert(sizeof(root_reference) == 2uz * sizeof(void*));

    namespace frame_root_details
    {
        inline constexpr ::std::size_t active_bit{::std::size_t{1uz} << (::std::numeric_limits<::std::size_t>::digits - 1u)};
        inline constexpr ::std::size_t capacity_mask{active_bit - 1uz};
        inline constexpr ::std::size_t max_capacity{
            static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(root_reference)};
        inline thread_local root_frame const* head{};

        [[nodiscard]] inline bool runtime_kind(root_reference reference) noexcept
        {
            using kind = ::uwvm2::object::global::wasm_ref_kind;
            switch(reference.kind)
            {
                case kind::wasm_null: case kind::wasm_i31:
                case kind::wasm_func_imported: case kind::wasm_func_defined:
                case kind::wasm_extern: case kind::wasm_exn:
                case kind::wasm_struct: case kind::wasm_array: return true;
                case kind::wasm_func: return false; // Parser index, not a runtime identity.
            }
            return false;
        }
    }

    // Calling-thread access only. A collector may borrow the returned chain
    // after the enrollment/pause protocol proves that every record and slot
    // remains live and immutable through visitation. This does not park a
    // thread, enumerate host handles, or make a VM eligible for collection.
    [[nodiscard]] inline root_frame const* current_root_frames() noexcept
    { return frame_root_details::head; }

    [[nodiscard]] inline bool enter_root_frame(root_frame& frame,
        ::std::byte const* slots, ::std::size_t capacity) noexcept
    {
        if((frame.capacity_and_active & frame_root_details::active_bit) != 0uz ||
           capacity > frame_root_details::max_capacity ||
           (capacity != 0uz && slots == nullptr)) { return false; }
        // [fresh native frame][older calling-thread frame chain]
        // [safe              ] all previous records remain live in their owners.
        // ^^ initialize every borrow before publishing this record in TLS.
        frame.previous = frame_root_details::head;
        frame.slots = slots;
        frame.capacity_and_active = capacity | frame_root_details::active_bit;
        frame.live_count = 0uz;
        frame_root_details::head = ::std::addressof(frame);
        return true;
    }
    [[nodiscard]] inline bool publish_root_frame(root_frame& frame, ::std::size_t count) noexcept
    {
        if(frame_root_details::head != ::std::addressof(frame) ||
           (frame.capacity_and_active & frame_root_details::active_bit) == 0uz ||
           count > (frame.capacity_and_active & frame_root_details::capacity_mask)) { return false; }
        // [0,count) complete reference carriers were initialized by the caller.
        // Pause publication supplies synchronization; no concurrent collector
        // may read this frame while its native owner is still mutating slots.
        frame.live_count = count;
        return true;
    }
    [[nodiscard]] inline bool leave_root_frame(root_frame& frame) noexcept
    {
        if(frame_root_details::head != ::std::addressof(frame) ||
           (frame.capacity_and_active & frame_root_details::active_bit) == 0uz) { return false; }
        // [current live frame][older live caller or null]
        // [safe             ] detach before native storage is released/reused.
        // ^^ restore TLS before clearing any of the borrowed record fields.
        frame_root_details::head = frame.previous;
        frame.previous = nullptr;
        frame.slots = nullptr;
        frame.capacity_and_active = frame.live_count = 0uz;
        return true;
    }

    class scoped_root_frame
    {
        root_frame frame_{};
        bool entered_{};
    public:
        explicit scoped_root_frame(::std::span<root_reference const> slots) noexcept
            : entered_{enter_root_frame(frame_, reinterpret_cast<::std::byte const*>(slots.data()), slots.size())} {}
        scoped_root_frame(scoped_root_frame const&) = delete;
        scoped_root_frame& operator=(scoped_root_frame const&) = delete;
        scoped_root_frame(scoped_root_frame&&) = delete;
        scoped_root_frame& operator=(scoped_root_frame&&) = delete;
        ~scoped_root_frame() noexcept
        { if(entered_ && !leave_root_frame(frame_)) { ::std::terminate(); } }
        [[nodiscard]] explicit operator bool() const noexcept { return entered_; }
        [[nodiscard]] bool publish(::std::size_t count) noexcept
        { return entered_ && publish_root_frame(frame_, count); }
        [[nodiscard]] root_frame const& record() const noexcept { return frame_; }
    };

    // The caller holds the complete native chain through a stopped participant
    // or the collecting initiator's own live scope. All byte spans are generated
    // from validated reference locations, not guessed by scanning stack bits.
    // First validate every record/kind before delivering any root. A known
    // kind does not prove token liveness: the visitor must use the GC codec.
    // Discard a rejected snapshot; do not sweep or retain a borrow in visitor.
    template<class Visitor>
    [[nodiscard]] inline frame_root_result visit_quiescent_frame_roots(
        root_frame const* head, Visitor&& visitor, ::std::size_t max_frames = 32768uz) noexcept
    {
        static_assert(::std::is_nothrow_invocable_r_v<bool, Visitor&, root_reference>);
        frame_root_result result{};
        ::std::size_t total{};
        for(auto const* current{head}; current != nullptr;)
        {
            auto const capacity{current->capacity_and_active & frame_root_details::capacity_mask};
            if(result.frames == max_frames ||
               (current->capacity_and_active & frame_root_details::active_bit) == 0uz ||
               capacity > frame_root_details::max_capacity || current->live_count > capacity ||
               (capacity != 0uz && current->slots == nullptr))
            { result.status = frame_root_status::invalid_frame; return result; }
            if(current->live_count > (::std::numeric_limits<::std::size_t>::max)() - total)
            { result.status = frame_root_status::size_overflow; return result; }
            total += current->live_count;
            for(::std::size_t index{}; index != current->live_count; ++index)
            {
                root_reference reference{};
                // [capacity complete native byte slots] end
                // [safe                              ] index < live_count <= capacity
                //          ^^ offset fits ptrdiff_t and names one initialized carrier.
                ::std::memcpy(::std::addressof(reference),
                    current->slots + index * sizeof(root_reference), sizeof(reference));
                if(!frame_root_details::runtime_kind(reference))
                { result.status = frame_root_status::invalid_reference; return result; }
            }
            ++result.frames;
            // [borrowed live native chain] the pause/owner holds every record.
            // ^^ advance only through its initialized previous link or null.
            current = current->previous;
        }
        for(auto const* current{head}; current != nullptr;)
        {
            for(::std::size_t index{}; index != current->live_count; ++index)
            {
                root_reference reference{};
                // [proved immutable native slots] index stayed below the same
                // complete extent checked in the preflight pass above.
                // ^^ copy one complete carrier; no integer payload is dereferenced.
                ::std::memcpy(::std::addressof(reference),
                    current->slots + index * sizeof(root_reference), sizeof(reference));
                if(!visitor(reference))
                { result.status = frame_root_status::rejected_reference; return result; }
                ++result.visited;
            }
            // [same protected native chain] no frame can leave during visitation.
            // ^^ follow only its live previous link; retain no borrowed address.
            current = current->previous;
        }
        return result;
    }

    // Internal generated-code ABI: frame_address names FRESH, properly aligned
    // native storage of sizeof(root_frame), retained until the matching leave.
    // Begin its C++ lifetime here; LLVM slot bytes stay raw and are read only
    // with memcpy. No guest/import API accepts these native addresses. Caller
    // generation must handle failure and emit leave on return/EH/musttail;
    // these bridges alone do not qualify that generated control flow. These
    // leaf operations never request VM collection, park a participant, or call
    // guest/host callbacks. That contract protects incoming reference arguments
    // before the generated callee publishes them; it is not an async-stop
    // root guarantee. Native TLS initialization may use the platform runtime.
    extern "C" [[nodiscard]] inline ::std::uintptr_t uwvm_gc_root_frame_enter_abi(
        ::std::uintptr_t frame_address, ::std::uintptr_t slots_address, ::std::size_t capacity) noexcept
    {
        if(frame_address == 0u || frame_address % alignof(root_frame) != 0u ||
           capacity > frame_root_details::max_capacity ||
           (capacity != 0uz && slots_address == 0u)) { return 0u; }
        // [fresh compiler-owned native record storage] complete and aligned.
        // ^^ placement construction begins its native C++ object lifetime.
        auto* frame{::std::construct_at(reinterpret_cast<root_frame*>(frame_address))};
        return enter_root_frame(*frame, reinterpret_cast<::std::byte const*>(slots_address), capacity) ? 1u : 0u;
    }
    extern "C" [[nodiscard]] inline ::std::uintptr_t uwvm_gc_root_frame_publish_abi(
        ::std::uintptr_t frame_address, ::std::size_t count) noexcept
    {
        if(frame_address == 0u || frame_address % alignof(root_frame) != 0u) { return 0u; }
        // [live record constructed by enter_abi] retained by its generated frame.
        // ^^ convert only the compiler-owned native record address.
        return publish_root_frame(*reinterpret_cast<root_frame*>(frame_address), count) ? 1u : 0u;
    }
    extern "C" [[nodiscard]] inline ::std::uintptr_t uwvm_gc_root_frame_leave_abi(
        ::std::uintptr_t frame_address) noexcept
    {
        if(frame_address == 0u || frame_address % alignof(root_frame) != 0u) { return 0u; }
        // [live generated record] do not end its lifetime on a rejected leave.
        auto* frame{reinterpret_cast<root_frame*>(frame_address)};
        if(!leave_root_frame(*frame)) { return 0u; }
        // [detached native record] no TLS or collector borrow remains.
        // ^^ end the C++ lifetime before generated stack storage is released.
        ::std::destroy_at(frame);
        return 1u;
    }

    // Generated functions use these fail-fast entries. A rejected native
    // lifetime/LIFO operation must never continue with a missing or stale root
    // chain. The recoverable ABI above remains available to cold validators
    // and native probes; no check is added to a guest memory/field access.
    extern "C" [[nodiscard]] inline ::std::uintptr_t uwvm_gc_root_frame_enter_checked_abi(
        ::std::uintptr_t frame_address, ::std::uintptr_t slots_address, ::std::size_t capacity) noexcept
    {
        if(uwvm_gc_root_frame_enter_abi(frame_address, slots_address, capacity) != 1u)
        { ::std::terminate(); }
        return 1u;
    }
    extern "C" [[nodiscard]] inline ::std::uintptr_t uwvm_gc_root_frame_leave_checked_abi(
        ::std::uintptr_t frame_address) noexcept
    {
        if(uwvm_gc_root_frame_leave_abi(frame_address) != 1u) { ::std::terminate(); }
        return 1u;
    }
}
