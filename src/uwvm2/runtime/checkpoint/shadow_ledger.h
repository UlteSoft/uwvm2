/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include "materialization.h"
# include <array>
# include <cstring>
# include <type_traits>
# include <uwvm2/object/global/ref.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
#include <uwvm2/utils/macro/push_macros.h>
UWVM_MODULE_EXPORT namespace uwvm2::runtime::checkpoint
{
    using native_reference = ::uwvm2::object::global::wasm_global_ref_t;
    struct activation_identity
    {
        ::std::uint64_t incarnation{}, parent{}, continuation{}, runtime_epoch{};
        friend bool operator==(activation_identity const&, activation_identity const&) = default;
    };
    struct native_value
    {
        typed_slot declaration{};
        // Private host ABI bits, NEVER wire bytes or debugger-visible pointers.
        // The GC/extern exporter must census actual typed owners and produce IDs.
        ::std::array<::std::byte, native_slot_bytes> bits{};
    };
    struct logical_frame
    {
        activation_identity identity{};
        sealed_function_plan::owner plan{};
        ::std::uint64_t site{};
        ::std::vector<native_value> values{};
        bool materialized{};
        // Owned numeric DATA, never native addresses or roots. Small, completely
        // initialized numeric packets need no per-slot type expansion until an
        // authenticated cold reader requests the frame. Every byte is copied
        // synchronously before the producer's workspace may change or retire.
        ::std::array<::std::byte, 8u * native_slot_bytes> numeric_packet{};
        bool numeric_packet_pending{};
    };
    // Calling-thread-only native materialization ledger. A public scalar frame
    // ID cannot read it: the runtime must still use its actual participant,
    // canonical publication/control-block owner, generation lease and worldstop.
    // No method below exposes a snapshot/restore management capability.
    class shadow_ledger
    {
        compilation_profile::owner profile_{};
        ::std::vector<logical_frame> frames_{};
        // Private borrows into separately owned native activation storage.
        // Never copied into logical_frame/captures or retained after release.
        ::std::vector<::std::span<::std::byte>> observer_storage_{};
        // Storage follows the deepest actual activation. A bounded private
        // observer cache may retain immutable plan DATA only. Retired slots
        // have no identity, values, borrow or availability and are never exposed.
        // Reuse their already constructed numeric byte storage without treating
        // any previous bytes as initialized state in the next activation.
        ::std::size_t active_frame_count_{};
        ::std::vector<::std::uint8_t> compiler_metadata_reuse_{};
        ::std::size_t retained_metadata_count_{};
        inline static constexpr ::std::size_t retained_metadata_limit{32u};
        [[nodiscard]] bool active_frames_empty() const noexcept { return active_frame_count_ == 0u; }
        [[nodiscard]] logical_frame& active_frame() noexcept { return frames_[active_frame_count_ - 1u]; }
        [[nodiscard]] logical_frame const& active_frame() const noexcept { return frames_[active_frame_count_ - 1u]; }
        [[nodiscard]] ::std::span<logical_frame> live_frames() noexcept { return {frames_.data(),active_frame_count_}; }
        [[nodiscard]] ::std::span<logical_frame const> live_frames() const noexcept { return {frames_.data(),active_frame_count_}; }
        // Empty value storage only. Plans, identities, materialized state and
        // reference cells never survive retirement. Bound cached capacity to
        // 32 * 16 KiB per owning thread; larger/deeper frames free normally.
        ::std::array<::std::vector<native_value>,32u> recycled_values_{};
        ::std::size_t recycled_count_{};
        void recycle_values(::std::vector<native_value>& values) noexcept
        {
            if(values.capacity() != 0u && values.capacity() <= 16384u / sizeof(native_value) && recycled_count_ != recycled_values_.size())
            { values.clear(); values.swap(recycled_values_[recycled_count_++]); }
        }
        status failure_{status::ok};
        ::std::uint64_t pending_tail_continuation_{}, pending_tail_parent_{};
        void fail(status result) noexcept { if(failure_ == status::ok) { failure_ = result; } }
        static void retain_numeric_packet(logical_frame& frame, ::std::uint64_t site,
            ::std::span<::std::byte const> payload) noexcept
        {
            // The caller proved the entire extent, sealed numeric declaration,
            // actual flags and SAME activation before this nonthrowing commit.
            if(!payload.empty()) { ::std::memcpy(frame.numeric_packet.data(), payload.data(), payload.size()); }
            // A pending packet has already emptied typed values and marked
            // this frame materialized. Cold expansion clears pending only
            // after constructing all values; reference/full commits and leave
            // also clear it. Reuse only that private representation invariant,
            // never an earlier packet's source/extent/flag validation.
            if(!frame.numeric_packet_pending)
            {
                frame.values.clear();
                frame.materialized = true; frame.numeric_packet_pending = true;
            }
            frame.site = site;
        }
        void expand_numeric_packets()
        {
            for(auto& frame : live_frames())
            {
                if(!frame.numeric_packet_pending) { continue; }
                // Only retain_numeric_packet creates this private pending state:
                // its site/extent/all-initialized/no-reference proofs precede
                // the owned copy. The retained immutable plan cannot change.
                auto const& site{frame.plan->get().sites[static_cast<::std::size_t>(frame.site - 1u)]};
                frame.values.reserve(site.slots.size()); // allocation before any mutation
                for(::std::size_t i{}; i != site.slots.size(); ++i)
                {
                    native_value value{site.slots[i], {}};
                    ::std::memcpy(value.bits.data(), frame.numeric_packet.data() + i * native_slot_bytes, native_slot_bytes);
                    frame.values.push_back(value); // reserve proves no allocation
                }
                frame.numeric_packet_pending = false;
            }
        }
        // The sealed site proved every local requires flag 1. Small exact
        // ranges need no out-of-line libc comparator. memcpy loads tolerate
        // unaligned flags and read ONLY the proved extent, including 0/1-byte
        // boundaries. Repeated 0x01 bytes are independent of native endianness.
        [[nodiscard]] static bool all_required_flags_initialized(
            ::std::span<::std::uint8_t const> flags, ::std::span<::std::uint8_t const> expected) noexcept
        {
            if(flags.size() > 8u)
            { return ::std::memcmp(flags.data(), expected.data(), flags.size()) == 0; }
            auto const* cursor{flags.data()}; auto count{flags.size()};
            if(count == 8u)
            {
                ::std::uint64_t word{}; ::std::memcpy(::std::addressof(word), cursor, 8u);
                return word == UINT64_C(0x0101010101010101);
            }
            if(count >= 4u)
            {
                ::std::uint32_t word{}; ::std::memcpy(::std::addressof(word), cursor, 4u);
                if(word != UINT32_C(0x01010101)) { return false; }
                cursor += 4u; count -= 4u;
            }
            if(count >= 2u)
            {
                ::std::uint16_t word{}; ::std::memcpy(::std::addressof(word), cursor, 2u);
                if(word != 0x0101u) { return false; }
                cursor += 2u; count -= 2u;
            }
            return count == 0u || *cursor == 1u;
        }
        [[nodiscard]] UWVM_ALWAYS_INLINE status observer_site(::std::size_t index, ::std::uint64_t& site_id) const noexcept
        {
            site_id = 0u;
            auto const storage{observer_storage_[index]};
            if(storage.empty()) { return status::ok; }
            ::std::memcpy(::std::addressof(site_id), storage.data(), observer_site_bytes);
            if(site_id == 0u) { return status::ok; }
            auto const& plan{frames_[index].plan};
            if(site_id > plan->get().sites.size()) { return status::invalid_layout; }
            auto const ordinal{static_cast<::std::size_t>(site_id-1u)};
            auto const& site{plan->get().sites[ordinal]};
            auto const workspace{storage.subspan(observer_site_bytes)};
            if(!observer_workspace_fits(site.local_count,site.slots.size()) ||
               site.local_count > workspace.size()/observer_local_metadata_bytes)
            { return status::invalid_layout; }
            auto const prefix{site.local_count*observer_local_metadata_bytes};
            if(site.slots.size() > (workspace.size()-prefix)/native_slot_bytes) { return status::invalid_layout; }
            // Decode ownership and dynamic initialization only on a genuine
            // cold read/census. Validate every reference before any visitor;
            // the marker alone never authenticates a carrier or GC object.
            return validate_compiler_packet_impl(plan,site_id,
                workspace.subspan(prefix,site.slots.size()*native_slot_bytes),
                {reinterpret_cast<::std::uint8_t const*>(workspace.data()+site.local_count),site.local_count});
        }
        void expand_observer_slots()
        {
            for(::std::size_t index{}; index != active_frame_count_; ++index)
            {
                ::std::uint64_t site_id{};
                auto const checked{observer_site(index,site_id)};
                if(checked != status::ok) { fail(checked); return; }
                if(site_id == 0u) { continue; }
                auto& frame{frames_[index]};
                auto const& site{frame.plan->get().sites[static_cast<::std::size_t>(site_id-1u)]};
                auto const workspace{observer_storage_[index].subspan(observer_site_bytes)};
                auto const payload{workspace.subspan(site.local_count*observer_local_metadata_bytes,
                    site.slots.size()*native_slot_bytes)};
                auto const flags{::std::span<::std::uint8_t const>{
                    reinterpret_cast<::std::uint8_t const*>(workspace.data()+site.local_count),site.local_count}};
                auto const decoded{decode_compiler_packet_impl(frame.plan,site_id,payload,flags,frame.values,false)};
                if(decoded != status::ok) { fail(decoded); return; }
                frame.site=site_id; frame.materialized=true; frame.numeric_packet_pending=false;
            }
        }
        [[nodiscard]] static bool runtime_reference(native_reference reference) noexcept
        {
            using kind = ::uwvm2::object::global::wasm_ref_kind;
            switch(reference.kind)
            {
                case kind::wasm_null: case kind::wasm_i31: case kind::wasm_struct:
                case kind::wasm_array: case kind::wasm_exn: case kind::wasm_extern:
                case kind::wasm_func_imported: case kind::wasm_func_defined: return true;
                case kind::wasm_func: return false; // Parser function index is not a live reference.
            }
            return false;
        }
        [[nodiscard]] static bool typed_reference_envelope(native_reference ref, types::core_value_type type) noexcept
        {
            using kind = ::uwvm2::object::global::wasm_ref_kind;
            bool known{};
            switch(ref.kind)
            {
                case kind::wasm_null: case kind::wasm_i31: case kind::wasm_struct:
                case kind::wasm_array: case kind::wasm_exn: case kind::wasm_extern:
                case kind::wasm_func_imported: case kind::wasm_func_defined: known = true; break;
                case kind::wasm_func: return false;
            }
            if(!known) { return false; }
            if(ref.kind == kind::wasm_null) { return type.nullable; }
            // Only a conservative carrier envelope. A defined heap type still
            // requires actual module subtype/layout/store-owner verification.
            // wasm_extern also needs real host-vs-wrapper registry inspection;
            // no opaque address or copied tag can supply that proof here.
            if(type.heap.is_defined())
            {
                return ref.kind == kind::wasm_struct || ref.kind == kind::wasm_array ||
                       ref.kind == kind::wasm_func_imported || ref.kind == kind::wasm_func_defined;
            }
            switch(static_cast<types::abstract_heap_type>(type.heap.code))
            {
                case types::abstract_heap_type::any:
                    return ref.kind == kind::wasm_struct || ref.kind == kind::wasm_array ||
                           ref.kind == kind::wasm_i31 || ref.kind == kind::wasm_extern;
                case types::abstract_heap_type::eq:
                    return ref.kind == kind::wasm_struct || ref.kind == kind::wasm_array || ref.kind == kind::wasm_i31;
                case types::abstract_heap_type::i31: return ref.kind == kind::wasm_i31;
                case types::abstract_heap_type::struct_: return ref.kind == kind::wasm_struct;
                case types::abstract_heap_type::array: return ref.kind == kind::wasm_array;
                case types::abstract_heap_type::func:
                    return ref.kind == kind::wasm_func_imported || ref.kind == kind::wasm_func_defined;
                case types::abstract_heap_type::extern_: return ref.kind == kind::wasm_extern;
                case types::abstract_heap_type::exn: return ref.kind == kind::wasm_exn;
                case types::abstract_heap_type::none: case types::abstract_heap_type::nofunc:
                case types::abstract_heap_type::noextern: case types::abstract_heap_type::noexn: return false;
            }
            return false;
        }
    private:
        // `readable` in a site is a validation proof. A false nondefaultable
        // local may nevertheless contain an actual assigned value after a
        // conservative control merge. Canonical compiler-executed flags decide
        // whether to copy that value, without permitting an illegal local.get.
        // All flags are checked before reading ANY native payload slot.
        [[nodiscard]] UWVM_ALWAYS_INLINE static status validate_compiler_packet_impl(sealed_function_plan::owner const& plan, ::std::uint64_t site,
            ::std::span<::std::byte const> payload, ::std::span<::std::uint8_t const> actual_local_flags) noexcept
        {
            if(!plan || site == 0u || site > plan->get().sites.size()) { return status::invalid_plan; }
            auto const& declaration{plan->get().sites[static_cast<::std::size_t>(site - 1u)]};
            if(declaration.slots.size() > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / native_slot_bytes ||
               payload.size() != declaration.slots.size() * native_slot_bytes || actual_local_flags.size() != declaration.local_count)
            { return status::invalid_layout; }
            auto const& shape{plan->packet_shape(static_cast<::std::size_t>(site-1u))};
            if(shape.all_locals_required)
            {
                // Sealing proved that every local requires flag 1. Compare
                // ALL executed flags, including invalid tags, in one byte run.
                // Numeric payload bits remain unrestricted and fully copied.
                if(!all_required_flags_initialized(actual_local_flags, plan->initialized_flags()))
                { return status::invalid_layout; }
            }
            else for(::std::size_t i{}; i != actual_local_flags.size(); ++i)
            {
                // [actual compiler-owned bounded original-index flag bytes] end
                // [safe                                                  ] i<N
                // and complete flags.size==local_count precede the flag read.
                if(actual_local_flags[i] > 1u || (declaration.slots[i].initialized && actual_local_flags[i] == 0u))
                { return status::invalid_layout; }
            }
            // Validate reference payloads before changing the previous owned
            // values. Numeric bit patterns are unrestricted (including NaNs),
            // so they need one full copy below and no temporary value here.
            // No callback or guest runs between validation and the copy.
            for(auto const& reference_slot : shape.reference_slots)
            {
                auto const i{reference_slot.index};
                auto const& slot{declaration.slots[i]};
                bool const initialized{i < declaration.local_count ? actual_local_flags[i] != 0u : slot.initialized};
                // Complete equal extent and ptrdiff bounds precede subspan.
                auto const bytes{payload.subspan(i * native_slot_bytes, native_slot_bytes)};
                if(!initialized)
                {
                    if(i >= declaration.local_count || slot.type.kind != types::value_kind::reference || slot.type.nullable)
                    { return status::invalid_layout; }
                    for(auto byte : bytes) { if(byte != ::std::byte{}) { return status::invalid_layout; } }
                }
                else
                {
                    native_reference ref{};
                    static_assert(sizeof(ref) <= native_slot_bytes);
                    // Copy a bounded carrier into a live object; no pointed-to
                    // GC or host object is read through these opaque bits.
                    ::std::memcpy(::std::addressof(ref), bytes.data(), sizeof(ref));
                    // The sealed shape supplies only fixed type decoding.
                    // Read and check THIS carrier every time; bound the shift
                    // before using an unknown tag. Parser indices never match.
                    auto const tag{static_cast<unsigned>(ref.kind)};
                    if(tag >= ::std::numeric_limits<unsigned>::digits ||
                       (reference_slot.accepted_kinds & (1u << tag)) == 0u)
                    { return status::invalid_reference; }
                }
            }
            return status::ok;
        }
        [[nodiscard]] static status decode_compiler_packet_impl(sealed_function_plan::owner const& plan, ::std::uint64_t site,
            ::std::span<::std::byte const> payload, ::std::span<::std::uint8_t const> actual_local_flags,
            ::std::vector<native_value>& values, bool retain_local_types)
        {
            auto const checked{validate_compiler_packet_impl(plan,site,payload,actual_local_flags)};
            if(checked != status::ok) { return checked; }
            auto const& declaration{plan->get().sites[static_cast<::std::size_t>(site-1u)]};
            // Growth is the only allocation and precedes every mutation. These
            // plain value cells have nonthrowing construction/assignment, so a
            // failed allocation or rejected packet leaves the old frame intact.
            static_assert(::std::is_nothrow_default_constructible_v<native_value> &&
                ::std::is_nothrow_copy_constructible_v<native_value> && ::std::is_nothrow_copy_assignable_v<native_value>);
            values.reserve(declaration.slots.size());
            if(values.size() > declaration.slots.size()) { values.resize(declaration.slots.size()); }
            auto const previous_count{values.size()};
            for(::std::size_t i{}; i != previous_count; ++i)
            {
                auto& value{values[i]};
                // Only an already materialized SAME activation may reuse its
                // local types. Sealing compared every site's local prefix.
                // Dynamic initialization and all 16 value bytes still refresh.
                if(!retain_local_types || i >= declaration.local_count)
                { value.declaration=declaration.slots[i]; }
                if(i < declaration.local_count) { value.declaration.initialized=actual_local_flags[i]!=0u; }
                ::std::memcpy(value.bits.data(),payload.data()+i*native_slot_bytes,native_slot_bytes);
            }
            for(::std::size_t i{previous_count}; i != declaration.slots.size(); ++i)
            {
                // Append an already complete cell. Growing with resize()
                // first clears its declaration and bits, then overwrites
                // both. Keep this separate from the existing-cell loop so
                // stable local prefixes incur no per-cell growth branch.
                native_value added{declaration.slots[i], {}};
                if(i < declaration.local_count) { added.declaration.initialized=actual_local_flags[i]!=0u; }
                ::std::memcpy(added.bits.data(),payload.data()+i*native_slot_bytes,native_slot_bytes);
                values.push_back(added); // reserve above proves no allocation
            }
            return status::ok;
        }
    public:
        [[nodiscard]] static status decode_compiler_packet(sealed_function_plan::owner const& plan, ::std::uint64_t site,
            ::std::span<::std::byte const> payload, ::std::span<::std::uint8_t const> actual_local_flags,
            ::std::vector<native_value>& values)
        { return decode_compiler_packet_impl(plan,site,payload,actual_local_flags,values,false); }
        explicit shadow_ledger(compilation_profile::owner profile) : profile_{::std::move(profile)}
        { if(!profile_) { failure_ = status::invalid_profile; } }
        shadow_ledger(shadow_ledger const&) = delete;
        shadow_ledger& operator=(shadow_ledger const&) = delete;
        [[nodiscard]] status failure() const noexcept { return failure_; }
        // Native runtime reports a failed actual owner/ABI/allocation check;
        // poisoning closes capture and can never mint pause/restore authority.
        void poison(status reason) noexcept { fail(reason == status::ok ? status::invalid_activation : reason); }
        [[nodiscard]] ::std::size_t size() const noexcept { return active_frame_count_; }
        // Owned recording DATA only. A failed producer has stopped updating
        // its history, so stale copies can outlive real generated leave hooks.
        // The terminal shutdown caller must FIRST authenticate its actual
        // private cancellation and independently prove native/GC frame cleanup.
        // Keep failure sticky: discarding history grants no capture, GC census,
        // execution drain, join or restore capability. Healthy recording stays
        // untouched and must still retire through its exact generated hooks.
        [[nodiscard]] bool discard_failed_recording() noexcept
        {
            if(failure_ == status::ok) { return false; }
            frames_.clear(); observer_storage_.clear(); compiler_metadata_reuse_.clear(); retained_metadata_count_=0u; active_frame_count_=0u; pending_tail_continuation_ = pending_tail_parent_ = 0u;
            return true;
        }
        // Borrow only while the actual thread is parked. A span itself is not
        // a pause ticket, native-root census or registry publication lease.
        [[nodiscard]] ::std::span<logical_frame const> frames_while_actually_stopped() noexcept
        {
            // Cold owning-thread access only. Expanding owned numeric DATA does
            // not authenticate a stop, census a root or grant resume permission.
            // Do not expose a partially expanded view if allocation fails.
# ifdef UWVM_CPP_EXCEPTIONS
            try { if(failure_ == status::ok) { expand_observer_slots(); } expand_numeric_packets(); }
            catch(...) { fail(status::quota_exceeded); return {}; }
# else
            if(failure_ == status::ok) { expand_observer_slots(); } expand_numeric_packets();
# endif
            return live_frames();
        }
        // Synchronous native producer only, on this ledger's owning thread.
        // The entry hook already authenticated and retained this immutable
        // plan. The caller independently authenticates the LIVE activation;
        // this recording-data borrow is never a capture/restore permission.
        // No enter/leave/publication may run while this reference is borrowed.
        [[nodiscard]] sealed_function_plan::owner const* compiler_top_plan(activation_identity actual) const noexcept
        {
            if(failure_ != status::ok || active_frames_empty() || active_frame().identity != actual ||
               pending_tail_continuation_ != 0u) { return nullptr; }
            return ::std::addressof(active_frame().plan);
        }
        // Runtime binds only its actual current LIFO allocation after entry
        // authenticated the plan and activation. DATA alone grants no capture,
        // GC root or resume permission. Storage stays stable until unbound.
        [[nodiscard]] status bind_observer_storage(activation_identity actual, ::std::span<::std::byte> storage) noexcept
        {
            if(failure_ != status::ok) { return failure_; }
            if(active_frames_empty() || active_frame().identity != actual ||
               profile_->purpose() != compilation_purpose::observe_values ||
               !observer_storage_[active_frame_count_-1u].empty())
            { fail(status::invalid_activation); return failure_; }
            auto const locals{active_frame().plan->get().sites.front().local_count};
            if(storage.size() < observer_site_bytes ||
               locals > (storage.size()-observer_site_bytes)/(observer_local_metadata_bytes+native_slot_bytes))
            { fail(status::invalid_layout); return failure_; }
            ::std::uint64_t site{}; ::std::memcpy(::std::addressof(site),storage.data(),observer_site_bytes);
            if(site != 0u) { fail(status::invalid_layout); return failure_; }
            observer_storage_[active_frame_count_-1u]=storage; return status::ok;
        }
        void unbind_observer_storage(::std::byte const* actual_storage) noexcept
        {
            if(failure_ != status::ok)
            {
                // Poisoned recording may have stopped following actual depth.
                // Drop every borrow before native storage can retire; never read it.
                for(auto& storage : observer_storage_) { storage={}; }
                return;
            }
            if(active_frames_empty() || observer_storage_[active_frame_count_-1u].data() != actual_storage)
            { fail(status::invalid_activation); for(auto& storage : observer_storage_) { storage={}; } return; }
            observer_storage_[active_frame_count_-1u]={};
        }
        // A genuine safe-point bridge can keep this exact typed packet in
        // its bound slots. Inspectors still authenticate their actual stop;
        // no borrowed native range is returned or placed in a capture.
        [[nodiscard]] bool compiler_observer_packet(activation_identity actual, ::std::uint64_t site,
            ::std::span<::std::byte const> payload, ::std::span<::std::uint8_t const> flags) const noexcept
        {
            if(failure_ != status::ok || active_frames_empty() || active_frame().identity != actual) { return false; }
            auto const index{active_frame_count_-1u}; ::std::uint64_t current{};
            if(observer_site(index,current) != status::ok || current == 0u || current != site) { return false; }
            auto const& layout{active_frame().plan->get().sites[static_cast<::std::size_t>(site-1u)]};
            auto const workspace{observer_storage_[index].subspan(observer_site_bytes)};
            return payload.data() == workspace.data()+layout.local_count*observer_local_metadata_bytes &&
                payload.size() == layout.slots.size()*native_slot_bytes &&
                flags.data() == reinterpret_cast<::std::uint8_t const*>(workspace.data()+layout.local_count) &&
                flags.size() == layout.local_count;
        }
    private:
        template<typename Plan>
        [[nodiscard]] UWVM_ALWAYS_INLINE status enter_impl(activation_identity actual, Plan&& plan, bool compiler_reuse)
        {
            if(failure_ != status::ok) { return failure_; }
            if(!plan || plan->get().profile != profile_ ||
               plan->get().profile.owner_before(profile_) || profile_.owner_before(plan->get().profile))
            { fail(status::invalid_plan); return failure_; }
            if(plan->get().producer_availability != status::ok)
            { fail(plan->get().producer_availability); return failure_; }
            if(actual.incarnation == 0u || actual.continuation == 0u || actual.runtime_epoch == 0u ||
               actual.parent != (active_frames_empty() ? 0u : active_frame().identity.incarnation) ||
               (!active_frames_empty() && actual.runtime_epoch != active_frame().identity.runtime_epoch))
            { fail(status::invalid_activation); return failure_; }
            if(profile_->limits().frames != 0u && active_frame_count_ >= profile_->limits().frames) { fail(status::quota_exceeded); return failure_; }
            if(pending_tail_continuation_ != 0u)
            {
                if(actual.continuation != pending_tail_continuation_ || actual.parent != pending_tail_parent_)
                { fail(status::invalid_activation); return failure_; }
                pending_tail_continuation_ = pending_tail_parent_ = 0u;
            }
            else if(!active_frames_empty())
            {
                ::std::uint64_t site{}; auto const checked{observer_site(active_frame_count_-1u,site)};
                if(checked != status::ok) { fail(checked); return failure_; }
                if(site == 0u) { site=active_frame().materialized ? active_frame().site : 0u; }
                if(site == 0u || active_frame().plan->get().sites[static_cast<::std::size_t>(site-1u)].phase != frame_phase::awaiting_call_return)
                { fail(status::unmaterialized); return failure_; }
            }
            // Grow only for a new maximum depth. Every admission above is
            // still required, including the complete current awaiting parent.
            // Retained metadata alone cannot admit a new activation.
            if(active_frame_count_ == frames_.size()) { frames_.emplace_back(); }
            if(active_frame_count_ == observer_storage_.size()) { observer_storage_.emplace_back(); }
            if(active_frame_count_ == compiler_metadata_reuse_.size()) { compiler_metadata_reuse_.push_back(0u); }
            auto& slot{frames_[active_frame_count_]};
            if(slot.plan) { --retained_metadata_count_; }
            compiler_metadata_reuse_[active_frame_count_] = compiler_reuse &&
                profile_->purpose() == compilation_purpose::observe_values && plan->observer_retention_bytes() != 0u;
            ++active_frame_count_;
            active_frame().identity = actual;
            if(!compiler_reuse || slot.plan.get() != plan.get() || slot.plan.owner_before(plan) || plan.owner_before(slot.plan))
            { slot.plan = ::std::forward<Plan>(plan); }
            if(recycled_count_ != 0u) { active_frame().values.swap(recycled_values_[--recycled_count_]); }
            // The new frame is empty/unmaterialized even when its storage was
            // used by another function with different local/reference types.
            return status::ok;
        }
    public:
        [[nodiscard]] status enter(activation_identity actual, sealed_function_plan::owner plan)
        { return enter_impl(actual,::std::move(plan),false); }
        // Synchronous native producer DATA only. The runtime independently
        // authenticates the actual publication, entries, epoch, generation and
        // shared control block before borrowing. No call/park may span this use.
        [[nodiscard]] UWVM_ALWAYS_INLINE status compiler_enter_observer(activation_identity actual, sealed_function_plan::owner const& plan)
        {
            if(failure_ != status::ok) { return failure_; }
            if(profile_->purpose() != compilation_purpose::observe_values)
            { fail(status::invalid_profile); return failure_; }
            return enter_impl(actual,plan,true);
        }
        [[nodiscard]] status materialize(activation_identity actual, ::std::uint64_t site_id,
            ::std::span<::std::byte const> actual_native_slots, bool defer_numeric_expansion = false)
        {
            if(failure_ != status::ok) { return failure_; }
            if(active_frames_empty() || active_frame().identity != actual || pending_tail_continuation_ != 0u)
            { fail(status::invalid_activation); return failure_; }
            auto& frame{active_frame()};
            if(site_id == 0u || site_id > frame.plan->get().sites.size()) { fail(status::invalid_layout); return failure_; }
            auto const& site{frame.plan->get().sites[static_cast<::std::size_t>(site_id - 1u)]};
            if(site.slots.size() > (static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / native_slot_bytes) ||
               actual_native_slots.size() != site.slots.size() * native_slot_bytes)
            { fail(status::invalid_layout); return failure_; }
            if(defer_numeric_expansion && profile_->purpose() == compilation_purpose::observe_values &&
               actual_native_slots.size() <= frame.numeric_packet.size() &&
               frame.plan->packet_shape(static_cast<::std::size_t>(site_id-1u)).reference_slots.empty())
            {
                // validate_site permits an uninitialized cell only for a
                // nondefaultable reference local. Thus a sealed numeric-only
                // fixed packet has no flags, references or zero-slot checks to
                // defer. It still copies ALL 16 bytes per actual declared slot.
                retain_numeric_packet(frame, site_id, actual_native_slots); return status::ok;
            }
            // Validate the complete packet BEFORE modifying the previous frame.
            // Allocation follows validation and precedes all mutations, exactly
            // as in the dynamic producer. Avoid a detached allocation at every
            // entry/awaiting-call fixed packet while preserving rejection atomicity.
            for(::std::size_t i{}; i != site.slots.size(); ++i)
            {
                auto const& declaration{site.slots[i]};
                // [actual compiler-owned 16-byte slots ... i ... count] end
                // [safe                                                ] i<count
                // and exact count*16/ptrdiff bounds above precede subspan.
                // The pointer changes only inside that one actual host range.
                auto const bytes{actual_native_slots.subspan(i * native_slot_bytes, native_slot_bytes)};
                if(!declaration.initialized)
                {
                    // The compiler emitted zero stores, never a load from an
                    // uninitialized nondefaultable local. Reject forged bits.
                    for(auto byte : bytes) { if(byte != ::std::byte{}) { fail(status::invalid_layout); return failure_; } }
                }
                else if(declaration.type.kind == types::value_kind::reference)
                {
                    static_assert(sizeof(native_reference) <= native_slot_bytes);
                    native_reference reference{};
                    // [one already bounded native ABI slot] end
                    // [safe                               ] complete real
                    // carrier copied into a constructed C++ object; no
                    // pointed-to GC/host object is read or inferred here.
                    ::std::memcpy(::std::addressof(reference), bytes.data(), sizeof(reference));
                    if(!runtime_reference(reference) || (!declaration.type.nullable &&
                       reference.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_null))
                    { fail(status::invalid_reference); return failure_; }
                }
            }
            static_assert(::std::is_nothrow_default_constructible_v<native_value> &&
                ::std::is_nothrow_copy_constructible_v<native_value> && ::std::is_nothrow_copy_assignable_v<native_value>);
            frame.values.reserve(site.slots.size());
            frame.values.resize(site.slots.size()); // no allocation after reserve
            for(::std::size_t i{}; i != site.slots.size(); ++i)
            {
                auto& value{frame.values[i]}; value.declaration=site.slots[i];
                ::std::memcpy(value.bits.data(),actual_native_slots.data()+i*native_slot_bytes,native_slot_bytes);
            }
            frame.site = site_id; frame.materialized = true; frame.numeric_packet_pending = false; return status::ok;
        }
        // Typed native DATA commit only. The actual runtime bridge authenticates
        // its current activation/publication/compiler packet before this call.
        // A tuple/plan/identity provided by a native client never grants capture,
        // GC-root ownership or executable continuation authority here.
        [[nodiscard]] status materialize_validated_native_data(activation_identity actual,
            sealed_function_plan::owner const& actual_plan, ::std::uint64_t site_id,
            ::std::span<native_value const> values)
        {
            if(failure_ != status::ok) { return failure_; }
            if(active_frames_empty() || active_frame().identity != actual || pending_tail_continuation_ != 0u)
            { fail(status::invalid_activation); return failure_; }
            auto& frame{active_frame()};
            if(!actual_plan || frame.plan.get() != actual_plan.get() || frame.plan.owner_before(actual_plan) ||
               actual_plan.owner_before(frame.plan) || site_id == 0u || site_id > frame.plan->get().sites.size())
            { fail(status::invalid_plan); return failure_; }
            // [actual sealed same-control-block plan sites ... site_id-1 ... N] end
            // [safe] complete ID bound BEFORE metadata indexing; no public
            // shared_ptr or matching raw address is treated as runtime permission.
            auto const& site{frame.plan->get().sites[static_cast<::std::size_t>(site_id - 1u)]};
            if(values.size() != site.slots.size()) { fail(status::invalid_layout); return failure_; }
            for(::std::size_t i{}; i != values.size(); ++i)
            {
                // [validated owned DATA values and exact declarations0..N] end
                // [safe] full equal counts BEFORE selecting either i cell.
                auto const& value{values[i]}; auto const& expected{site.slots[i]};
                if(value.declaration.type != expected.type || (expected.initialized && !value.declaration.initialized))
                { fail(status::invalid_layout); return failure_; }
                if(!value.declaration.initialized)
                {
                    if(i >= site.local_count || expected.type.kind != types::value_kind::reference || expected.type.nullable)
                    { fail(status::invalid_layout); return failure_; }
                    for(auto byte : value.bits)
                    { if(byte != ::std::byte{}) { fail(status::invalid_layout); return failure_; } }
                    continue;
                }
                if(expected.type.kind == types::value_kind::reference)
                {
                    native_reference ref{};
                    static_assert(sizeof(ref) <= native_slot_bytes);
                    // [owned complete native_value::bits[16]] bits_end
                    // [safe] constructed carrier copy only; never dereference
                    // an opaque token, parsed function index or native address.
                    ::std::memcpy(::std::addressof(ref), value.bits.data(), sizeof(ref));
                    if(!typed_reference_envelope(ref, expected.type))
                    { fail(status::invalid_reference); return failure_; }
                }
            }
            // All types/actual initialized states/counts are checked before one
            // detached allocation/copy. Failure leaves the old frame untouched;
            // recording failure is sticky and cannot mint a new stop capability.
            ::std::vector<native_value> detached{values.begin(), values.end()};
            frame.values.swap(detached); frame.site = site_id; frame.materialized = true; frame.numeric_packet_pending = false;
            return status::ok;
        }

        // Full validation precedes mutation of the current activation-owned values.
        [[nodiscard]] status materialize_compiler_native_data(activation_identity actual,
            sealed_function_plan::owner const& actual_plan, ::std::uint64_t site_id,
            ::std::span<::std::byte const> payload, ::std::span<::std::uint8_t const> actual_local_flags,
            bool defer_numeric_expansion = false)
        {
            if(failure_!=status::ok) { return failure_; }
            if(active_frames_empty() || active_frame().identity!=actual || pending_tail_continuation_!=0u)
            { fail(status::invalid_activation);return failure_; }
            auto& frame{active_frame()};
            // A native leaf borrow is a reference to this EXACT shared_ptr object.
            // Its address equality proves the control block as well as pointee;
            // other owners still need the complete anti-alias checks below.
            if(::std::addressof(frame.plan) != ::std::addressof(actual_plan) &&
               (!actual_plan || frame.plan.get()!=actual_plan.get() || frame.plan.owner_before(actual_plan) || actual_plan.owner_before(frame.plan)))
            { fail(status::invalid_plan);return failure_; }
            if(defer_numeric_expansion && profile_->purpose() == compilation_purpose::observe_values &&
               site_id != 0u && site_id <= actual_plan->get().sites.size())
            {
                auto const index{static_cast<::std::size_t>(site_id-1u)};
                auto const& site{actual_plan->get().sites[index]};
                auto const& shape{actual_plan->packet_shape(index)};
                if(site.slots.size() <= frame.numeric_packet.size() / native_slot_bytes &&
                   shape.reference_slots.empty() && shape.all_locals_required)
                {
                    // Exact extents precede ALL flag/payload reads. Compare
                    // every executed flag (including invalid tags); never cache
                    // a previous packet's availability or trust a caller hint.
                    if(payload.size() != site.slots.size() * native_slot_bytes || actual_local_flags.size() != site.local_count ||
                       !all_required_flags_initialized(actual_local_flags, actual_plan->initialized_flags()))
                    { fail(status::invalid_layout); return failure_; }
                    retain_numeric_packet(frame, site_id, payload); return status::ok;
                }
            }
            bool const retain_local_types{frame.materialized && actual_plan->invariant_local_types() &&
                site_id != 0u && site_id <= actual_plan->get().sites.size() &&
                frame.values.size() >= actual_plan->get().sites[static_cast<::std::size_t>(site_id-1u)].local_count};
            auto const decoded{decode_compiler_packet_impl(actual_plan,site_id,payload,actual_local_flags,frame.values,retain_local_types)};
            if(decoded!=status::ok) { fail(decoded);return failure_; }
            frame.site=site_id;frame.materialized=true;frame.numeric_packet_pending=false;return status::ok;
        }

        // Called BEFORE the compiler emits musttail; never inserts work between
        // musttail and its required return. Typed successors retain continuation.
        [[nodiscard]] UWVM_ALWAYS_INLINE status leave(activation_identity actual, bool typed_tail) noexcept
        {
            if(failure_ != status::ok) { return failure_; }
            if(active_frames_empty() || active_frame().identity != actual || pending_tail_continuation_ != 0u)
            { fail(status::invalid_activation); return failure_; }
            if(typed_tail)
            { pending_tail_continuation_ = actual.continuation; pending_tail_parent_ = actual.parent; }
            auto& retired{active_frame()};
            recycle_values(retired.values);
            // Large values or a full capacity cache must still release their
            // allocation immediately, as pop_back did. References, identity,
            // borrowed storage and availability retire before reducing the range.
            if(retired.values.capacity() != 0u) { ::std::vector<native_value>{}.swap(retired.values); }
            // At most 32 inactive plans, each with <=16 KiB of immutable
            // owned metadata payload (allocator overhead excluded). This is
            // scope-local DATA: no code lease, native carrier or authority.
            if(compiler_metadata_reuse_[active_frame_count_-1u] && retained_metadata_count_ < retained_metadata_limit)
            { ++retained_metadata_count_; }
            else { retired.plan.reset(); }
            compiler_metadata_reuse_[active_frame_count_-1u]=0u;
            retired.identity={}; retired.site=0u;
            retired.materialized=false; retired.numeric_packet_pending=false;
            observer_storage_[active_frame_count_-1u]={};
            --active_frame_count_; return status::ok;
        }
        [[nodiscard]] status host_import_admission(bool has_actual_record_replay_adapter) noexcept
        {
            if(failure_ != status::ok) { return failure_; }
            if(!has_actual_record_replay_adapter) { fail(status::non_replayable_import); }
            return failure_;
        }
        [[nodiscard]] bool has_complete_materialized_frames() const noexcept
        {
            if(failure_ != status::ok || active_frames_empty() || pending_tail_continuation_ != 0u) { return false; }
            for(::std::size_t index{}; index != active_frame_count_; ++index)
            {
                ::std::uint64_t site{};
                if(observer_site(index,site) != status::ok || (site == 0u && !frames_[index].materialized)) { return false; }
            }
            return true;
        }
        // A cold typed population visitor, not a root-registration capability.
        // Actual runtime must call it BEFORE any GC can reclaim these recorded
        // carriers, keep the resulting independently owned roots for the entire
        // actual collection ticket, and also census current native/static/EH
        // roots. Entry-only recording currently cannot authorize that census.
        // Never register this heap-backed history as a strict-LIFO frame_root.
        template<typename Visitor>
        [[nodiscard]] status visit_recorded_native_references(Visitor&& visitor) const
        {
            if(failure_ != status::ok) { return failure_; }
            for(::std::size_t index{}; index != active_frame_count_; ++index)
            {
                ::std::uint64_t site{}; auto const checked{observer_site(index,site)};
                if(checked != status::ok) { return checked; }
                auto const& frame{frames_[index]};
                if(site != 0u)
                {
                    // observer_site validated all flags and carriers before
                    // the first callback. Visit the LIVE activation-owned
                    // slots, never a previous cold copy. A visitor must still
                    // authenticate real store/subtype/GC ownership; copying
                    // a conservative carrier envelope grants no root rights.
                    auto const ordinal{static_cast<::std::size_t>(site-1u)};
                    auto const& layout{frame.plan->get().sites[ordinal]};
                    auto const workspace{observer_storage_[index].subspan(observer_site_bytes)};
                    auto const prefix{layout.local_count*observer_local_metadata_bytes};
                    for(auto const& reference_slot : frame.plan->packet_shape(ordinal).reference_slots)
                    {
                        auto const i{reference_slot.index};
                        if(i < layout.local_count && workspace[layout.local_count+i] == ::std::byte{}) { continue; }
                        native_reference reference{};
                        ::std::memcpy(::std::addressof(reference),workspace.data()+prefix+i*native_slot_bytes,sizeof(reference));
                        if(!visitor(reference)) { return status::invalid_reference; }
                    }
                    continue;
                }
                // Pending packets were sealed and checked numeric-only before
                // copying. They contain NO reference carriers to census; do not
                // allocate or expand numeric values in a GC visitation path.
                if(frame.numeric_packet_pending) { continue; }
                if(!frame.materialized) { return status::unmaterialized; }
                for(auto const& value : frame.values)
                {
                    if(!value.declaration.initialized || value.declaration.type.kind != types::value_kind::reference) { continue; }
                    native_reference reference{};
                    static_assert(sizeof(reference) <= native_slot_bytes);
                    // [owned complete native_value::bits[16]] bits_end
                    // [safe                                ] fixed extent >=
                    // sizeof the constructed carrier; no GC object is read.
                    ::std::memcpy(::std::addressof(reference), value.bits.data(), sizeof(reference));
                    if(!runtime_reference(reference) || !visitor(reference)) { return status::invalid_reference; }
                }
            }
            return status::ok;
        }
        [[nodiscard]] status executable_restore_capability() const noexcept
        { return status::unavailable_resume; } // No native-PC jump or fabricated dispatcher.
    };
}
#include <uwvm2/utils/macro/pop_macros.h>
