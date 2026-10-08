/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once

#ifndef UWVM_MODULE
# include <array>
# include <algorithm>
# include <bit>
# include <atomic>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <exception>
# include <limits>
# include <memory>
# include <new>
# include <span>
# include <vector>
# include <type_traits>
# include <uwvm2/object/global/ref.h>
# include <uwvm2/parser/wasm/standard/wasm3/type/recursive_type.h>
# include <uwvm2/runtime/exception/value.h>
#if defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) && UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES == 1
# include <uwvm2/runtime/exception/external_handle.h>
#endif
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
# include <algorithm>
# include <functional>
# include <span>
# include <uwvm2/runtime/exception/roots.h>
#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1
# include <uwvm2/runtime/exception/native_roots.h>
#endif
#endif
# include <uwvm2/validation/standard/wasm3/recursive_type_registry.h>
# include "gc_trace_metadata.h"
#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1
#  include <bit>
#if defined(UWVM_EXPERIMENTAL_COMPACT_COLLECTION_DIRECTORY) && UWVM_EXPERIMENTAL_COMPACT_COLLECTION_DIRECTORY == 1
#  include <algorithm>
#endif
#  include "compact_numeric/impl.h"
#endif
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

namespace uwvm2::runtime::lib
{
    extern "C++" { class runtime_checkpoint_gc_state_borrow; class runtime_checkpoint_world_transaction; }
}

// These are the FIRST declarations seen by the storage/initializer module
// interfaces. Their later definitions are exported, so the first declarations
// must also be exported ([module.interface]). The native restoration friend
// remains attached to the global module by its existing C++ linkage block;
// full_source_instance remains attached to this storage module. Exporting only
// these incomplete names neither defines a constructor nor grants native setup.
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::initializer { extern "C++" { class restoration_context; } }
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::full { class full_source_instance; }

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::storage
{
    namespace gc_type = ::uwvm2::parser::wasm::standard::wasm3::type;
    using gc_reference = ::uwvm2::object::global::wasm_global_ref_t;

    enum class gc_object_status : ::std::uint32_t
    {
        ok, invalid_store, invalid_type, null_reference, invalid_reference, invalid_value, out_of_bounds,
        immutable_field, size_overflow, out_of_memory
    };

    // Exactly one 16-byte Wasm stack slot. The validated instruction/type declaration
    // determines its kind; host imports must not bypass Wasm type validation.
    struct alignas(8) gc_object_value
    {
        ::std::array<::std::byte, 16uz> bits{};

        [[nodiscard]] static constexpr ::std::size_t wasm_value_size(gc_type::value_kind kind) noexcept
        {
            switch(kind)
            {
                case gc_type::value_kind::i32: case gc_type::value_kind::f32: return 4uz;
                case gc_type::value_kind::i64: case gc_type::value_kind::f64: return 8uz;
                case gc_type::value_kind::v128: return 16uz;
                case gc_type::value_kind::reference: return sizeof(gc_reference);
            }
            return 0uz;
        }
        [[nodiscard]] static constexpr ::std::size_t field_storage_size(gc_type::storage_type storage) noexcept
        {
            if(storage.packed == gc_type::packed_kind::i8) { return 1uz; }
            if(storage.packed == gc_type::packed_kind::i16) { return 2uz; }
            return wasm_value_size(storage.value.kind);
        }

        template <class T>
        [[nodiscard]] static inline gc_object_value from(T value) noexcept
        {
            static_assert(::std::is_trivially_copyable_v<T> && sizeof(T) <= 16uz);
            gc_object_value result{};
            ::std::memcpy(result.bits.data(), ::std::addressof(value), sizeof(value));
            return result;
        }
        [[nodiscard]] static inline gc_object_value i32(::std::uint32_t value) noexcept
        { return from(value); }
        [[nodiscard]] static inline gc_object_value i64(::std::uint64_t value) noexcept
        { return from(value); }
        [[nodiscard]] static inline gc_object_value f32(float value) noexcept
        { return from(value); }
        [[nodiscard]] static inline gc_object_value f64(double value) noexcept
        { return from(value); }
        [[nodiscard]] static inline gc_object_value reference(gc_reference value) noexcept
        { return from(value); }
        template <class T> [[nodiscard]] inline T as() const noexcept
        {
            static_assert(::std::is_trivially_copyable_v<T> && sizeof(T) <= 16uz);
            T result{};
            ::std::memcpy(::std::addressof(result), bits.data(), sizeof(result));
            return result;
        }
    };

    static_assert(::std::is_trivially_copyable_v<gc_object_value>);
    static_assert(sizeof(gc_object_value) == 16uz);
    static_assert(sizeof(gc_reference) <= 16uz);

#if defined(__has_feature)
# if __has_feature(address_sanitizer)
#  define UWVM2_GC_NUMERIC_SLAB_ADDRESS_SANITIZER 1
# endif
#endif
#if defined(__SANITIZE_ADDRESS__)
# define UWVM2_GC_NUMERIC_SLAB_ADDRESS_SANITIZER 1
#endif
#if defined(UWVM2_GC_NUMERIC_SLAB_ADDRESS_SANITIZER)
    // Native sanitizer boundary is explicit C/noexcept, not a C++ potentially
    // throwing declaration. These hooks exist only in instrumented candidates.
    extern "C" void gc_numeric_slab_poison(void const volatile*, ::std::size_t) noexcept asm("__asan_poison_memory_region");
    extern "C" void gc_numeric_slab_unpoison(void const volatile*, ::std::size_t) noexcept asm("__asan_unpoison_memory_region");
#endif

    class managed_numeric_entry_page;
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
    class sealed_compact_entry;
#endif
    class gc_object_store;

    // Host-owned, immutable binding. The implementation must prove function
    // storage membership before reading a reference payload and must preserve
    // the final provider's declared type through imported aliases.
    using gc_function_type_match_callback = bool (*)(gc_reference,
        gc_object_store const*, ::std::uint_least32_t) noexcept;

#if defined(UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP) && UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP == 1
#if defined(UWVM2TEST_GC_COLLECTION_LOCAL_MEMBERSHIP_PROBE) && UWVM2TEST_GC_COLLECTION_LOCAL_MEMBERSHIP_PROBE == 1
    // Native correctness-only witness; absent in every performance build.
    extern "C" void gc_collection_local_membership_probe(void const*, ::std::uintptr_t) noexcept
        asm("uwvm2_test_gc_collection_local_membership_probe");
#endif
#endif
#if defined(UWVM2TEST_GC_NUMERIC_SLAB_RESERVED_PROBE)
    // Correctness-only rendezvous after releasing the allocator lock and before
    // constructing an unpublished header. Production candidates emit no hook.
    extern "C" void gc_numeric_slab_reserved_probe(gc_object_store const*) noexcept
        asm("uwvm2_test_gc_numeric_slab_reserved_probe");
#endif

    // The module instance owns this lease list separately from its GC store.
    // A receiving module retains the originating store while it can hold an
    // imported wrapped reference. Separate ownership prevents A<->B store
    // cycles when two modules exchange references and unload independently.
    class gc_lease_owner
    {
        struct lease_node
        {
            ::std::shared_ptr<gc_object_store const> store{};
            lease_node* next{};
        };
        ::std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
        lease_node* leases_{};

    public:
        gc_lease_owner() = default;
        gc_lease_owner(gc_lease_owner const&) = delete;
        gc_lease_owner& operator=(gc_lease_owner const&) = delete;
        [[nodiscard]] inline bool hold(::std::shared_ptr<gc_object_store const> source) noexcept
        {
            if(!source) { return false; }
            while(lock_.test_and_set(::std::memory_order_acquire)) {}
            for(auto* curr{leases_}; curr != nullptr; curr = curr->next)
            {
                // [live lease chain] a module reset cannot destroy this owner
                // while the caller holds its shared_ptr.
                // ^^ curr advances only to an initialized next link under lock_.
                if(curr->store.get() == source.get())
                { lock_.clear(::std::memory_order_release); return true; }
            }
            auto* fresh{new(::std::nothrow) lease_node{::std::move(source), leases_}};
            if(fresh == nullptr)
            { lock_.clear(::std::memory_order_release); return false; }
            // [fresh initialized][old leases_ chain]
            // ^^ publish one owning lease before releasing lock_.
            leases_ = fresh;
            lock_.clear(::std::memory_order_release);
            return true;
        }
        // Exclusive collector only. All users of this lease owner are stopped,
        // every candidate target has an independent canonical cohort pin, and
        // the predicate reads already validated typed fields without throwing.
        // Detach under the lease lock; destroy outside it so recursive native
        // owner release cannot reenter a held lock.
        template<class Keep>
        inline void prune_exclusive(Keep&& keep) noexcept
        {
            static_assert(::std::is_nothrow_invocable_r_v<bool, Keep&, gc_object_store const*>);
            lease_node* retired{};
            {
                while(lock_.test_and_set(::std::memory_order_acquire)) {}
                auto** link{::std::addressof(leases_)};
                while(*link != nullptr)
                {
                    // [owned lease chain] all nodes stay live under lock_.
                    // ^^ current is the live node in this native pointer slot.
                    auto* current{*link};
                    if(keep(current->store.get()))
                    {
                        // [retained owned lease] its next slot remains live.
                        // ^^ advance the pointer-to-pointer to a proved native slot.
                        link = ::std::addressof(current->next);
                    }
                    else
                    {
                        // [link][current][successor] successor is initialized.
                        // ^^ splice before putting current on the private retired chain.
                        *link = current->next;
                        current->next = retired;
                        retired = current;
                    }
                }
                lock_.clear(::std::memory_order_release);
            }
            while(retired != nullptr)
            {
                // [detached owning lease chain] no lease lock is held here.
                // ^^ save the initialized successor before releasing its owner.
                auto* next{retired->next};
                delete retired;
                // [remaining private chain] next was read before deletion.
                // ^^ advance only to another owned node or null.
                retired = next;
            }
        }
        ~gc_lease_owner()
        {
            // The module instance has stopped; no caller can acquire a new
            // lease-owner shared_ptr once its last module owner is released.
            auto* curr{leases_};
            while(curr != nullptr)
            {
                auto* next{curr->next};
                delete curr;
                // [remaining live lease chain] next was read before deletion.
                // ^^ curr advances only to an owned node or null.
                curr = next;
            }
        }
    };

    class checkpoint_gc_staging;

    class gc_object_store : public ::std::enable_shared_from_this<gc_object_store>
    {
        friend class ::uwvm2::runtime::lib::runtime_checkpoint_gc_state_borrow;
        friend class checkpoint_gc_staging;
        friend class ::uwvm2::uwvm::runtime::initializer::restoration_context;
        friend class managed_numeric_entry_page;
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
        friend class sealed_compact_entry;
#endif
        struct type_layout
        {
            gc_type::composite_kind kind{};
            ::std::uint_least32_t parent{(::std::numeric_limits<::std::uint_least32_t>::max)()};
            ::std::unique_ptr<gc_type::field_type[]> fields{};
            ::std::size_t field_count{};
            // Immutable classification of copied fields; no guest object address is cached.
            bool has_reference_fields{};
            bool has_packed_fields{};
            bool has_nonnullable_reference_fields{};
#if defined(UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA == 1
            gc_trace_metadata trace{};
#endif
        };
        // A single native pointer with the same representation as the old
        // unique_ptr field. The enclosing object owns the complete allocation;
        // this view never separately deletes the initialized tail array.
        struct object_values_view
        {
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
            // Native allocation view only. The immutable enclosing header says
            // whether the tail contains real carriers or original byte-array
            // elements. A byte tail is never exposed as a carrier array.
            void* pointer{};
            [[nodiscard]] constexpr gc_object_value* get() const noexcept
            { return static_cast<gc_object_value*>(pointer); }
            [[nodiscard]] constexpr ::std::byte* byte_data() const noexcept
            { return static_cast<::std::byte*>(pointer); }
#else
            gc_object_value* pointer{};
            [[nodiscard]] constexpr gc_object_value* get() const noexcept
            { return pointer; }
#endif
            [[nodiscard]] constexpr explicit operator bool() const noexcept
            { return pointer != nullptr; }
            [[nodiscard]] constexpr gc_object_value& operator[](::std::size_t index) const noexcept
            {
                // [pointer, pointer + object.length) is an initialized array.
                // Callers prove index < length before forming this element.
                // ^^ pointer + index stays within that checked owned range.
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
                return get()[index];
#else
                return pointer[index];
#endif
            }
            constexpr void reset(gc_object_value* initialized_array) noexcept
            {
                // [unpublished object][complete initialized tail array]
                // ^^ bind once before publication; this view owns no other block.
                pointer = initialized_array;
            }
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
            constexpr void reset_bytes(::std::byte* original_array_tail) noexcept
            {
                // [unpublished numeric array][complete original byte tail]
                // [safe ] retain that real byte-array element pointer; no
                // gc_object_value lifetime is fabricated in this allocation.
                pointer = original_array_tail;
            }
#endif
        };
        static_assert(sizeof(object_values_view) == sizeof(::std::unique_ptr<gc_object_value[]>));
        static_assert(alignof(object_values_view) == alignof(::std::unique_ptr<gc_object_value[]>));
        static_assert(::std::is_trivially_destructible_v<gc_object_value>);
        static_assert(::std::is_nothrow_default_constructible_v<gc_object_value>);

        // A named live envelope contains an inactive/active object union at
        // offset zero and two cold metadata words after it. It is backed by a
        // real std::byte[] whose original array-new pointer is retained. No
        // native header-derived byte pointer is advanced or moved backwards.
        struct slab_chunk;
        struct allocation_header;
        enum class slab_slot_state : ::std::uint8_t { free, reserved, allocated };
        struct allocation_metadata
        {
            // Pooled: exact live chunk. Fallback: original array-new pointer.
            // The active generation distinguishes the two; a free slot always
            // belongs to a chunk and is never decoded as fallback metadata.
            void* owner_or_storage{};
            // Free: next integer slot index. Active: generation/index/state.
            // There are no pointer-to-integer free-list round trips.
            ::std::uintptr_t link_state{static_cast<::std::uintptr_t>(slab_slot_state::reserved)};
        };
#if defined(UWVM_EXPERIMENTAL_GENERAL_GC_SLAB) && UWVM_EXPERIMENTAL_GENERAL_GC_SLAB == 1
        inline static constexpr ::std::size_t numeric_slab_classes{10uz};
#else
        inline static constexpr ::std::size_t numeric_slab_classes{8uz};
#endif
        inline static constexpr ::std::size_t numeric_slab_slots{256uz};
        inline static constexpr ::std::size_t slab_no_slot{numeric_slab_slots};
        inline static constexpr ::std::uintptr_t slab_state_mask{3u};
        inline static constexpr unsigned slab_slot_index_bits{8u};
        inline static constexpr unsigned slab_generation_shift{2u + slab_slot_index_bits};
        inline static constexpr ::std::uintptr_t slab_slot_index_mask{255u};
        inline static constexpr ::std::uint64_t slab_generation_limit{
            static_cast<::std::uint64_t>((::std::numeric_limits<::std::uintptr_t>::max)() >> slab_generation_shift)};
        static_assert(sizeof(allocation_metadata) == sizeof(void*) * 2uz);
        static_assert((1uz << slab_slot_index_bits) == numeric_slab_slots);

        struct object
        {
            gc_object_store const* owner{};
            // Guest-visible identity is an opaque process-unique token. The
            // allocation address must never be reused as a Wasm reference.
            void* token{};
            object* next{};
            object* hash_next{};
            ::std::atomic_flag mutation_lock = ATOMIC_FLAG_INIT;
            // Written only while every reader/mutator is stopped. This byte
            // occupies existing alignment padding; it adds no hot-path access.
            bool marked{};
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
            // Immutable after publication; lives in existing header padding.
            // Only the checked native array allocator may select a byte tail.
            bool numeric_array_{};
#endif
            object_values_view values{};
            ::std::size_t length{};
            ::std::uint_least32_t type_index{};
            gc_type::composite_kind kind{};
            // Cold foreign-object metadata follows every local get/set hot field.
            object* global_next{};
            // Retain arenas referenced by this object's fields. A receiving
            // module may unload before an exported object holding its values.
            gc_lease_owner value_leases{};

            ~object() noexcept
            {
                // [initialized tail array, initialized tail array + length)
                // belongs to this unpublished/live native header. A partial
                // placement-array failure leaves values null; no element is read.
                // End all carrier lifetimes before a pooled body is poisoned or
                // reused, even though their destructors generate no instructions.
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
                // Numeric tails are live original std::byte[] elements, not
                // placement-created carriers. Their array owner ends them.
                if(!numeric_array_ && values) { ::std::destroy_n(values.get(), length); }
#else
                if(values) { ::std::destroy_n(values.get(), length); }
#endif
            }

            // This deallocation function is called directly by destroy_object,
            // with the still-live envelope, after ending this member lifetime.
            // No ordinary delete-expression targets this union member: see
            // [expr.delete]/2. The original byte[] is deleted only by its own
            // saved array-new pointer; a guest token is never accepted here.
            static inline void operator delete(void* live_allocator_envelope) noexcept
            {
                // [live envelope + metadata][ended header/carrier lifetimes]
                // ^^ this value came from allocation_header*, not from a
                // byte-pointer reconstruction of a destroyed object.
                gc_object_store::retire_object_allocation(
                    static_cast<allocation_header*>(live_allocator_envelope));
            }
        };

        // The named union lives for the whole slot lifetime; it has no active
        // object member while the slot is free/reserved. Only placement new
        // activates that member. Its empty destructor never double-destroys an
        // object whose lifetime destroy_object has already ended.
        union object_slot
        {
            object instance;
            object_slot() noexcept {}
            ~object_slot() noexcept {}
        };
        struct allocation_header
        {
            object_slot payload;
            allocation_metadata metadata{};
        };
        static_assert(::std::is_standard_layout_v<object>);
        static_assert(::std::is_standard_layout_v<object_slot>);
        static_assert(::std::is_standard_layout_v<allocation_header>);
        static_assert(offsetof(allocation_header, payload) == 0uz);
        static_assert(sizeof(object_slot) == sizeof(object));
        static_assert(alignof(object_slot) == alignof(object));
        // object <-> its union <-> the standard-layout envelope's first member
        // is the transitive pointer-interconvertible chain [basic.compound]/7.
        // Unlike a preceding prefix, it requires no backwards byte arithmetic.

        // Only 1..8-field numeric structs enter these size classes. Every other
        // form uses one explicit byte-array backing. Full 16-byte carriers and
        // every old hot object offset remain unchanged. No TLS owner cache or
        // native free-list pointer can retain an unloaded store.
        inline static constexpr ::std::size_t allocation_alignment{__STDCPP_DEFAULT_NEW_ALIGNMENT__};
        inline static constexpr ::std::size_t object_value_offset{
            (sizeof(allocation_header) + alignof(gc_object_value) - 1uz) / alignof(gc_object_value) * alignof(gc_object_value)};
        static_assert(alignof(allocation_metadata) <= allocation_alignment);
        static_assert(alignof(allocation_header) <= allocation_alignment);
        static_assert(alignof(gc_object_value) <= allocation_alignment);
        static_assert(object_value_offset >= sizeof(allocation_header));
        static_assert(numeric_slab_classes <=
            ((::std::numeric_limits<::std::size_t>::max)() - object_value_offset
             - (allocation_alignment - 1uz)) / sizeof(gc_object_value));

        struct slab_chunk
        {
            gc_object_store* owner{};
            slab_chunk* all_previous{};
            slab_chunk* all_next{};
            slab_chunk* available_previous{};
            slab_chunk* available_next{};
            ::std::size_t free_head{slab_no_slot};
            // This is the ORIGINAL result of array new, saved before any
            // placement object begins. It remains the sole pointer for byte
            // arithmetic and the operand of the final matching delete[].
            ::std::byte* backing{};
            ::std::size_t stride{};
            ::std::size_t bytes{};
            ::std::size_t class_index{};
            ::std::size_t free_slots{};
            ::std::size_t reserved_slots{};
            ::std::size_t allocated_slots{};
            ::std::uint64_t generation{};
        };
        inline static constexpr ::std::size_t chunk_slots_offset{
            (sizeof(slab_chunk) + allocation_alignment - 1uz) / allocation_alignment * allocation_alignment};
        static_assert(alignof(slab_chunk) <= allocation_alignment);

        struct slot_reservation
        {
            allocation_header* envelope{};
            ::std::byte* original_array_slot{};
        };

        struct object_layout_before_sweep
        {
            gc_object_store const* owner{};
            // Guest-visible identity is an opaque process-unique token. The
            // allocation address must never be reused as a Wasm reference.
            void* token{};
            object* next{};
            object* hash_next{};
            ::std::atomic_flag mutation_lock = ATOMIC_FLAG_INIT;
            ::std::unique_ptr<gc_object_value[]> values{};
            ::std::size_t length{};
            ::std::uint_least32_t type_index{};
            gc_type::composite_kind kind{};
            // Cold foreign-object metadata follows every local get/set hot field.
            object* global_next{};
            // Retain arenas referenced by this object's fields. A receiving
            // module may unload before an exported object holding its values.
            gc_lease_owner value_leases{};
        };

        // Keep the exact object size, alignment and every existing field offset
        // on each target. If the mark byte cannot fit into existing padding,
        // this experimental implementation is rejected at compile time.
        static_assert(sizeof(object) == sizeof(object_layout_before_sweep));
        static_assert(alignof(object) == alignof(object_layout_before_sweep));
        static_assert(offsetof(object, owner) == offsetof(object_layout_before_sweep, owner));
        static_assert(offsetof(object, token) == offsetof(object_layout_before_sweep, token));
        static_assert(offsetof(object, next) == offsetof(object_layout_before_sweep, next));
        static_assert(offsetof(object, hash_next) == offsetof(object_layout_before_sweep, hash_next));
        static_assert(offsetof(object, mutation_lock) == offsetof(object_layout_before_sweep, mutation_lock));
        static_assert(offsetof(object, values) == offsetof(object_layout_before_sweep, values));
        static_assert(offsetof(object, length) == offsetof(object_layout_before_sweep, length));
        static_assert(offsetof(object, type_index) == offsetof(object_layout_before_sweep, type_index));
        static_assert(offsetof(object, kind) == offsetof(object_layout_before_sweep, kind));
        static_assert(offsetof(object, global_next) == offsetof(object_layout_before_sweep, global_next));
        static_assert(offsetof(object, value_leases) == offsetof(object_layout_before_sweep, value_leases));

#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1
        // Experimental persistent numeric ranges. The existing membership map
        // stays on its original store cache line and legacy arrays/mutable
        // aggregates retain their original locks and ownership protocol.
        // Only this real store's private factory can mint/publish a descriptor.

        [[nodiscard]] static constexpr gc_object_status compact_status(
            compact_numeric_status status) noexcept
        {
            switch(status)
            {
                case compact_numeric_status::ok: return gc_object_status::ok;
                case compact_numeric_status::invalid_store:
                case compact_numeric_status::busy:
                case compact_numeric_status::retiring:
                case compact_numeric_status::not_published:
                case compact_numeric_status::invariant_error: return gc_object_status::invalid_store;
                case compact_numeric_status::invalid_type: return gc_object_status::invalid_type;
                case compact_numeric_status::out_of_bounds: return gc_object_status::out_of_bounds;
                case compact_numeric_status::size_overflow: return gc_object_status::size_overflow;
                case compact_numeric_status::out_of_memory: return gc_object_status::out_of_memory;
                default: return gc_object_status::invalid_reference;
            }
        }
        [[nodiscard]] static constexpr bool compact_layout(type_layout const* layout) noexcept
        {
#if !defined(__cpp_exceptions)
            return false; // Preserve legacy allocation when control-block failures cannot be caught.
#endif
            if(layout == nullptr || layout->field_count != 1uz || !layout->fields ||
               layout->fields[0uz].mutable_ ||
               layout->fields[0uz].storage.packed != gc_type::packed_kind::none)
            { return false; }
            auto const kind{layout->fields[0uz].storage.value.kind};
            return kind == gc_type::value_kind::i32 || kind == gc_type::value_kind::f32;
        }

        // Native borrowed view, never a Wasm carrier, root, allocation ticket
        // or public authority. Its lifetime has EXACTLY the existing object*
        // API's contract: the caller pins the owner and excludes collection,
        // teardown and guest callbacks through the final immediate access.
        // A view cannot be kept through ANY poll/call/collection/control change.
        // Cold foreign lookup additionally retains the actual recipient lease.
        struct aggregate_object_view
        {
            object* legacy{};
            compact_numeric_descriptor* compact{};
            gc_object_store const* owning_store{};
            ::std::size_t compact_slot{};
            [[nodiscard]] explicit operator bool() const noexcept
            { return legacy != nullptr || compact != nullptr; }
            [[nodiscard]] gc_object_store const* owner() const noexcept
            { return legacy != nullptr ? legacy->owner : owning_store; }
            [[nodiscard]] ::std::uint_least32_t type_index() const noexcept
            { return legacy != nullptr ? legacy->type_index : compact->type_index_; }
            [[nodiscard]] gc_type::composite_kind kind() const noexcept
            { return legacy != nullptr ? legacy->kind : gc_type::composite_kind::struct_; }
        };

        [[nodiscard]] inline aggregate_object_view checked_local_compact_object(
            gc_reference reference) const noexcept
        {
            if(reference.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_struct ||
               reference.storage.ptr == nullptr) { return {}; }
            auto const token{reinterpret_cast<::std::uintptr_t>(reference.storage.ptr)};
            auto* current{compact_numeric_head_.load(::std::memory_order_acquire)};
            while(current != nullptr)
            {
                ::std::size_t slot{};
                if(compact_numeric_range_geometry::slot(current->token_begin_, current->capacity_, token, slot))
                {
                    if(current->phase_.load(::std::memory_order_acquire) != compact_numeric_descriptor::phase::published ||
                       slot >= current->initialized_frontier_.load(::std::memory_order_acquire)) { return {}; }
                    // [0,initialized_frontier<=capacity<=1024) owns real raw cells.
                    // Token subtraction is an integer range check, never a cast
                    // of a guest value into an object or native array pointer.
                    auto const bit{::std::uint64_t{1u} << (slot % 64uz)};
                    if((current->live_[slot / 64uz].load(::std::memory_order_acquire) & bit) == 0u)
                    { return {}; }
                    return {nullptr, current, this, slot};
                }
                // [store-owned initialized range chain] the head was published
                // with release after its canonical owner/layout/range/node.
                // ^^ follow only a native descriptor's strong owned next link.
                // Collection/teardown may unlink only after this caller drains.
                current = current->owner_next_.get();
            }
            return {};
        }

#if defined(__GNUC__) || defined(__clang__)
        [[gnu::noinline, gnu::cold]]
#elif defined(_MSC_VER)
        __declspec(noinline)
#endif
        [[nodiscard]] aggregate_object_view checked_foreign_compact_object(
            gc_reference reference) const noexcept
        {
            compact_numeric_reader reader{};
            if(compact_numeric_reader::try_foreign(reference, reader) != compact_numeric_status::ok)
            { return {}; }
            auto const* owner{reader.store_.get()};
            if(owner == nullptr) { return {}; }
            if(owner != this)
            {
                auto recipient{lease_owner_.lock()};
                if(!recipient || !recipient->hold(reader.store_)) { return {}; }
            }
            // [registered range][canonical typed source][actual recipient pin]
            // The cold reader excludes sweeping until the receiving lease is
            // installed. Its own shared admission retires on return; this view
            // then follows the same outer exclusion contract as legacy object*.
            return {nullptr, reader.descriptor_.get(), owner, reader.slot_};
        }
        [[nodiscard]] inline aggregate_object_view checked_aggregate_object(
            gc_reference reference, gc_type::composite_kind kind) const noexcept
        {
            if(kind == gc_type::composite_kind::struct_)
            {
                if(auto compact{checked_local_compact_object(reference)}; compact) { return compact; }
            }
            if(auto* local{checked_local_object(reference, kind)}; local != nullptr)
            { return {local, nullptr, local->owner, 0uz}; }
            if(reference.storage.ptr == nullptr ||
               reference.kind != (kind == gc_type::composite_kind::struct_ ?
                   ::uwvm2::object::global::wasm_ref_kind::wasm_struct :
                   ::uwvm2::object::global::wasm_ref_kind::wasm_array)) { return {}; }
            if(auto* foreign{checked_foreign_object(reference, kind)}; foreign != nullptr)
            { return {foreign, nullptr, foreign->owner, 0uz}; }
            return kind == gc_type::composite_kind::struct_ ?
                checked_foreign_compact_object(reference) : aggregate_object_view{};
        }
        [[nodiscard]] static inline gc_object_status compact_get(
            aggregate_object_view view, ::std::size_t field_index, gc_object_value& result) noexcept
        {
            if(!view || view.compact == nullptr) { return gc_object_status::invalid_reference; }
            if(field_index != 0uz) { return gc_object_status::out_of_bounds; }
            ::std::uint32_t bits{};
            auto const status{view.compact->payload_.load(view.compact_slot, bits)};
            if(status != compact_numeric_status::ok) { return compact_status(status); }
            // [one real raw uint32 cell] copy exactly four bytes into a complete
            // carrier; f32 NaN/sign bits remain untouched, with no floating ABI.
            result = gc_object_value::i32(bits);
            return gc_object_status::ok;
        }

        [[nodiscard]] gc_object_status prepare_private_compact_numeric_segment(
            ::std::uint_least32_t index, ::std::shared_ptr<compact_numeric_descriptor>& result) noexcept
        {
            auto const* layout{checked_type(index, gc_type::composite_kind::struct_)};
            if(!compact_layout(layout)) { return gc_object_status::invalid_type; }
            auto self{weak_from_this().lock()};
            if(!self || self.get() != this || !valid_) { return gc_object_status::invalid_store; }
            ::std::shared_ptr<gc_object_store const> canonical{self};
            ::std::weak_ptr<gc_object_store const> canonical_weak{weak_from_this()};
            ::std::shared_ptr<compact_numeric_descriptor> prepared{};
            auto const status{compact_numeric_descriptor::prepare_for_store(canonical, canonical_weak,
                index, canonical_type_id(index),
                layout->fields[0uz].storage.value.kind == gc_type::value_kind::i32 ?
                    compact_numeric_kind::i32 : compact_numeric_kind::f32,
                compact_numeric_range_geometry::reserved_width, prepared)};
            if(status != compact_numeric_status::ok) { return compact_status(status); }
            // Enter global admission BEFORE any store/descriptor registry lock.
            // Never wait for a collector's exclusive gate while holding a lock
            // that that collector itself needs to trace/retire these objects.
            auto admission{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
            slab_guard guard{*this};
            prepared->owner_next_ = compact_numeric_segments_;
            auto const publication{prepared->publish_range_from_store(prepared, next_object_token_id_)};
            if(publication != compact_numeric_status::ok) { return compact_status(publication); }
            // [complete published native range] install its real store owner
            // before exposing the release head or any newly issued guest token.
            compact_numeric_segments_ = prepared;
            compact_numeric_head_.store(prepared.get(), ::std::memory_order_release);
            result = ::std::move(prepared);
            return gc_object_status::ok;
        }
        [[nodiscard]] gc_object_status allocate_compact_numeric(
            ::std::uint_least32_t type_index, ::std::uint32_t bits, gc_reference& result) noexcept
        {
            // Correctness foundation, not the future sealed per-entry bump ABI.
            // A native store guard chooses one real range; no per-object legacy
            // header, local membership CAS or global stripe entry is published.
            auto admission{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
            ::std::shared_ptr<compact_numeric_descriptor> selected{};
            {
                slab_guard guard{*this};
                for(auto current{compact_numeric_segments_}; current;)
                {
                    if(current->type_index_ == type_index &&
                       current->phase_.load(::std::memory_order_acquire) == compact_numeric_descriptor::phase::published &&
                       current->initialized_frontier_.load(::std::memory_order_acquire) < current->capacity_)
                    { selected = current; break; }
                    // [strong store-owned range chain] retain next before the
                    // old local shared owner is released; no guest-derived link.
                    current = current->owner_next_;
                }
            }
            for(;;)
            {
                if(!selected)
                {
                    auto const status{prepare_private_compact_numeric_segment(type_index, selected)};
                    if(status != gc_object_status::ok) { return status; }
                }
                auto const appended{selected->append_raw32(bits, result)};
                if(appended != compact_numeric_status::out_of_bounds) { return compact_status(appended); }
                // A real concurrent appender may consume the final slot between
                // selection and append. Burn no identity and expose no reference;
                // prepare a new complete range and retry this one allocation.
                selected.reset();
            }
        }

        // Called only while all actual cohort actors/readers have drained.
        // Empty ranges unlink from both local/global indexes before payload free;
        // metadata follows live segments instead of all historically issued IDs.
        void retire_private_compact_numeric_segments_after_drain() noexcept
        {
            compact_numeric_head_.store(nullptr, ::std::memory_order_release);
            while(compact_numeric_segments_)
            {
                // [strong store-owned chain] detach one head before releasing it.
                // ^^ next remains pinned and payload bytes are never moved.
                auto current{::std::move(compact_numeric_segments_)};
                compact_numeric_segments_ = ::std::move(current->owner_next_);
                if(current->begin_retire() != compact_numeric_status::ok ||
                   current->finish_retire() != compact_numeric_status::ok)
                { ::std::terminate(); }
            }
        }

#endif

        ::std::unique_ptr<type_layout[]> layouts_{};
        ::std::size_t layout_count_{};
        ::std::atomic<object*> objects_{};
        ::std::weak_ptr<gc_lease_owner> lease_owner_{};
        // Object identities are reserved in blocks per thread, so allocation
        // does not contend on a process-wide counter for every object. Never
        // recycle an issued identity: a stale Wasm reference cannot alias a
        // later object even when the native allocator reuses its address.
        inline static constexpr ::std::uintptr_t object_token_first{0x10000u};
        inline static constexpr ::std::uintptr_t object_token_block_size{1024u};
        struct object_token_block
        {
            ::std::uintptr_t next;
            ::std::uintptr_t end;
        };
        inline static ::std::atomic<::std::uintptr_t> next_object_token_id_{object_token_first};
        inline static thread_local object_token_block object_tokens_{};
        // Published objects are inserted with release CAS and traversed with
        // acquire loads. Until a coordinated collector exists, they are
        // removed only after all guest threads drain at module teardown.
        // A forged token is compared as an integer key, never dereferenced.
        inline static constexpr ::std::size_t bucket_count{1uz << 16u};
        ::std::unique_ptr<::std::atomic<object*>[]> membership_buckets_{};
        // Canonical IDs are cold metadata. Keep membership_buckets_ on the
        // first store cache line for every local GC read/write.
        ::std::unique_ptr<::std::size_t[]> canonical_ids_{};
#if defined(UWVM_EXPERIMENTAL_ARRAY_SET_LOCAL_REF_AUTH) && UWVM_EXPERIMENTAL_ARRAY_SET_LOCAL_REF_AUTH == 1
        struct local_subtype_interval
        {
            ::std::uint32_t begin{}, end{};
        };
        // Immutable after validated construction; no guest token/object is cached.
        ::std::unique_ptr<local_subtype_interval[]> local_subtype_intervals_{};
#endif
        // Only a failed local membership probe enters this process-wide slow
        // path. A bucket stripe proves membership and pins the foreign owner
        // before any untrusted payload is dereferenced. Independent buckets
        // publish concurrently; the local membership read path is unchanged.
        inline static ::std::array<object*, bucket_count> global_buckets_{};
        inline static constexpr ::std::size_t global_stripe_count{1uz << 8u};
        struct alignas(64uz) global_stripe
        {
            // C++20 atomic_flag default construction initializes the clear state.
            ::std::atomic_flag lock;
        };
        inline static ::std::array<global_stripe, global_stripe_count> global_stripes_{};
        inline static ::uwvm2::validation::standard::wasm3::recursive_type_registry canonical_registry_{};
        inline static ::std::atomic_flag canonical_lock_ = ATOMIC_FLAG_INIT;
        // An externref created from an anyref carries an opaque token, never a
        // dereferenceable guest pointer. These process-wide indexes let another
        // live module unwrap the same token without trusting its payload.
        struct extern_bridge
        {
            gc_object_store const* owner{};
            gc_reference inner{};
            // A bridge may outlive the module that supplied a foreign inner
            // aggregate. Retain that arena independently of the creator's
            // module lease roots until this bridge is removed.
            ::std::shared_ptr<gc_object_store const> inner_owner{};
            void* token{};
            extern_bridge* token_next{};
            extern_bridge* inner_next{};
        };
        inline static constexpr ::std::size_t bridge_bucket_count{1uz << 10u};
        inline static ::std::array<extern_bridge*, bridge_bucket_count> bridge_token_buckets_{};
        inline static ::std::array<extern_bridge*, bridge_bucket_count> bridge_inner_buckets_{};
        inline static ::std::atomic_flag bridge_lock_ = ATOMIC_FLAG_INIT;
        inline static ::std::uintptr_t next_bridge_id_{1u};
        inline static constexpr unsigned token_payload_bits{sizeof(::std::uintptr_t) * 8u - 8u};
        inline static constexpr ::std::uintptr_t token_payload_mask{(::std::uintptr_t{1u} << token_payload_bits) - 1u};
        inline static constexpr ::std::uintptr_t token_prefix{~token_payload_mask};
        // Exnref carries a VM-issued identity, never value*. The registry owns
        // immutable value_ref while at least one module roots its token.
        struct exn_token_entry
        {
#if defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) && UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES == 1
            ::uwvm2::runtime::exception::immutable_record_ref value{};
#else
            ::uwvm2::runtime::exception::value_ref value{};
#endif
            // Foreign modules lease this issuer separately; the token itself
            // never owns its originating store and cannot create a self-cycle.
            ::std::weak_ptr<gc_object_store const> owner{};
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
            // Captured ONLY from a genuine retained external handle. Weak origin
            // authenticates re-materialization without a source/store ownership cycle.
            ::std::weak_ptr<::uwvm2::runtime::exception::external_exception_lifetime const> source_origin{};
            void const* source_native_leaf_certificate{}; // Actual external-block certificate only.
#endif
            ::std::uintptr_t token{};
            ::std::size_t root_count{};
            exn_token_entry* next{};
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
            // Collection-only metadata, never hot-path authority or root seeds.
            bool collection_marked{};
            ::std::size_t collection_recipients{};
#endif
        };
        struct exn_root_node
        {
            exn_token_entry* entry{};
            exn_root_node* next{};
            exn_root_node* bucket_next{};
        };
        inline static constexpr ::std::size_t exn_bucket_count{1uz << 10u};
        inline static ::std::array<exn_token_entry*, exn_bucket_count> exn_buckets_{};
        inline static ::std::atomic_flag exn_lock_ = ATOMIC_FLAG_INIT;
        inline static ::std::uintptr_t next_exn_id_{1u};
        inline static constexpr ::std::uintptr_t exn_token_prefix{::std::uintptr_t{0xfd} << token_payload_bits};
        mutable ::std::atomic_flag exn_roots_lock_ = ATOMIC_FLAG_INIT;
        mutable exn_root_node* exn_roots_{};
        // A module can read thousands of distinct exception tokens from a table.
        // Keep membership lookup bounded by a bucket chain rather than walking
        // every root on each table.get; the teardown chain remains independent.
        using exn_root_bucket_array = ::std::array<exn_root_node*, exn_bucket_count>;
        // Most modules never handle an exnref. Allocate the 8 KiB membership
        // index only on first use so ordinary module startup stays unchanged.
        mutable ::std::unique_ptr<exn_root_bucket_array> exn_root_buckets_{};
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
# include "gc_exception_graph.h"
#endif
        bool valid_{};
        // Cold admission metadata, including valid stores with no aggregates.
        // An empty module can still own a global/table incoming edge; object
        // token indexes alone therefore cannot prove a closed store cohort.
        gc_object_store* cohort_previous_{};
        gc_object_store* cohort_next_{};
        bool cohort_registered_{};
        inline static gc_object_store* cohort_head_{};
        inline static ::std::atomic_flag cohort_lock_ = ATOMIC_FLAG_INIT;

        // Kept after all existing store fields: local token membership and
        // object field checks gain no new load or offset. Every allocator list
        // and accounting transition is protected by this store-local lock.
        mutable ::std::atomic_flag slab_lock_ = ATOMIC_FLAG_INIT;
        ::std::array<slab_chunk*, numeric_slab_classes> slab_all_{};
        ::std::array<slab_chunk*, numeric_slab_classes> slab_available_{};
        ::std::uint64_t slab_epoch_{1u};
        ::std::uint64_t next_slab_generation_{1u};
        ::std::size_t slab_chunk_count_{};
        ::std::size_t slab_reserved_slots_{};
        ::std::size_t slab_allocated_slots_{};
        ::std::size_t slab_backing_bytes_{};
        bool slab_closing_{};
#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1
        // New cold fields follow ALL legacy instance data. Preserve the old
        // layouts_/membership/cohort/slab offsets in this experimental mode.
        ::std::shared_ptr<compact_numeric_descriptor> compact_numeric_segments_{};
        ::std::atomic<compact_numeric_descriptor*> compact_numeric_head_{};
#endif

        // Follow every pre-existing field, preserving allocator/layout offsets.
        // Only the native constructor binds this code pointer, before store
        // publication; guest values cannot replace it or furnish a callback.
        gc_function_type_match_callback const function_type_match_{};
        // Cold staging-only source lineage. Ordinary constructors leave this empty;
        // no local/foreign object access or allocator hot field gains a new load.
        ::std::weak_ptr<::uwvm2::uwvm::runtime::full::full_source_instance const> checkpoint_source_binding_{};

        struct slab_guard
        {
            gc_object_store const* store;
            explicit slab_guard(gc_object_store const& owner) noexcept : store{::std::addressof(owner)}
            { while(store->slab_lock_.test_and_set(::std::memory_order_acquire)) {} }
            ~slab_guard() { store->slab_lock_.clear(::std::memory_order_release); }
            slab_guard(slab_guard const&) = delete;
            slab_guard& operator=(slab_guard const&) = delete;
        };
        [[nodiscard]] static inline slab_slot_state metadata_state(allocation_metadata const& metadata) noexcept
        { return static_cast<slab_slot_state>(metadata.link_state & slab_state_mask); }
        [[nodiscard]] static inline ::std::uint64_t metadata_generation(allocation_metadata const& metadata) noexcept
        { return static_cast<::std::uint64_t>(metadata.link_state >> slab_generation_shift); }
        [[nodiscard]] static inline ::std::size_t metadata_slot_index(allocation_metadata const& metadata) noexcept
        { return static_cast<::std::size_t>((metadata.link_state >> 2u) & slab_slot_index_mask); }
        [[nodiscard]] static inline slab_chunk* active_metadata_chunk(allocation_metadata const& metadata) noexcept
        {
            if(metadata_state(metadata) != slab_slot_state::reserved &&
               metadata_state(metadata) != slab_slot_state::allocated) { ::std::terminate(); }
            return metadata_generation(metadata) == 0u ? nullptr :
                static_cast<slab_chunk*>(metadata.owner_or_storage);
        }
        static inline void set_active_metadata(allocation_metadata& metadata, ::std::uint64_t generation,
            ::std::size_t slot_index, slab_slot_state state) noexcept
        {
            if(generation >= slab_generation_limit || slot_index >= numeric_slab_slots ||
               (generation == 0u && slot_index != 0uz) ||
               (state != slab_slot_state::reserved && state != slab_slot_state::allocated)) { ::std::terminate(); }
            metadata.link_state = (static_cast<::std::uintptr_t>(generation) << slab_generation_shift) |
                (static_cast<::std::uintptr_t>(slot_index) << 2u) |
                static_cast<::std::uintptr_t>(state);
        }
        [[nodiscard]] static inline ::std::size_t next_free_slot(allocation_metadata const& metadata) noexcept
        {
            if(metadata_state(metadata) != slab_slot_state::free) { ::std::terminate(); }
            auto const next{static_cast<::std::size_t>(metadata.link_state >> 2u)};
            if(next > slab_no_slot) { ::std::terminate(); }
            return next;
        }
        static inline void set_free_metadata(allocation_metadata& metadata, ::std::size_t next) noexcept
        {
            if(next > slab_no_slot) { ::std::terminate(); }
            metadata.link_state = static_cast<::std::uintptr_t>(next) << 2u;
        }
        [[nodiscard]] static inline allocation_header* allocation_header_for(object* header) noexcept
        {
            // [LIVE object union member]<->[LIVE named union]<->[LIVE envelope]
            // ^^ both casts use [basic.compound]/7 + [expr.static.cast]/12,
            // before object destruction. No launder or byte arithmetic repairs
            // a dead header or makes an unrelated containing object exist.
            auto* payload{static_cast<object_slot*>(static_cast<void*>(header))};
            return static_cast<allocation_header*>(static_cast<void*>(payload));
        }
        [[nodiscard]] static inline ::std::byte* slab_slot_bytes(slab_chunk const& chunk,
            ::std::size_t slot_index) noexcept
        {
            if(slot_index >= numeric_slab_slots || chunk.backing == nullptr ||
               chunk.bytes < chunk_slots_offset || chunk.stride < object_value_offset ||
               chunk.stride % allocation_alignment != 0uz ||
               chunk.stride > (chunk.bytes - chunk_slots_offset) / numeric_slab_slots) { ::std::terminate(); }
            // [original backing byte array, backing + bytes)
            //             [256 checked whole slots]              end
            // ^^ only the retained array-element pointer participates in
            // addition; each index/stride product is inside that same array.
            return chunk.backing + chunk_slots_offset + slot_index * chunk.stride;
        }
        [[nodiscard]] static inline allocation_header* slab_slot_header(slab_chunk const& chunk,
            ::std::size_t slot_index) noexcept
        {
            // The envelope at this checked offset was placement-constructed
            // when the chunk was made and stays live until chunk teardown.
            // The original byte-array element reaches the entire containing
            // byte array [basic.compound]/8, including this whole envelope.
            // ^^ launder selects an EXISTING live envelope at this address;
            // it creates neither storage nor lifetime and enlarges no reach.
            auto* location{slab_slot_bytes(chunk, slot_index)};
            return ::std::launder(reinterpret_cast<allocation_header*>(location));
        }
        static inline void poison_free_slab_body(slab_chunk const& chunk, ::std::size_t slot_index) noexcept
        {
#if defined(UWVM2_GC_NUMERIC_SLAB_ADDRESS_SANITIZER)
            auto* body{slab_slot_bytes(chunk, slot_index)};
            // [ended/unconstructed union member][LIVE cold metadata][tail]
            // ^^ poison only object/tail; leave envelope metadata addressable.
            gc_numeric_slab_poison(body, sizeof(object_slot));
            gc_numeric_slab_poison(body + object_value_offset, chunk.stride - object_value_offset);
#else
            static_cast<void>(chunk); static_cast<void>(slot_index);
#endif
        }
        static inline void unpoison_reserved_slab_body(slab_chunk const& chunk, ::std::size_t slot_index) noexcept
        {
#if defined(UWVM2_GC_NUMERIC_SLAB_ADDRESS_SANITIZER)
            auto* body{slab_slot_bytes(chunk, slot_index)};
            // [reserved complete slot] both extents are original-array ranges.
            // ^^ activate access before placement construction, not lifetime.
            gc_numeric_slab_unpoison(body, sizeof(object_slot));
            gc_numeric_slab_unpoison(body + object_value_offset, chunk.stride - object_value_offset);
#else
            static_cast<void>(chunk); static_cast<void>(slot_index);
#endif
        }
        static inline void destroy_object(object* header) noexcept
        {
            if(header == nullptr) { return; }
            // Recover the live envelope BEFORE ending its union-member lifetime.
            // The object is not a most-derived allocation and cannot be the
            // operand of an ordinary delete-expression [expr.delete]/2.
            auto* envelope{allocation_header_for(header)};
            header->~object();
            // [LIVE envelope/metadata][ended object/carrier/lease lifetimes]
            // ^^ direct cold class-deallocator call, passing the live envelope.
            object::operator delete(static_cast<void*>(envelope));
        }
        static inline void release_empty_chunk(slab_chunk& chunk) noexcept
        {
            if(chunk.free_slots != numeric_slab_slots || chunk.reserved_slots != 0uz ||
               chunk.allocated_slots != 0uz) { ::std::terminate(); }
            auto* backing{chunk.backing};
#if defined(UWVM2_GC_NUMERIC_SLAB_ADDRESS_SANITIZER)
            gc_numeric_slab_unpoison(backing, chunk.bytes);
#endif
            for(::std::size_t index{}; index != numeric_slab_slots; ++index)
            {
                // [live byte array][live free envelope, no active object]
                // ^^ end only cold union/envelope/metadata lifetimes; object
                // and carrier lifetimes have already ended exactly once.
                auto* envelope{slab_slot_header(chunk, index)};
                if(metadata_state(envelope->metadata) != slab_slot_state::free) { ::std::terminate(); }
                envelope->~allocation_header();
            }
            chunk.~slab_chunk();
            // [original array-new pointer] never reconstruct the cookie/base.
            // ^^ matching array delete, after every nested lifetime has ended.
            delete[] backing;
        }
        static inline void remove_available_chunk(gc_object_store& store, slab_chunk& chunk) noexcept
        {
            // [store-local doubly linked available list] slab_lock_ is held.
            // ^^ splice both initialized neighbor links before detaching chunk.
            if(chunk.available_previous != nullptr)
            { chunk.available_previous->available_next = chunk.available_next; }
            else { store.slab_available_[chunk.class_index] = chunk.available_next; }
            if(chunk.available_next != nullptr)
            { chunk.available_next->available_previous = chunk.available_previous; }
            chunk.available_previous = nullptr;
            chunk.available_next = nullptr;
        }
        static inline void add_available_chunk(gc_object_store& store, slab_chunk& chunk) noexcept
        {
            // [detached chunk with at least one free slot][owned available list]
            // ^^ publish initialized predecessor/successor under slab_lock_.
            chunk.available_previous = nullptr;
            chunk.available_next = store.slab_available_[chunk.class_index];
            if(chunk.available_next != nullptr) { chunk.available_next->available_previous = ::std::addressof(chunk); }
            store.slab_available_[chunk.class_index] = ::std::addressof(chunk);
        }
        [[nodiscard]] inline bool numeric_slab_eligible(::std::uint_least32_t type_index,
            gc_type::composite_kind kind, ::std::size_t length) const noexcept
        {
            if(kind != gc_type::composite_kind::struct_ || length == 0uz ||
               length > numeric_slab_classes || type_index >= layout_count_) { return false; }
            auto const& layout{layouts_[type_index]};
            if(layout.kind != kind || layout.field_count != length) { return false; }
            return !layout.has_reference_fields;
        }
#if defined(UWVM_EXPERIMENTAL_SINGLE_LOCK_NUMERIC_SLAB) && UWVM_EXPERIMENTAL_SINGLE_LOCK_NUMERIC_SLAB == 1 || \
    (defined(UWVM_EXPERIMENTAL_GENERAL_GC_SLAB) && UWVM_EXPERIMENTAL_GENERAL_GC_SLAB == 1)
        [[nodiscard]] inline gc_object_status reserve_numeric_slot_locked(::std::size_t length,
            slot_reservation& result, slab_guard const& held) noexcept
#else
        [[nodiscard]] inline gc_object_status reserve_numeric_slot(::std::size_t length,
            slot_reservation& result) noexcept
#endif
        {
#if defined(UWVM_EXPERIMENTAL_GENERAL_GC_SLAB) && UWVM_EXPERIMENTAL_GENERAL_GC_SLAB == 1
            if(length == 0uz || length > 512uz) { return gc_object_status::size_overflow; }
            // length was bounded to [1,512]. The shift is at most nine;
            // bit_width maps the original power-of-two classes on every host.
            auto const index{static_cast<::std::size_t>(::std::bit_width(length - 1uz))};
            auto const capacity{::std::size_t{1uz} << index};
#else
            auto const index{length - 1uz};
            auto const capacity{length};
#endif
#if defined(UWVM_EXPERIMENTAL_SINGLE_LOCK_NUMERIC_SLAB) && UWVM_EXPERIMENTAL_SINGLE_LOCK_NUMERIC_SLAB == 1 || \
    (defined(UWVM_EXPERIMENTAL_GENERAL_GC_SLAB) && UWVM_EXPERIMENTAL_GENERAL_GC_SLAB == 1)
            if(held.store != this) { ::std::terminate(); }
#else
            slab_guard guard{*this};
#endif
            if(slab_closing_) { return gc_object_status::invalid_store; }
            auto* chunk{slab_available_[index]};
            if(chunk == nullptr)
            {
                if(next_slab_generation_ >= slab_generation_limit)
                { return gc_object_status::size_overflow; }
                auto const body_bytes{object_value_offset + capacity * sizeof(gc_object_value)};
                auto const stride{(body_bytes + allocation_alignment - 1uz) / allocation_alignment * allocation_alignment};
                auto constexpr maximum{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())};
                if(stride > (maximum - chunk_slots_offset) / numeric_slab_slots)
                { return gc_object_status::size_overflow; }
                auto const bytes{chunk_slots_offset + stride * numeric_slab_slots};
                if(slab_chunk_count_ == (::std::numeric_limits<::std::size_t>::max)() / numeric_slab_slots ||
                   bytes > (::std::numeric_limits<::std::size_t>::max)() - slab_backing_bytes_)
                { return gc_object_status::size_overflow; }
                // This expression starts a real byte ARRAY lifetime. Its
                // original result, including any implementation array cookie,
                // is retained. Fundamental alignment is [expr.new]/17.
                auto* backing{new(::std::nothrow) ::std::byte[bytes]};
                if(backing == nullptr) { return gc_object_status::out_of_memory; }
                chunk = ::new(static_cast<void*>(backing)) slab_chunk{};
                chunk->backing = backing;
                chunk->owner = this;
                chunk->stride = stride;
                chunk->bytes = bytes;
                chunk->class_index = index;
                chunk->generation = next_slab_generation_++;
                for(::std::size_t slot{}; slot != numeric_slab_slots; ++slot)
                {
                    // [backing byte array][checked whole slot] end
                    // ^^ form the address from the original array, then create
                    // a live envelope/union; no object member starts yet.
                    auto* slot_address{slab_slot_bytes(*chunk, slot)};
                    auto* envelope{::new(static_cast<void*>(slot_address)) allocation_header{}};
                    envelope->metadata.owner_or_storage = chunk;
                    set_free_metadata(envelope->metadata, chunk->free_head);
                    chunk->free_head = slot;
                    poison_free_slab_body(*chunk, slot);
                    ++chunk->free_slots;
                }
                chunk->all_next = slab_all_[index];
                if(chunk->all_next != nullptr) { chunk->all_next->all_previous = chunk; }
                slab_all_[index] = chunk;
                add_available_chunk(*this, *chunk);
                ++slab_chunk_count_;
                slab_backing_bytes_ += bytes;
            }
            auto const slot_index{chunk->free_head};
            if(slot_index >= numeric_slab_slots || chunk->free_slots == 0uz) { ::std::terminate(); }
            auto* envelope{slab_slot_header(*chunk, slot_index)};
            auto& metadata{envelope->metadata};
            if(metadata_state(metadata) != slab_slot_state::free ||
               metadata.owner_or_storage != chunk) { ::std::terminate(); }
            // [free index][live envelope] no native link or guest ID is used.
            // ^^ transfer exactly this initialized slot to reserved ownership.
            chunk->free_head = next_free_slot(metadata);
            set_active_metadata(metadata, chunk->generation, slot_index, slab_slot_state::reserved);
            unpoison_reserved_slab_body(*chunk, slot_index);
            --chunk->free_slots;
            ++chunk->reserved_slots;
            ++slab_reserved_slots_;
            if(chunk->free_slots == 0uz) { remove_available_chunk(*this, *chunk); }
            result = {envelope, slab_slot_bytes(*chunk, slot_index)};
            return gc_object_status::ok;
        }
#if defined(UWVM_EXPERIMENTAL_SINGLE_LOCK_NUMERIC_SLAB) && UWVM_EXPERIMENTAL_SINGLE_LOCK_NUMERIC_SLAB == 1 || \
    (defined(UWVM_EXPERIMENTAL_GENERAL_GC_SLAB) && UWVM_EXPERIMENTAL_GENERAL_GC_SLAB == 1)
        inline void commit_reserved_slot_locked(allocation_metadata& metadata, slab_guard const& held) noexcept
#else
        inline void commit_reserved_slot(allocation_metadata& metadata) noexcept
#endif
        {
            auto* selected{active_metadata_chunk(metadata)};
            if(selected == nullptr)
            { set_active_metadata(metadata, 0u, 0uz, slab_slot_state::allocated); return; }
#if defined(UWVM_EXPERIMENTAL_SINGLE_LOCK_NUMERIC_SLAB) && UWVM_EXPERIMENTAL_SINGLE_LOCK_NUMERIC_SLAB == 1 || \
    (defined(UWVM_EXPERIMENTAL_GENERAL_GC_SLAB) && UWVM_EXPERIMENTAL_GENERAL_GC_SLAB == 1)
            if(held.store != this) { ::std::terminate(); }
#else
            slab_guard guard{*this};
#endif
            auto& chunk{*selected};
            if(chunk.owner != this || metadata_generation(metadata) != chunk.generation ||
               metadata_state(metadata) != slab_slot_state::reserved || chunk.reserved_slots == 0uz) { ::std::terminate(); }
            set_active_metadata(metadata, chunk.generation, metadata_slot_index(metadata), slab_slot_state::allocated);
            --chunk.reserved_slots;
            --slab_reserved_slots_;
            ++chunk.allocated_slots;
            ++slab_allocated_slots_;
        }
#if defined(UWVM_EXPERIMENTAL_SINGLE_LOCK_NUMERIC_SLAB) && UWVM_EXPERIMENTAL_SINGLE_LOCK_NUMERIC_SLAB == 1 || \
    (defined(UWVM_EXPERIMENTAL_GENERAL_GC_SLAB) && UWVM_EXPERIMENTAL_GENERAL_GC_SLAB == 1)
        [[nodiscard]] inline gc_object_status reserve_numeric_slot(::std::size_t length,
            slot_reservation& result) noexcept
        {
            slab_guard held{*this};
            return reserve_numeric_slot_locked(length, result, held);
        }
        inline void commit_reserved_slot(allocation_metadata& metadata) noexcept
        {
            if(active_metadata_chunk(metadata) == nullptr)
            { set_active_metadata(metadata, 0u, 0uz, slab_slot_state::allocated); return; }
            slab_guard held{*this};
            commit_reserved_slot_locked(metadata, held);
        }
# include "numeric_slab_single_lock.h"
#endif

        // The guard proves allocator-list exclusion only. Collection admission,
        // index removal and ended object lifetimes remain the caller's duties.
        [[nodiscard]] static inline slab_chunk* retire_slab_allocation_locked(
            allocation_header* envelope, slab_guard const& held) noexcept
        {
            auto& metadata{envelope->metadata};
            auto* chunk{active_metadata_chunk(metadata)};
            if(chunk == nullptr || chunk->owner != held.store) { ::std::terminate(); }
            auto const slot_index{metadata_slot_index(metadata)};
            auto& store{*chunk->owner};
            if(metadata_generation(metadata) != chunk->generation ||
               slab_slot_header(*chunk, slot_index) != envelope ||
               (metadata_state(metadata) != slab_slot_state::reserved &&
                metadata_state(metadata) != slab_slot_state::allocated)) { ::std::terminate(); }
            if(metadata_state(metadata) == slab_slot_state::reserved)
            {
                if(chunk->reserved_slots == 0uz) { ::std::terminate(); }
                --chunk->reserved_slots; --store.slab_reserved_slots_;
            }
            else
            {
                if(chunk->allocated_slots == 0uz) { ::std::terminate(); }
                --chunk->allocated_slots; --store.slab_allocated_slots_;
            }
            // [ended body][live cold envelope metadata][free integer chain]
            // ^^ caller first removed all indexes, or never published this
            // token. Poison and return its slot only after carrier/lease dtor.
            poison_free_slab_body(*chunk, slot_index);
            set_free_metadata(metadata, chunk->free_head);
            chunk->free_head = slot_index;
            if(chunk->free_slots++ == 0uz) { add_available_chunk(store, *chunk); }
            if(chunk->free_slots == numeric_slab_slots)
            {
                if(chunk->reserved_slots != 0uz || chunk->allocated_slots != 0uz) { ::std::terminate(); }
                remove_available_chunk(store, *chunk);
                if(chunk->all_previous != nullptr) { chunk->all_previous->all_next = chunk->all_next; }
                else { store.slab_all_[chunk->class_index] = chunk->all_next; }
                if(chunk->all_next != nullptr) { chunk->all_next->all_previous = chunk->all_previous; }
                --store.slab_chunk_count_;
                store.slab_backing_bytes_ -= chunk->bytes;
                return chunk;
            }
            return nullptr;
        }
        static inline void retire_object_allocation(allocation_header* envelope) noexcept
        {
            // The explicit destructor has ended object/tail lifetimes; envelope,
            // union and metadata are still alive. Never access the old object.
            auto& metadata{envelope->metadata};
            auto* chunk{active_metadata_chunk(metadata)};
            if(chunk == nullptr)
            {
                auto* backing{static_cast<::std::byte*>(metadata.owner_or_storage)};
                envelope->~allocation_header();
                // [saved original fallback byte-array pointer]
                // ^^ matching delete[] after all nested lifetimes have ended.
                delete[] backing;
                return;
            }
            auto& store{*chunk->owner};
            slab_chunk* retired{};
            {
                slab_guard guard{store};
                retired = retire_slab_allocation_locked(envelope, guard);
            }
            if(retired != nullptr)
            {
                // [detached all-free chunk] no indexes, reservations, TLS cache
                // or live object retains a slot. No allocator/index lock held.
                // ^^ end its envelopes and free its own original byte array.
                release_empty_chunk(*retired);
            }
        }
#if defined(UWVM_EXPERIMENTAL_GENERAL_GC_SLAB) && UWVM_EXPERIMENTAL_GENERAL_GC_SLAB == 1
# include "gc_sweep_retirement_batch.h"
#endif
        [[nodiscard]] inline bool can_advance_slab_epoch_exclusive() const noexcept
        {
            slab_guard guard{*this};
            return !slab_closing_ && slab_reserved_slots_ == 0uz &&
                slab_epoch_ != (::std::numeric_limits<::std::uint64_t>::max)();
        }
        inline void advance_slab_epoch_exclusive() noexcept
        {
            slab_guard guard{*this};
            if(slab_closing_ || slab_reserved_slots_ != 0uz ||
               slab_epoch_ == (::std::numeric_limits<::std::uint64_t>::max)()) { ::std::terminate(); }
            // A future weak TLS ticket must compare this epoch under the same
            // admission protocol before touching a cached chunk/slot. This
            // first candidate retains no TLS ticket or owner of any strength.
            ++slab_epoch_;
        }

        struct cohort_guard
        {
            cohort_guard() noexcept
            { while(cohort_lock_.test_and_set(::std::memory_order_acquire)) {} }
            ~cohort_guard() { cohort_lock_.clear(::std::memory_order_release); }
            cohort_guard(cohort_guard const&) = delete;
            cohort_guard& operator=(cohort_guard const&) = delete;
        };
        inline void register_cohort_member() noexcept
        {
            cohort_guard guard{};
            // [fully initialized valid store][live admission list]
            // ^^ link this native store before constructor publication.
            cohort_next_ = cohort_head_;
            if(cohort_head_ != nullptr) { cohort_head_->cohort_previous_ = this; }
            cohort_head_ = this;
            cohort_registered_ = true;
        }
        inline void unregister_cohort_member() noexcept
        {
            cohort_guard guard{};
            if(!cohort_registered_) { return; }
            // [previous][this][next] admission lock keeps all slots live.
            // ^^ detach before teardown frees any object/root metadata.
            if(cohort_previous_ != nullptr) { cohort_previous_->cohort_next_ = cohort_next_; }
            else { cohort_head_ = cohort_next_; }
            if(cohort_next_ != nullptr) { cohort_next_->cohort_previous_ = cohort_previous_; }
            cohort_previous_ = nullptr;
            cohort_next_ = nullptr;
            cohort_registered_ = false;
        }

        struct bridge_guard
        {
            bridge_guard() noexcept
            { while(bridge_lock_.test_and_set(::std::memory_order_acquire)) {} }
            ~bridge_guard() { bridge_lock_.clear(::std::memory_order_release); }
            bridge_guard(bridge_guard const&) = delete;
            bridge_guard& operator=(bridge_guard const&) = delete;
        };
        struct global_guard
        {
            ::std::atomic_flag& lock;
            explicit global_guard(::std::size_t bucket) noexcept :
                lock(global_stripes_[bucket & (global_stripe_count - 1uz)].lock)
            { while(lock.test_and_set(::std::memory_order_acquire)) {} }
            ~global_guard() { lock.clear(::std::memory_order_release); }
            global_guard(global_guard const&) = delete;
            global_guard& operator=(global_guard const&) = delete;
        };
        struct canonical_guard
        {
            canonical_guard() noexcept
            { while(canonical_lock_.test_and_set(::std::memory_order_acquire)) {} }
            ~canonical_guard() { canonical_lock_.clear(::std::memory_order_release); }
            canonical_guard(canonical_guard const&) = delete;
            canonical_guard& operator=(canonical_guard const&) = delete;
        };
        struct exn_guard
        {
            exn_guard() noexcept
            { while(exn_lock_.test_and_set(::std::memory_order_acquire)) {} }
            ~exn_guard() { exn_lock_.clear(::std::memory_order_release); }
            exn_guard(exn_guard const&) = delete;
            exn_guard& operator=(exn_guard const&) = delete;
        };
        struct exn_roots_guard
        {
            gc_object_store const& store;
            explicit exn_roots_guard(gc_object_store const& value) noexcept : store(value)
            { while(store.exn_roots_lock_.test_and_set(::std::memory_order_acquire)) {} }
            ~exn_roots_guard() { store.exn_roots_lock_.clear(::std::memory_order_release); }
            exn_roots_guard(exn_roots_guard const&) = delete;
            exn_roots_guard& operator=(exn_roots_guard const&) = delete;
        };
        [[nodiscard]] static inline bool exn_token_shape(void const* token) noexcept
        {
            return (reinterpret_cast<::std::uintptr_t>(token) & ~token_payload_mask) == exn_token_prefix;
        }
        [[nodiscard]] static inline ::std::size_t exn_bucket(::std::uintptr_t token) noexcept
        { return static_cast<::std::size_t>(token) & (exn_bucket_count - 1uz); }
        [[nodiscard]] static inline exn_token_entry* find_exn_locked(void const* token) noexcept
        {
            auto const identity{reinterpret_cast<::std::uintptr_t>(token)};
            auto* current{exn_buckets_[exn_bucket(identity)]};
            while(current != nullptr)
            {
                if(current->token == identity) { return current; }
                // [registered exn token chain] exn_lock_ excludes removal.
                // ^^ current advances only to a live registered entry or null.
                current = current->next;
            }
            return nullptr;
        }
        [[nodiscard]] inline gc_object_status retain_exn_reference(gc_reference reference) const noexcept
        {
            if(reference.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_exn ||
               !exn_token_shape(reference.storage.ptr)) { return gc_object_status::invalid_reference; }
            exn_guard registry_guard{};
            auto* entry{find_exn_locked(reference.storage.ptr)};
            if(entry == nullptr) { return gc_object_status::invalid_reference; }
            exn_roots_guard roots_guard{*this};
            auto const bucket_index{exn_bucket(entry->token)};
            for(auto* current{exn_root_buckets_ ? (*exn_root_buckets_)[bucket_index] : nullptr};
                current != nullptr; current = current->bucket_next)
            {
                if(current->entry == entry) { return gc_object_status::ok; }
                // [module-owned exn root bucket] roots_guard excludes mutation.
                // ^^ current advances only to a live root in this bucket or null.
            }
            // The common table.get/local path already owns this token. Allocate
            // only for the first transfer into this module's root set.
            ::std::unique_ptr<exn_root_node> fresh{new(::std::nothrow) exn_root_node{}};
            if(!fresh) { return gc_object_status::out_of_memory; }
            if(!exn_root_buckets_)
            {
                exn_root_buckets_.reset(new(::std::nothrow) exn_root_bucket_array{});
                if(!exn_root_buckets_) { return gc_object_status::out_of_memory; }
            }
            auto origin{entry->owner.lock()};
            if(!origin) { return gc_object_status::invalid_reference; }
            if(origin.get() != this)
            {
                // The receiving module, rather than its GC store, owns this
                // edge so mutually importing modules cannot retain each other.
                auto roots{lease_owner_.lock()};
                if(!roots) { return gc_object_status::invalid_store; }
                if(!roots->hold(::std::move(origin))) { return gc_object_status::out_of_memory; }
            }
            fresh->entry = entry;
            // [fresh root][module root head] exn_lock_ protects entry root_count.
            // ^^ publish an owning module root before the source can retire.
            fresh->bucket_next = (*exn_root_buckets_)[bucket_index];
            (*exn_root_buckets_)[bucket_index] = fresh.get();
            fresh->next = exn_roots_;
            exn_roots_ = fresh.release();
            ++entry->root_count;
            return gc_object_status::ok;
        }
        [[nodiscard]] static inline bool bridge_token_shape(void const* token) noexcept
        { return (reinterpret_cast<::std::uintptr_t>(token) & token_prefix) == token_prefix; }
        [[nodiscard]] static inline ::std::size_t bridge_token_bucket(void const* token) noexcept
        { return bucket_index(token) & (bridge_bucket_count - 1uz); }
        [[nodiscard]] static inline ::std::size_t bridge_inner_bucket(gc_reference reference) noexcept
        {
            auto bits{reference.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_i31 ?
                static_cast<::std::uintptr_t>(reference.storage.wasm_i31.get_u()) :
                reinterpret_cast<::std::uintptr_t>(reference.storage.ptr)};
            bits ^= static_cast<::std::uintptr_t>(reference.kind) << 3u;
            bits ^= bits >> 17u;
            bits ^= bits >> 9u;
            return static_cast<::std::size_t>(bits) & (bridge_bucket_count - 1uz);
        }
        [[nodiscard]] static inline bool same_bridge_inner(gc_reference left, gc_reference right) noexcept
        {
            if(left.kind != right.kind) { return false; }
            if(left.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_i31)
            { return left.storage.wasm_i31.get_u() == right.storage.wasm_i31.get_u(); }
            return left.storage.ptr == right.storage.ptr;
        }
        // bridge_lock_ protects both linked indexes and each bridge's lifetime.
        // The caller never dereferences a token supplied by Wasm or a host.
        [[nodiscard]] static inline extern_bridge* find_bridge_token_locked(void const* token) noexcept
        {
            auto* curr{bridge_token_buckets_[bridge_token_bucket(token)]};
            while(curr != nullptr)
            {
                if(curr->token == token) { return curr; }
                // [registered bridge node] token_next is initialized before publication.
                // ^^ curr advances only through the lock-protected bucket chain.
                curr = curr->token_next;
            }
            return nullptr;
        }
        [[nodiscard]] static inline extern_bridge* find_bridge_inner_locked(gc_reference reference) noexcept
        {
            auto* curr{bridge_inner_buckets_[bridge_inner_bucket(reference)]};
            while(curr != nullptr)
            {
                if(same_bridge_inner(curr->inner, reference)) { return curr; }
                // [registered bridge node] inner_next remains live under bridge_lock_.
                // ^^ curr advances only to a published bridge.
                curr = curr->inner_next;
            }
            return nullptr;
        }
        // Called only while bridge_lock_ proves that bridge->owner is live.
        [[nodiscard]] inline gc_object_status retain_foreign_bridge_locked(extern_bridge const* bridge) const noexcept
        {
            if(bridge->owner == this) { return gc_object_status::ok; }
            auto lease_owner{lease_owner_.lock()};
            if(!lease_owner) { return gc_object_status::invalid_store; }
            auto source_owner{bridge->owner->weak_from_this().lock()};
            if(!source_owner) { return gc_object_status::invalid_reference; }
            return lease_owner->hold(::std::move(source_owner)) ?
                gc_object_status::ok : gc_object_status::out_of_memory;
        }

        [[nodiscard]] static inline ::std::size_t bucket_index(void const* address) noexcept
        {
            auto bits{reinterpret_cast<::std::uintptr_t>(address)};
            bits ^= bits >> 17u;
            bits ^= bits >> 9u;
            return static_cast<::std::size_t>(bits) & (bucket_count - 1uz);
        }
        [[nodiscard]] static inline void* issue_object_token() noexcept
        {
            auto& block{object_tokens_};
            if(block.next == block.end) [[unlikely]]
            {
                auto first{next_object_token_id_.load(::std::memory_order_relaxed)};
                constexpr auto last{(::std::numeric_limits<::std::uintptr_t>::max)()};
                while(true)
                {
                    // Keep end exclusive and representable on 32-bit targets.
                    // Exhaustion fails before an ID can wrap or repeat.
                    if(first > last - object_token_block_size) { return nullptr; }
                    if(next_object_token_id_.compare_exchange_weak(first, first + object_token_block_size,
                        ::std::memory_order_relaxed, ::std::memory_order_relaxed)) { break; }
                }
                block.next = first;
                block.end = first + object_token_block_size;
            }
            // [block.next, block.end) is a uniquely reserved nonzero ID range.
            // ^^ increment the cursor only inside that range; the token is never dereferenced.
            auto const id{block.next++};
            return reinterpret_cast<void*>(id);
        }

        struct object_lock
        {
            object& target;
            explicit object_lock(object& value) noexcept : target(value)
            {
                while(target.mutation_lock.test_and_set(::std::memory_order_acquire)) {}
            }
            ~object_lock() { target.mutation_lock.clear(::std::memory_order_release); }
            object_lock(object_lock const&) = delete;
            object_lock& operator=(object_lock const&) = delete;
        };

        [[nodiscard]] inline type_layout const* checked_type(::std::uint_least32_t index,
                                                               gc_type::composite_kind kind) const noexcept
        {
            if(!valid_ || index >= layout_count_) { return nullptr; }
            // [0, layout_count_) index was checked above; layouts_ owns the full immutable array.
            auto const* layout{layouts_.get() + index};
            return layout->kind == kind ? layout : nullptr;
        }
#if defined(UWVM_EXPERIMENTAL_ARRAY_SET_LOCAL_REF_AUTH) && UWVM_EXPERIMENTAL_ARRAY_SET_LOCAL_REF_AUTH == 1
        [[nodiscard]] inline bool build_local_subtype_intervals() noexcept
        {
            if(layout_count_ == 0uz) { return true; }
            auto constexpr none{(::std::numeric_limits<::std::uint32_t>::max)()};
            // Both temporary links and persistent intervals use two u32 words per type.
            if(layout_count_ > none || layout_count_ >
                (::std::numeric_limits<::std::size_t>::max)() / sizeof(::std::uint32_t) / 2uz)
            { return false; }
            ::std::unique_ptr<local_subtype_interval[]> intervals{
                new(::std::nothrow) local_subtype_interval[layout_count_]{}};
            ::std::unique_ptr<::std::uint32_t[]> links{
                new(::std::nothrow) ::std::uint32_t[layout_count_ * 2uz]};
            if(!intervals || !links) { return false; }
            // [links, links + 2 * layout_count_) is one owned temporary array.
            auto* first_child{links.get()};
            auto* next_sibling{links.get() + layout_count_};
            ::std::fill_n(first_child, layout_count_, none);
            ::std::fill_n(next_sibling, layout_count_, none);
            ::std::uint32_t root{none};
            for(auto index{layout_count_}; index != 0uz;)
            {
                --index;
                auto const parent{layouts_[index].parent};
                if(parent < layout_count_)
                {
                    // Validated single-parent declarations have parent < index.
                    next_sibling[index] = first_child[parent];
                    first_child[parent] = static_cast<::std::uint32_t>(index);
                }
                else
                {
                    next_sibling[index] = root;
                    root = static_cast<::std::uint32_t>(index);
                }
            }
            ::std::uint32_t cursor{root}, ordinal{};
            // Iterative preorder: each node/edge is visited a bounded number of times.
            // Parent links replace a native recursion stack, including long chains.
            while(cursor != none)
            {
                intervals[cursor].begin = ordinal++;
                if(first_child[cursor] != none)
                {
                    cursor = first_child[cursor];
                    continue;
                }
                for(;;)
                {
                    intervals[cursor].end = ordinal;
                    if(next_sibling[cursor] != none)
                    {
                        cursor = next_sibling[cursor];
                        break;
                    }
                    auto const parent{layouts_[cursor].parent};
                    if(parent >= layout_count_) { cursor = none; break; }
                    cursor = static_cast<::std::uint32_t>(parent);
                }
            }
            if(ordinal != layout_count_) { return false; }
            // Publish only a complete map, before registration or any object publication.
            local_subtype_intervals_ = ::std::move(intervals);
            return true;
        }
#endif
        [[nodiscard]] inline bool is_defined_subtype(::std::uint_least32_t actual,
                                                      ::std::uint_least32_t expected) const noexcept
        {
#if defined(UWVM_EXPERIMENTAL_ARRAY_SET_LOCAL_REF_AUTH) && UWVM_EXPERIMENTAL_ARRAY_SET_LOCAL_REF_AUTH == 1
            if(local_subtype_intervals_)
            {
                if(actual >= layout_count_ || expected >= layout_count_) { return false; }
                auto const position{local_subtype_intervals_[actual].begin};
                auto const range{local_subtype_intervals_[expected]};
                return range.begin <= position && position < range.end;
            }
#endif
            // Validation has proved the parent graph acyclic and each parent index strictly smaller.
            while(actual < layout_count_)
            {
                if(actual == expected) { return true; }
                // [0, layout_count_) actual is a live layout index, and each step moves toward the root.
                actual = (layouts_.get() + actual)->parent;
            }
            return false;
        }
        [[nodiscard]] static inline bool canonical_subtype(gc_object_store const* actual_owner,
            ::std::uint_least32_t actual, gc_object_store const* expected_owner,
            ::std::uint_least32_t expected) noexcept
        {
            // A module may declare structurally identical types at different
            // indices. The local parent chain is the fast path, but a miss is
            // not proof of a failed cast: canonical IDs may still be equal or
            // have a canonical subtype relation.
            if(actual_owner == nullptr || expected_owner == nullptr) { return false; }
            if(actual_owner == expected_owner && actual_owner->is_defined_subtype(actual, expected))
            { return true; }
            if(actual >= actual_owner->layout_count_ || expected >= expected_owner->layout_count_ ||
               !actual_owner->canonical_ids_ || !expected_owner->canonical_ids_)
            { return false; }
            auto const actual_id{actual_owner->canonical_ids_[actual]};
            auto const expected_id{expected_owner->canonical_ids_[expected]};
            // Immutable IDs can establish equivalent types without taking the
            // registry lock, including distinct indices in the same module.
            if(actual_id == expected_id) { return true; }
            // Canonical IDs are immutable after construction. The registry's
            // backing vector may grow for a concurrently initialized module,
            // so matches() must remain under its insertion lock.
            canonical_guard guard{};
            return canonical_registry_.matches(actual_id, expected_id);
        }
        [[nodiscard]] inline bool function_reference_matches(gc_reference ref,
            gc_type::core_value_type expected, gc_object_store const* expected_owner) const noexcept
        {
            using ref_kind = ::uwvm2::object::global::wasm_ref_kind;
            using heap = gc_type::abstract_heap_type;
            if(expected.heap.code == static_cast<::std::int_least64_t>(heap::func)) { return true; }
            if(ref.kind == ref_kind::wasm_func || ref.storage.ptr == nullptr ||
               !expected.heap.is_defined() ||
               expected.heap.code > static_cast<::std::int_least64_t>((::std::numeric_limits<::std::uint_least32_t>::max)()))
            { return false; }
            auto const index{static_cast<::std::uint_least32_t>(expected.heap.code)};
            if(expected_owner->checked_type(index, gc_type::composite_kind::function) == nullptr ||
               expected_owner->function_type_match_ == nullptr)
            { return false; }
            // [registered module/function metadata] retained by the native
            // initializer or active invocation. The callback compares the
            // opaque payload to live storage before any payload dereference.
            // ^^ expected_owner is a borrowed store, never advanced or retained.
            return expected_owner->function_type_match_(ref, expected_owner, index);
        }
        [[nodiscard]] inline bool reference_matches(gc_reference ref, gc_type::core_value_type expected,
                                                    gc_object_store const* expected_owner = nullptr) const noexcept
        {
#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1

            using ref_kind = ::uwvm2::object::global::wasm_ref_kind;
            using heap = gc_type::abstract_heap_type;
            if(expected_owner == nullptr) { expected_owner = this; }
            if(ref.kind == ref_kind::wasm_null) { return expected.nullable; }
            if(ref.kind == ref_kind::wasm_i31)
            {
                return expected.heap.code == static_cast<::std::int_least64_t>(heap::i31) ||
                       expected.heap.code == static_cast<::std::int_least64_t>(heap::eq) ||
                       expected.heap.code == static_cast<::std::int_least64_t>(heap::any);
            }
            if(ref.kind == ref_kind::wasm_struct || ref.kind == ref_kind::wasm_array)
            {
                if(ref.storage.ptr == nullptr) { return false; }
                // Never dereference the payload until local or global membership
                // lookup proves publication and retains a foreign owner.
                auto const referenced{checked_aggregate_object(ref, ref.kind == ref_kind::wasm_struct ?
                    gc_type::composite_kind::struct_ : gc_type::composite_kind::array)};
                if(!referenced) { return false; }
                if(expected.heap.is_defined())
                {
                    if(expected.heap.code > static_cast<::std::int_least64_t>((::std::numeric_limits<::std::uint_least32_t>::max)()))
                    { return false; }
                    return canonical_subtype(referenced.owner(), referenced.type_index(),
                        expected_owner, static_cast<::std::uint_least32_t>(expected.heap.code));
                }
                auto const code{expected.heap.code};
                return code == static_cast<::std::int_least64_t>(heap::any) ||
                       code == static_cast<::std::int_least64_t>(heap::eq) ||
                       code == static_cast<::std::int_least64_t>(ref.kind == ref_kind::wasm_struct ? heap::struct_ : heap::array);
            }
            if(ref.kind == ref_kind::wasm_func || ref.kind == ref_kind::wasm_func_imported ||
               ref.kind == ref_kind::wasm_func_defined)
            { return function_reference_matches(ref, expected, expected_owner); }
            if(ref.kind == ref_kind::wasm_extern)
            {
                if(ref.storage.ptr == nullptr) { return false; }
                if(bridge_token_shape(ref.storage.ptr))
                {
                    bridge_guard guard{};
                    auto const* bridge{find_bridge_token_locked(ref.storage.ptr)};
                    if(bridge == nullptr || retain_foreign_bridge_locked(bridge) != gc_object_status::ok)
                    { return false; }
                }
                // any.convert_extern may expose a host-owned opaque external
                // reference as anyref. Its payload is never an eqref or a
                // dereferenceable GC object in this representation.
                return expected.heap.code == static_cast<::std::int_least64_t>(heap::extern_) ||
                       expected.heap.code == static_cast<::std::int_least64_t>(heap::any);
            }
            if(ref.kind == ref_kind::wasm_exn)
            {
                return expected.heap.code == static_cast<::std::int_least64_t>(heap::exn) &&
                       static_cast<bool>(lookup_exn_reference(ref));
            }
            return false;

#else

            using ref_kind = ::uwvm2::object::global::wasm_ref_kind;
            using heap = gc_type::abstract_heap_type;
            if(expected_owner == nullptr) { expected_owner = this; }
            if(ref.kind == ref_kind::wasm_null) { return expected.nullable; }
            if(ref.kind == ref_kind::wasm_i31)
            {
                return expected.heap.code == static_cast<::std::int_least64_t>(heap::i31) ||
                       expected.heap.code == static_cast<::std::int_least64_t>(heap::eq) ||
                       expected.heap.code == static_cast<::std::int_least64_t>(heap::any);
            }
            if(ref.kind == ref_kind::wasm_struct || ref.kind == ref_kind::wasm_array)
            {
                if(ref.storage.ptr == nullptr) { return false; }
                // Never dereference the payload until local or global membership
                // lookup proves publication and retains a foreign owner.
                auto const* referenced{checked_object(ref, ref.kind == ref_kind::wasm_struct ?
                    gc_type::composite_kind::struct_ : gc_type::composite_kind::array)};
                if(referenced == nullptr) { return false; }
                if(expected.heap.is_defined())
                {
                    if(expected.heap.code > static_cast<::std::int_least64_t>((::std::numeric_limits<::std::uint_least32_t>::max)()))
                    { return false; }
                    return canonical_subtype(referenced->owner, referenced->type_index,
                        expected_owner, static_cast<::std::uint_least32_t>(expected.heap.code));
                }
                auto const code{expected.heap.code};
                return code == static_cast<::std::int_least64_t>(heap::any) ||
                       code == static_cast<::std::int_least64_t>(heap::eq) ||
                       code == static_cast<::std::int_least64_t>(ref.kind == ref_kind::wasm_struct ? heap::struct_ : heap::array);
            }
            if(ref.kind == ref_kind::wasm_func || ref.kind == ref_kind::wasm_func_imported ||
               ref.kind == ref_kind::wasm_func_defined)
            { return function_reference_matches(ref, expected, expected_owner); }
            if(ref.kind == ref_kind::wasm_extern)
            {
                if(ref.storage.ptr == nullptr) { return false; }
                if(bridge_token_shape(ref.storage.ptr))
                {
                    bridge_guard guard{};
                    auto const* bridge{find_bridge_token_locked(ref.storage.ptr)};
                    if(bridge == nullptr || retain_foreign_bridge_locked(bridge) != gc_object_status::ok)
                    { return false; }
                }
                // any.convert_extern may expose a host-owned opaque external
                // reference as anyref. Its payload is never an eqref or a
                // dereferenceable GC object in this representation.
                return expected.heap.code == static_cast<::std::int_least64_t>(heap::extern_) ||
                       expected.heap.code == static_cast<::std::int_least64_t>(heap::any);
            }
            if(ref.kind == ref_kind::wasm_exn)
            {
                return expected.heap.code == static_cast<::std::int_least64_t>(heap::exn) &&
                       static_cast<bool>(lookup_exn_reference(ref));
            }
            return false;

#endif
        }
        [[nodiscard]] inline bool value_matches(gc_object_value value, gc_type::storage_type storage,
                                                gc_object_store const* expected_owner = nullptr) const noexcept
        {
            // Numeric kinds are statically established by the validated opcode stack.
            // Only references have a dynamic tag and nullable/heap condition to enforce.
            return storage.packed != gc_type::packed_kind::none ||
                   storage.value.kind != gc_type::value_kind::reference ||
                   reference_matches(value.as<gc_reference>(), storage.value, expected_owner);
        }
#if (defined(UWVM_EXPERIMENTAL_ARRAY_SET_LOCAL_REF_AUTH) && UWVM_EXPERIMENTAL_ARRAY_SET_LOCAL_REF_AUTH == 1) || \
    (defined(UWVM_EXPERIMENTAL_STRUCT_SET_LOCAL_REF_AUTH) && UWVM_EXPERIMENTAL_STRUCT_SET_LOCAL_REF_AUTH == 1)
        // Array and local struct writes may request this same-operation result. Its
        // actual destination is local; all callers still pin the live store
        // and exclude collection/teardown until their immediate access ends.
        // A bool carries no object pointer/root/ticket or persistent authority.
        template<bool CaptureLocal>
        [[nodiscard]] inline bool finish_local_reference_match(bool matched, object const* ordinary,
            gc_object_store const* expected_owner, bool* local_authenticated) const noexcept
        {
            if constexpr(CaptureLocal)
            {
                if(matched && ordinary != nullptr && ordinary->owner == this &&
                   expected_owner == this && local_authenticated != nullptr)
                {
                    // [original acquire membership][successful canonical match]
                    // [safe ] ordinary is a live member of THIS pinned store.
                    // ^^ record only this completed same-operation check; no token
                    // dereference, foreign lease, poll or allocation occurs.
                    *local_authenticated = true;
                }
            }
            return matched;
        }
        template<bool CaptureLocal>
        [[nodiscard]] inline bool reference_matches_with_local_auth(gc_reference ref, gc_type::core_value_type expected,
            gc_object_store const* expected_owner = nullptr, bool* local_authenticated = nullptr) const noexcept
        {
            static_assert(CaptureLocal); // Only the private same-operation result is supported.
            if constexpr(CaptureLocal)
            {
                if(local_authenticated != nullptr)
                {
                    // [caller-owned native output slot] no previous proof survives.
                    // [safe ] clear BEFORE any value/kind/type verification.
                    // ^^ a failed/other-kind match cannot inherit prior success.
                    *local_authenticated = false;
                }
            }
#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1

            using ref_kind = ::uwvm2::object::global::wasm_ref_kind;
            using heap = gc_type::abstract_heap_type;
            if(expected_owner == nullptr) { expected_owner = this; }
            if(ref.kind == ref_kind::wasm_null) { return expected.nullable; }
            if(ref.kind == ref_kind::wasm_i31)
            {
                return expected.heap.code == static_cast<::std::int_least64_t>(heap::i31) ||
                       expected.heap.code == static_cast<::std::int_least64_t>(heap::eq) ||
                       expected.heap.code == static_cast<::std::int_least64_t>(heap::any);
            }
            if(ref.kind == ref_kind::wasm_struct || ref.kind == ref_kind::wasm_array)
            {
                if(ref.storage.ptr == nullptr) { return false; }
                // Never dereference the payload until local or global membership
                // lookup proves publication and retains a foreign owner.
                auto const referenced{checked_aggregate_object(ref, ref.kind == ref_kind::wasm_struct ?
                    gc_type::composite_kind::struct_ : gc_type::composite_kind::array)};
                if(!referenced) { return false; }
                if(expected.heap.is_defined())
                {
                    if(expected.heap.code > static_cast<::std::int_least64_t>((::std::numeric_limits<::std::uint_least32_t>::max)()))
                    { return false; }
                    if constexpr(CaptureLocal)
                    {
                        return finish_local_reference_match<true>(
                            canonical_subtype(referenced.owner(), referenced.type_index(),
                                expected_owner, static_cast<::std::uint_least32_t>(expected.heap.code)),
                            referenced.legacy, expected_owner, local_authenticated);
                    }
                    else
                    {
                        // Keep non-capturing callers on the original direct
                        // result path, including builds that disable inlining.
                        return canonical_subtype(referenced.owner(), referenced.type_index(),
                            expected_owner, static_cast<::std::uint_least32_t>(expected.heap.code));
                    }
                }
                auto const code{expected.heap.code};
                if constexpr(CaptureLocal)
                {
                    return finish_local_reference_match<true>(
                        code == static_cast<::std::int_least64_t>(heap::any) ||
                        code == static_cast<::std::int_least64_t>(heap::eq) ||
                        code == static_cast<::std::int_least64_t>(ref.kind == ref_kind::wasm_struct ? heap::struct_ : heap::array),
                        referenced.legacy, expected_owner, local_authenticated);
                }
                else
                {
                    return code == static_cast<::std::int_least64_t>(heap::any) ||
                           code == static_cast<::std::int_least64_t>(heap::eq) ||
                           code == static_cast<::std::int_least64_t>(ref.kind == ref_kind::wasm_struct ? heap::struct_ : heap::array);
                }
            }
            if(ref.kind == ref_kind::wasm_func || ref.kind == ref_kind::wasm_func_imported ||
               ref.kind == ref_kind::wasm_func_defined)
            { return function_reference_matches(ref, expected, expected_owner); }
            if(ref.kind == ref_kind::wasm_extern)
            {
                if(ref.storage.ptr == nullptr) { return false; }
                if(bridge_token_shape(ref.storage.ptr))
                {
                    bridge_guard guard{};
                    auto const* bridge{find_bridge_token_locked(ref.storage.ptr)};
                    if(bridge == nullptr || retain_foreign_bridge_locked(bridge) != gc_object_status::ok)
                    { return false; }
                }
                // any.convert_extern may expose a host-owned opaque external
                // reference as anyref. Its payload is never an eqref or a
                // dereferenceable GC object in this representation.
                return expected.heap.code == static_cast<::std::int_least64_t>(heap::extern_) ||
                       expected.heap.code == static_cast<::std::int_least64_t>(heap::any);
            }
            if(ref.kind == ref_kind::wasm_exn)
            {
                return expected.heap.code == static_cast<::std::int_least64_t>(heap::exn) &&
                       static_cast<bool>(lookup_exn_reference(ref));
            }
            return false;
        
#else

            using ref_kind = ::uwvm2::object::global::wasm_ref_kind;
            using heap = gc_type::abstract_heap_type;
            if(expected_owner == nullptr) { expected_owner = this; }
            if(ref.kind == ref_kind::wasm_null) { return expected.nullable; }
            if(ref.kind == ref_kind::wasm_i31)
            {
                return expected.heap.code == static_cast<::std::int_least64_t>(heap::i31) ||
                       expected.heap.code == static_cast<::std::int_least64_t>(heap::eq) ||
                       expected.heap.code == static_cast<::std::int_least64_t>(heap::any);
            }
            if(ref.kind == ref_kind::wasm_struct || ref.kind == ref_kind::wasm_array)
            {
                if(ref.storage.ptr == nullptr) { return false; }
                // Never dereference the payload until local or global membership
                // lookup proves publication and retains a foreign owner.
                auto const* referenced{checked_object(ref, ref.kind == ref_kind::wasm_struct ?
                    gc_type::composite_kind::struct_ : gc_type::composite_kind::array)};
                if(referenced == nullptr) { return false; }
                if(expected.heap.is_defined())
                {
                    if(expected.heap.code > static_cast<::std::int_least64_t>((::std::numeric_limits<::std::uint_least32_t>::max)()))
                    { return false; }
                    if constexpr(CaptureLocal)
                    {
                        return finish_local_reference_match<true>(
                            canonical_subtype(referenced->owner, referenced->type_index,
                                expected_owner, static_cast<::std::uint_least32_t>(expected.heap.code)),
                            referenced, expected_owner, local_authenticated);
                    }
                    else
                    {
                        // Keep non-capturing callers on the original direct
                        // result path, including builds that disable inlining.
                        return canonical_subtype(referenced->owner, referenced->type_index,
                            expected_owner, static_cast<::std::uint_least32_t>(expected.heap.code));
                    }
                }
                auto const code{expected.heap.code};
                if constexpr(CaptureLocal)
                {
                    return finish_local_reference_match<true>(
                        code == static_cast<::std::int_least64_t>(heap::any) ||
                        code == static_cast<::std::int_least64_t>(heap::eq) ||
                        code == static_cast<::std::int_least64_t>(ref.kind == ref_kind::wasm_struct ? heap::struct_ : heap::array),
                        referenced, expected_owner, local_authenticated);
                }
                else
                {
                    return code == static_cast<::std::int_least64_t>(heap::any) ||
                           code == static_cast<::std::int_least64_t>(heap::eq) ||
                           code == static_cast<::std::int_least64_t>(ref.kind == ref_kind::wasm_struct ? heap::struct_ : heap::array);
                }
            }
            if(ref.kind == ref_kind::wasm_func || ref.kind == ref_kind::wasm_func_imported ||
               ref.kind == ref_kind::wasm_func_defined)
            { return function_reference_matches(ref, expected, expected_owner); }
            if(ref.kind == ref_kind::wasm_extern)
            {
                if(ref.storage.ptr == nullptr) { return false; }
                if(bridge_token_shape(ref.storage.ptr))
                {
                    bridge_guard guard{};
                    auto const* bridge{find_bridge_token_locked(ref.storage.ptr)};
                    if(bridge == nullptr || retain_foreign_bridge_locked(bridge) != gc_object_status::ok)
                    { return false; }
                }
                // any.convert_extern may expose a host-owned opaque external
                // reference as anyref. Its payload is never an eqref or a
                // dereferenceable GC object in this representation.
                return expected.heap.code == static_cast<::std::int_least64_t>(heap::extern_) ||
                       expected.heap.code == static_cast<::std::int_least64_t>(heap::any);
            }
            if(ref.kind == ref_kind::wasm_exn)
            {
                return expected.heap.code == static_cast<::std::int_least64_t>(heap::exn) &&
                       static_cast<bool>(lookup_exn_reference(ref));
            }
            return false;
        
#endif
        }
        template<bool CaptureLocal>
        [[nodiscard]] inline bool value_matches_with_local_auth(gc_object_value value, gc_type::storage_type storage,
            gc_object_store const* expected_owner = nullptr, bool* local_authenticated = nullptr) const noexcept
        {
            static_assert(CaptureLocal); // Only the private same-operation result is supported.
            if constexpr(CaptureLocal)
            {
                if(local_authenticated != nullptr)
                {
                    // [native output slot] also clear for non-reference inputs.
                    // [safe ] no numeric/kind-short-circuit inherits old authority.
                    // ^^ clear this operation's result before verification begins.
                    *local_authenticated = false;
                }
            }
            // Numeric kinds are statically established by the validated opcode stack.
            // Only references have a dynamic tag and nullable/heap condition to enforce.
            return storage.packed != gc_type::packed_kind::none ||
                   storage.value.kind != gc_type::value_kind::reference ||
                   reference_matches_with_local_auth<CaptureLocal>(value.as<gc_reference>(), storage.value,
                       expected_owner, local_authenticated);
        }
#endif
        [[nodiscard]] inline gc_object_status retain_embedded_reference(object& destination,
            gc_object_value value, gc_type::storage_type storage) const noexcept
        {
#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1

            if(storage.packed != gc_type::packed_kind::none ||
               storage.value.kind != gc_type::value_kind::reference)
            { return gc_object_status::ok; }
            auto const reference{value.as<gc_reference>()};
            using ref_kind = ::uwvm2::object::global::wasm_ref_kind;
            if(reference.kind == ref_kind::wasm_struct || reference.kind == ref_kind::wasm_array)
            {
                // Membership must be proved before reading the originating
                // object's owner. The current module receives its own lease in
                // the foreign slow path; this object needs an independent lease
                // because the current module may unload first.
                auto const source{checked_aggregate_object(reference, reference.kind == ref_kind::wasm_struct ?
                    gc_type::composite_kind::struct_ : gc_type::composite_kind::array)};
                if(!source) { return gc_object_status::invalid_reference; }
                if(source.owner() == destination.owner) { return gc_object_status::ok; }
                auto owner{source.owner()->weak_from_this().lock()};
                if(!owner) { return gc_object_status::invalid_reference; }
                return destination.value_leases.hold(::std::move(owner)) ?
                    gc_object_status::ok : gc_object_status::out_of_memory;
            }
            if(reference.kind == ref_kind::wasm_extern && bridge_token_shape(reference.storage.ptr))
            {
                ::std::shared_ptr<gc_object_store const> owner{};
                {
                    bridge_guard guard{};
                    auto const* bridge{find_bridge_token_locked(reference.storage.ptr)};
                    if(bridge == nullptr) { return gc_object_status::invalid_reference; }
                    if(bridge->owner == destination.owner) { return gc_object_status::ok; }
                    owner = bridge->owner->weak_from_this().lock();
                }
                if(!owner) { return gc_object_status::invalid_reference; }
                return destination.value_leases.hold(::std::move(owner)) ?
                    gc_object_status::ok : gc_object_status::out_of_memory;
            }
            if(reference.kind == ref_kind::wasm_exn)
            {
                auto const status{destination.owner->retain_exn_reference(reference)};
                if(status != gc_object_status::ok) { return status; }
                ::std::shared_ptr<gc_object_store const> origin{};
                {
                    exn_guard guard{};
                    auto const* entry{find_exn_locked(reference.storage.ptr)};
                    if(entry == nullptr) { return gc_object_status::invalid_reference; }
                    origin = entry->owner.lock();
                }
                if(!origin) { return gc_object_status::invalid_reference; }
                if(origin.get() == destination.owner) { return gc_object_status::ok; }
                // The receiving object can outlive its module's lease list.
                // Its own lease keeps an embedded foreign exn's arena alive.
                return destination.value_leases.hold(::std::move(origin)) ?
                    gc_object_status::ok : gc_object_status::out_of_memory;
            }
            return gc_object_status::ok;
        
#else

            if(storage.packed != gc_type::packed_kind::none ||
               storage.value.kind != gc_type::value_kind::reference)
            { return gc_object_status::ok; }
            auto const reference{value.as<gc_reference>()};
            using ref_kind = ::uwvm2::object::global::wasm_ref_kind;
            if(reference.kind == ref_kind::wasm_struct || reference.kind == ref_kind::wasm_array)
            {
                // Membership must be proved before reading the originating
                // object's owner. The current module receives its own lease in
                // the foreign slow path; this object needs an independent lease
                // because the current module may unload first.
                auto const* source{checked_object(reference, reference.kind == ref_kind::wasm_struct ?
                    gc_type::composite_kind::struct_ : gc_type::composite_kind::array)};
                if(source == nullptr) { return gc_object_status::invalid_reference; }
                if(source->owner == destination.owner) { return gc_object_status::ok; }
                auto owner{source->owner->weak_from_this().lock()};
                if(!owner) { return gc_object_status::invalid_reference; }
                return destination.value_leases.hold(::std::move(owner)) ?
                    gc_object_status::ok : gc_object_status::out_of_memory;
            }
            if(reference.kind == ref_kind::wasm_extern && bridge_token_shape(reference.storage.ptr))
            {
                ::std::shared_ptr<gc_object_store const> owner{};
                {
                    bridge_guard guard{};
                    auto const* bridge{find_bridge_token_locked(reference.storage.ptr)};
                    if(bridge == nullptr) { return gc_object_status::invalid_reference; }
                    if(bridge->owner == destination.owner) { return gc_object_status::ok; }
                    owner = bridge->owner->weak_from_this().lock();
                }
                if(!owner) { return gc_object_status::invalid_reference; }
                return destination.value_leases.hold(::std::move(owner)) ?
                    gc_object_status::ok : gc_object_status::out_of_memory;
            }
            if(reference.kind == ref_kind::wasm_exn)
            {
                auto const status{destination.owner->retain_exn_reference(reference)};
                if(status != gc_object_status::ok) { return status; }
                ::std::shared_ptr<gc_object_store const> origin{};
                {
                    exn_guard guard{};
                    auto const* entry{find_exn_locked(reference.storage.ptr)};
                    if(entry == nullptr) { return gc_object_status::invalid_reference; }
                    origin = entry->owner.lock();
                }
                if(!origin) { return gc_object_status::invalid_reference; }
                if(origin.get() == destination.owner) { return gc_object_status::ok; }
                // The receiving object can outlive its module's lease list.
                // Its own lease keeps an embedded foreign exn's arena alive.
                return destination.value_leases.hold(::std::move(origin)) ?
                    gc_object_status::ok : gc_object_status::out_of_memory;
            }
            return gc_object_status::ok;
        
#endif
        }
        [[nodiscard]] static inline gc_object_value pack(gc_object_value value,
                                                         gc_type::packed_kind packed) noexcept
        {
            if(packed == gc_type::packed_kind::i8) { return gc_object_value::i32(value.as<::std::uint32_t>() & 0xffu); }
            if(packed == gc_type::packed_kind::i16) { return gc_object_value::i32(value.as<::std::uint32_t>() & 0xffffu); }
            return value;
        }
        [[nodiscard]] static inline gc_object_value unpack(gc_object_value value,
                                                           gc_type::packed_kind packed,
                                                           bool sign_extend) noexcept
        {
            if(packed == gc_type::packed_kind::i8)
            {
                auto const bits{static_cast<::std::uint8_t>(value.as<::std::uint32_t>())};
                return gc_object_value::i32(sign_extend ? static_cast<::std::uint32_t>(static_cast<::std::int8_t>(bits)) : bits);
            }
            if(packed == gc_type::packed_kind::i16)
            {
                auto const bits{static_cast<::std::uint16_t>(value.as<::std::uint32_t>())};
                return gc_object_value::i32(sign_extend ? static_cast<::std::uint32_t>(static_cast<::std::int16_t>(bits)) : bits);
            }
            return value;
        }
        [[nodiscard]] static inline gc_object_value default_value(gc_type::storage_type storage) noexcept
        {
            if(storage.packed != gc_type::packed_kind::none) { return gc_object_value::i32(0u); }
            gc_object_value result{};
            if(storage.value.kind == gc_type::value_kind::reference)
            {
                gc_reference null_ref{};
                null_ref.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_null;
                result = gc_object_value::reference(null_ref);
            }
            return result;
        }
#if defined(__GNUC__) || defined(__clang__)
        [[gnu::noinline]]
#elif defined(_MSC_VER)
        __declspec(noinline)
#endif
        [[nodiscard]] inline gc_object_status initialize_default_array_payload(
            ::std::uint_least32_t type_index, ::std::size_t length,
            gc_type::storage_type storage, gc_reference& result) noexcept
        {
            object* fresh{};
            auto const status{allocate(type_index, gc_type::composite_kind::array, length, fresh)};
            if(status != gc_object_status::ok) { return status; }
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
            if(fresh->numeric_array_)
            {
                // The allocator checked the complete length*width byte tail.
                // Numeric defaults have zero bits on every native byte order;
                // initialize this unpublished byte payload exactly once.
                if(length != 0uz)
                { ::std::memset(fresh->values.byte_data(), 0, length * numeric_array_storage_width(storage)); }
            }
            else
#endif
            if(field_is_reference(storage))
            {
                // A carrier's value-initialized bytes are not a portable null
                // reference representation. Construct the actual native null;
                // it creates no foreign-object or exception lease.
                if(length != 0uz)
                {
                    auto const native_null{default_value(storage)};
                    gc_object_value const cleared_carrier{};
                    // allocate already value-initialized EVERY carrier. Compare
                    // the constructed native null's complete representation to
                    // that exact initialized byte pattern; never assume pointer
                    // null bits or byte order. Equal representations need no
                    // second complete array write. Nonzero/padded representations
                    // retain the bounded libc copy path below.
                    if(::std::memcmp(native_null.bits.data(), cleared_carrier.bits.data(),
                                     sizeof(gc_object_value)) != 0)
                    {
                        auto* const destination{fresh->values.get()};
                        destination[0uz] = native_null;
                        for(::std::size_t initialized{1uz}; initialized < length;)
                        {
                            auto const remaining{length - initialized};
                            auto const copy{initialized < remaining ? initialized : remaining};
                            // Both complete carrier ranges are live and bounded by
                            // allocate; copy<=initialized proves they do not overlap.
                            // Copy the real native null representation through libc.
                            ::std::memcpy(destination + initialized, destination, copy * sizeof(gc_object_value));
                            initialized += copy;
                        }
                    }
                }
            }
            // Legacy numeric carriers were already value-initialized by
            // allocate. Do not clear or fill that complete array a second time.
            return publish(fresh, result);
        }
#if defined(UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA == 1
        // Both compact-enabled and default legacy collection use this exact
        // scanner. The caller already authenticated header/owner, canonical
        // layout and complete carrier extent, and checked matches_shape. Only
        // this stopped span may consume the immutable native classification.
        template<typename Visitor>
        [[nodiscard]] static inline bool visit_precise_reference_fields(
            object const& current, type_layout const& layout, Visitor&& visitor) noexcept
        {
            if(layout.trace.numeric_leaf()) { return true; }
            if(current.kind == gc_type::composite_kind::array)
            {
                if(!layout.trace.array_elements_are_references()) { return false; }
                // The stopped object's pointer and extent stay immutable even
                // when the visitor updates marks or collection-local caches.
                // Snapshot them so the uncommon visitor cannot force a header
                // reload for every immediate reference in the hot scan.
                auto const* values{current.values.get()};
                auto const length{current.length};
                ::std::size_t index{};
#if defined(UWVM_EXPERIMENTAL_GENERAL_GC_SLAB) && UWVM_EXPERIMENTAL_GENERAL_GC_SLAB == 1
                auto const non_object{[](gc_reference reference) noexcept
                {
                    return reference.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_null ||
                           reference.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_i31;
                }};
                // Batch only the leading null/i31 run. Bitwise bool AND reads
                // each of the eight checked, live slots without short-circuit
                // branches. No pointer representation, alignment, tag number
                // or architecture-specific vector instruction is assumed.
                // At the first other kind, resume the original ordered visitor
                // loop from this block's beginning; no aggregate is skipped.
                while(length - index >= 8uz)
                {
                    auto const all_non_object{
                        non_object(values[index].as<gc_reference>()) &
                        non_object(values[index + 1uz].as<gc_reference>()) &
                        non_object(values[index + 2uz].as<gc_reference>()) &
                        non_object(values[index + 3uz].as<gc_reference>()) &
                        non_object(values[index + 4uz].as<gc_reference>()) &
                        non_object(values[index + 5uz].as<gc_reference>()) &
                        non_object(values[index + 6uz].as<gc_reference>()) &
                        non_object(values[index + 7uz].as<gc_reference>())};
                    if(!all_non_object) { break; }
                    index += 8uz;
                }
#endif
                for(; index != length; ++index)
                {
                    // The caller authenticated this stopped array's owner,
                    // canonical reference layout and complete carrier extent.
                    // Read every live slot. Both collection visitors accept
                    // null and immediate i31 unconditionally as non_object;
                    // neither has a GC target, mark or semantic lease. Native
                    // pointer bytes are not assumed zero.
                    auto const reference{values[index].as<gc_reference>()};
#if defined(UWVM_EXPERIMENTAL_GENERAL_GC_SLAB) && UWVM_EXPERIMENTAL_GENERAL_GC_SLAB == 1
                    if(reference.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_null ||
                       reference.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_i31) { continue; }
#endif
                    // Every other kind reaches the original visitor, including
                    // all aggregate/exception/extern/unknown tokens.
                    if(!visitor(reference)) { return false; }
                }
            }
            else if(current.kind == gc_type::composite_kind::struct_)
            {
#if defined(UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA == 1
                return layout.trace.visit_struct_reference_fields(layout.field_count,
                    [&](::std::size_t field) noexcept
                    {
                        if(field >= current.length) { return false; }
                        // [values,values+length) are actual initialized owned
                        // carriers. No allocation, callback or epoch change
                        // intervenes after header/extent authentication.
                        // [safe                  ] field < length before the
                        // payload is read; membership remains visitor's job.
                        return visitor(current.values[field].as<gc_reference>());
                    });
#else
                auto const count{layout.trace.struct_reference_count()};
                for(::std::size_t position{}; position != count; ++position)
                {
                    ::std::size_t field{};
                    if(!layout.trace.reference_field_at(position, layout.field_count, field) ||
                       field >= current.length) { return false; }
                    // [values,values+length) initialized owned carriers.
                    // [safe                         ] field < length after
                    // bounded metadata lookup; construction classified this
                    // exact field as unpacked reference in the immutable
                    // canonical layout. No numeric carrier is reinterpreted.
                    if(!visitor(current.values[field].as<gc_reference>())) { return false; }
                }
#endif
            }
            else { return false; }
            return true;
        }
#endif
        [[nodiscard]] static inline gc_object_value data_element_value(
            ::std::byte const* source, gc_type::storage_type storage) noexcept
        {
            auto const width{gc_object_value::field_storage_size(storage)};
            if(width == 16uz)
            {
                gc_object_value result{};
                // [source, source + 16) was checked against the segment before this call.
                // v128 is the raw Wasm byte carrier. Shared SIMD lane helpers
                // interpret these bytes in little-endian order on BE hosts;
                // decoding into native uint64 lanes here would flip each lane.
                ::std::memcpy(result.bits.data(), source, 16uz);
                return result;
            }
            if(width != 1uz && width != 2uz && width != 4uz && width != 8uz)
            { ::fast_io::fast_terminate(); } // Canonical numeric/packed layouts prove this invariant.
            // [source,source+width) is one complete retained immutable segment
            // element. array_new_data/array_init_data checked the whole byte
            // extent by subtraction before this call; width is now 1/2/4/8.
            // [safe                                      ] unsafe (one-past)
            // ^^ first aliases its object bytes; last advances ONLY over the
            // already proved complete field, with no unaligned typed load.
            auto const* first{reinterpret_cast<char const*>(source)};
            auto const* last{first + width};
            ::std::uint64_t bits{};
            auto const decode{[&](auto getter) noexcept
            {
                auto const parsed{::fast_io::parse_by_scan(first, last, getter)};
                // Exact fixed-width fields have no decimal/whitespace conversion.
                // Full consumption preserves the existing native numeric carrier;
                // f32/f64 bits, including signalling NaNs, are never evaluated.
                if(parsed.code != ::fast_io::parse_code::ok || parsed.iter != last)
                { ::fast_io::fast_terminate(); }
            }};
            switch(width)
            {
                case 1uz: decode(::fast_io::mnp::le_get<8>(bits)); break;
                case 2uz: decode(::fast_io::mnp::le_get<16>(bits)); break;
                case 4uz: decode(::fast_io::mnp::le_get<32>(bits)); break;
                case 8uz: decode(::fast_io::mnp::le_get<64>(bits)); break;
                default: ::fast_io::fast_terminate();
            }
            return width <= 4uz ? gc_object_value::i32(static_cast<::std::uint32_t>(bits)) :
                                  gc_object_value::i64(bits);
        }
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
# include "numeric_array_payload.h"
#endif
#if defined(UWVM_EXPERIMENTAL_GENERAL_GC_SLAB) && UWVM_EXPERIMENTAL_GENERAL_GC_SLAB == 1
# include "general_gc_slab.h"
#endif
        // A complete initialized source representation starts a REAL carrier
        // array lifetime through std::memcpy [cstring.syn]/3, including its
        // implicit-lifetime std::array<byte,16> subobjects [intro.object]/13.
        // Use memcpy's returned suitable-created-object pointer, not a cast
        // of uninitialized raw storage. The enclosing byte array remains the
        // original allocation owner. Default constructors use placement new.
        [[nodiscard]] static inline gc_object_value* initialize_carrier_tail(
            ::std::byte* tail, ::std::size_t length, ::std::byte const* complete_source) noexcept
        {
            static_assert(::std::is_aggregate_v<gc_object_value>);
            static_assert(::std::is_standard_layout_v<gc_object_value>);
            static_assert(::std::is_trivially_copyable_v<gc_object_value>);
            static_assert(::std::is_trivially_destructible_v<gc_object_value>);
            // Callers checked alignment and length*sizeof(carrier), exclude
            // overlap, and never pass a partial/uninitialized source extent.
            if(complete_source != nullptr)
            { return static_cast<gc_object_value*>(::std::memcpy(tail, complete_source, length * sizeof(gc_object_value))); }
            return ::new(static_cast<void*>(tail)) gc_object_value[length]{};
        }
        [[nodiscard]] inline gc_object_status allocate(::std::uint_least32_t type_index,
                                                        gc_type::composite_kind kind,
                                                        ::std::size_t length,
                                                        object*& result,
                                                        ::std::byte const* complete_source = nullptr) noexcept
        {
            static_assert(::std::is_nothrow_default_constructible_v<object>);
            auto constexpr maximum_bytes{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())};
            static_assert(object_value_offset <= maximum_bytes);
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
            auto const raw_width{kind == gc_type::composite_kind::array ?
                numeric_array_width(checked_type(type_index, kind)) : 0uz};
            auto const element_bytes{raw_width != 0uz ? raw_width : sizeof(gc_object_value)};
            if(length > (maximum_bytes - object_value_offset) / element_bytes)
            { return gc_object_status::size_overflow; }
            auto const bytes{object_value_offset + length * element_bytes};
#else
            if(length > (maximum_bytes - object_value_offset) / sizeof(gc_object_value))
            { return gc_object_status::size_overflow; }
            auto const bytes{object_value_offset + length * sizeof(gc_object_value)};
#endif
#if defined(UWVM_EXPERIMENTAL_GENERAL_GC_SLAB) && UWVM_EXPERIMENTAL_GENERAL_GC_SLAB == 1 && !defined(UWVM2TEST_GC_NUMERIC_SLAB_RESERVED_PROBE)
            auto const payload_bytes{bytes - object_value_offset};
            if(general_slab_eligible(type_index,kind,length,payload_bytes))
            { return allocate_general_slab(type_index,kind,length,payload_bytes,
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
                raw_width != 0uz,
#else
                false,
#endif
                result, complete_source); }
#endif
#if defined(UWVM_EXPERIMENTAL_SINGLE_LOCK_NUMERIC_SLAB) && UWVM_EXPERIMENTAL_SINGLE_LOCK_NUMERIC_SLAB == 1 || \
    (defined(UWVM_EXPERIMENTAL_GENERAL_GC_SLAB) && UWVM_EXPERIMENTAL_GENERAL_GC_SLAB == 1)
#if !defined(UWVM2TEST_GC_NUMERIC_SLAB_RESERVED_PROBE)
            // A defined observer (including value zero) keeps the ORIGINAL
            // reserve-unlock-callback-construct-commit path. Never call it held.
            if(numeric_slab_eligible(type_index, kind, length))
            { return allocate_numeric_slab_single_lock(type_index, length, result, complete_source); }
#endif
#endif
            slot_reservation reservation{};
            if(numeric_slab_eligible(type_index, kind, length))
            {
                auto const status{reserve_numeric_slot(length, reservation)};
                if(status != gc_object_status::ok) { return status; }
#if defined(UWVM2TEST_GC_NUMERIC_SLAB_RESERVED_PROBE)
                gc_numeric_slab_reserved_probe(this);
#endif
            }
            else
            {
                auto* backing{new(::std::nothrow) ::std::byte[bytes]};
                if(backing == nullptr) { return gc_object_status::out_of_memory; }
                // [live original byte-array storage][envelope][aligned tail]
                // ^^ placement creates the union/envelope but no object member;
                // metadata retains the original array-new result for delete[].
                auto* envelope{::new(static_cast<void*>(backing)) allocation_header{}};
                envelope->metadata.owner_or_storage = backing;
                reservation = {envelope, backing};
            }
            // [live envelope with inactive union][exclusive reserved slot]
            // ^^ placement activates exactly its named object member. Header
            // pointer conversions later use the real containing objects.
            auto* fresh{::new(static_cast<void*>(::std::addressof(reservation.envelope->payload.instance))) object{}};
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
            fresh->numeric_array_ = raw_width != 0uz;
#endif
            if(length != 0uz)
            {
                // [original byte array][live envelope][aligned whole tail] end
                // ^^ tail is derived from the retained array element pointer,
                // never by arithmetic on the object or envelope representation.
                auto* tail{reservation.original_array_slot + object_value_offset};
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
                if(fresh->numeric_array_)
                {
                    // [original byte-array storage][length*raw_width tail]
                    // [safe ] the bounded multiplication above proves every
                    // element. Array constructors initialize the entire tail
                    // before publishing a token. No carrier pointer is made.
                    fresh->values.reset_bytes(tail);
                }
                else
                {
#endif
                auto* initialized_values{initialize_carrier_tail(tail, length, complete_source)};
                if(initialized_values == nullptr)
                { destroy_object(fresh); return gc_object_status::size_overflow; }
                fresh->values.reset(initialized_values);
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
                }
#endif
            }
            fresh->owner = this;
            fresh->kind = kind;
            fresh->type_index = type_index;
            fresh->length = length;
            commit_reserved_slot(reservation.envelope->metadata);
            result = fresh;
            return gc_object_status::ok;
        }
        [[nodiscard]] inline gc_object_status publish(object* fresh, gc_reference& result) noexcept
        {
            // [fresh owns all initialized slots] reserve its nonrecycling key
            // before exposing the object in any index or guest reference.
            auto const token{issue_object_token()};
            if(token == nullptr) { destroy_object(fresh); return gc_object_status::size_overflow; }
            // [fresh unpublished, token uniquely reserved]
            // ^^ assign the opaque key before release-publishing the object.
            fresh->token = token;
            return publish_reserved_object(fresh,token,result);
        }
        // Same unchanged ordinary hot publication body, with an already issued
        // unique token. Only private staging may bypass token issuance; no extra
        // dynamic checkpoint guard/load is inserted in the ordinary body.
        [[nodiscard]] inline gc_object_status publish_reserved_object(object* fresh,
            void* token,gc_reference& result) noexcept
        {
#if defined(UWVM_EXPERIMENTAL_SINGLE_CAS_GC_PUBLICATION) && UWVM_EXPERIMENTAL_SINGLE_CAS_GC_PUBLICATION == 1
            {
                // Every candidate publisher of THIS bucket takes this existing
                // global stripe. All translation units must use one policy:
                // mixing an old lock-free local CAS publisher with this store
                // publication is forbidden. The actual managed stop/admission
                // protocol, or the trusted native caller's equivalent exclusive
                // contract, excludes collection and same-store teardown here;
                // cohort_guard alone does not stop native readers/publishers.
                auto const index{bucket_index(token)};
                global_guard guard{index};
                // [0,bucket_count) index is masked by bucket_index above.
                // [safe                         ] select owned atomic/native
                // head slots only; a token is never a dereferenceable pointer.
                auto& bucket{membership_buckets_[index]};
                auto& global_head{global_buckets_[index]};
                // [existing release-published local chain][held writer stripe]
                // [safe ] the preceding stripe unlock synchronizes this load.
                // ^^ local_head is null or a live native member, not guest data.
                auto* local_head{bucket.load(::std::memory_order_relaxed)};
                // [fully initialized private fresh][live local predecessor]
                // [safe ] fresh is still undiscoverable in every public index.
                // ^^ initialize its bounded native link before first visibility.
                fresh->hash_next = local_head;
                // [private fresh][registered global predecessor or null]
                // [safe ] this same stripe excludes global unlink and insertion.
                // ^^ bind a real stable native link before the list can expose it.
                fresh->global_next = global_head;
                // [store-owned release-published object chain or null]
                // [safe ] no pointee is read through this relaxed head load.
                // ^^ head is an actual atomic list result, never a token cast.
                auto* head{objects_.load(::std::memory_order_relaxed)};
                do
                {
                    // [private fresh][current real store list head or null]
                    // [safe ] a failed CAS leaves fresh unpublished in that list.
                    // ^^ only this private next link changes before each retry.
                    fresh->next = head;
                }
                // [complete fresh header/links/payload][real atomic head slot]
                // [safe ] success release-publishes all initialized fields;
                // failure updates head only to another real list head, which is
                // not dereferenced and is still protected from collection.
                // ^^ CAS retries never modify any previously published header.
                while(!objects_.compare_exchange_weak(head, fresh, ::std::memory_order_release,
                                                       ::std::memory_order_relaxed));
                // [complete store-listed fresh][protected global head slot]
                // [safe ] foreign readers require this same stripe and can only
                // observe this insertion after the entire block has completed.
                // ^^ publish the global native head before returning any token.
                global_head = fresh;
                // [fully initialized local chain][owned local atomic head slot]
                // [safe ] writer serialization replaces ONLY the redundant CAS.
                // The unchanged reader acquire observes all fields and links.
                // ^^ release-store a real header; stale/forged tokens still need
                // the complete actual membership/type/foreign-ownership check.
                bucket.store(fresh, ::std::memory_order_release);
            }
#else
            auto& bucket{membership_buckets_[bucket_index(token)]};
            auto* old_bucket_head{bucket.load(::std::memory_order_relaxed)};
            do { fresh->hash_next = old_bucket_head; }
            while(!bucket.compare_exchange_weak(old_bucket_head, fresh, ::std::memory_order_release,
                                                ::std::memory_order_relaxed));
            auto* head{objects_.load(::std::memory_order_relaxed)};
            do { fresh->next = head; }
            while(!objects_.compare_exchange_weak(head, fresh, ::std::memory_order_release,
                                                  ::std::memory_order_relaxed));
            {
                auto const index{bucket_index(token)};
                global_guard guard{index};
                auto& global_head{global_buckets_[index]};
                // [fully initialized fresh][registered global chain]
                // ^^ publish the object in the foreign slow-path index before
                // returning its guest-visible reference.
                fresh->global_next = global_head;
                global_head = fresh;
            }
#endif
            // [three published indexes] readers can now resolve this exact token.
            // ^^ guest-visible pointer field carries an integer identity, never an object address.
            result.storage.ptr = token;
            result.kind = fresh->kind == gc_type::composite_kind::struct_ ?
                          ::uwvm2::object::global::wasm_ref_kind::wasm_struct :
                          ::uwvm2::object::global::wasm_ref_kind::wasm_array;
            return gc_object_status::ok;
        }
        // This path is entered only after a failed local membership lookup.
        // Keeping the global stripe and shared_ptr promotion out of the inlined
        // local path preserves local struct/array read throughput.
#if defined(__GNUC__) || defined(__clang__)
        [[gnu::noinline, gnu::cold]]
#elif defined(_MSC_VER)
        __declspec(noinline)
#endif
        [[nodiscard]] object* checked_foreign_object(gc_reference reference,
                                                     gc_type::composite_kind kind) const noexcept
        {
            object* foreign{};
            ::std::shared_ptr<gc_object_store const> owner{};
            {
                auto const index{bucket_index(reference.storage.ptr)};
                global_guard guard{index};
                auto* node{global_buckets_[index]};
                while(node != nullptr)
                {
                    if(node->token == reference.storage.ptr)
                    {
                        if(node->kind != kind) { return nullptr; }
                        // The same bucket stripe excludes teardown while weak
                        // promotion pins the published node's owner after unlock.
                        owner = node->owner->weak_from_this().lock();
                        foreign = node;
                        break;
                    }
                    // [registered global chain] no node can be removed under lock.
                    // ^^ node advances only through published object links.
                    node = node->global_next;
                }
            }
            if(!owner || !foreign) { return nullptr; }
            if(owner.get() == this) { return foreign; }
            auto lease_owner{lease_owner_.lock()};
            if(!lease_owner || !lease_owner->hold(::std::move(owner))) { return nullptr; }
            return foreign;
        }
        [[nodiscard]] inline object* checked_local_object(gc_reference reference,
                                                           gc_type::composite_kind kind) const noexcept
        {
            auto const expected{kind == gc_type::composite_kind::struct_ ?
                ::uwvm2::object::global::wasm_ref_kind::wasm_struct :
                ::uwvm2::object::global::wasm_ref_kind::wasm_array};
            if(reference.kind != expected || reference.storage.ptr == nullptr) { return nullptr; }
            if(!membership_buckets_) { return nullptr; }
            {
                // [bucket chain published with release CAS] acquire makes every node and
                // hash_next pointer visible; the untrusted payload is only compared as a key.
                auto* node{membership_buckets_[bucket_index(reference.storage.ptr)].load(::std::memory_order_acquire)};
                while(node != nullptr)
                {
                    if(node->token == reference.storage.ptr)
                    { return node->kind == kind ? node : nullptr; }
                    // [published local bucket chain] the next link is initialized and stable.
                    // ^^ node advances only to another member of this live store.
                    node = node->hash_next;
                }
            }
            return nullptr;
        }
#if defined(UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP) && UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP == 1
# include "collection_local_membership.h"
#endif
        [[nodiscard]] inline object* checked_object(gc_reference reference,
                                                     gc_type::composite_kind kind) const noexcept
        {
            if(auto* local{checked_local_object(reference, kind)}; local != nullptr) [[likely]] { return local; }
            if(reference.storage.ptr == nullptr ||
               reference.kind != (kind == gc_type::composite_kind::struct_ ?
                   ::uwvm2::object::global::wasm_ref_kind::wasm_struct :
                   ::uwvm2::object::global::wasm_ref_kind::wasm_array)) { return nullptr; }
            return checked_foreign_object(reference, kind);
        }
        [[nodiscard]] static inline gc_object_status reference_error(gc_reference reference) noexcept
        {
            return reference.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_null ?
                   gc_object_status::null_reference : gc_object_status::invalid_reference;
        }
        [[nodiscard]] static inline gc_object_status struct_get_object(object* obj,
            ::std::size_t field_index, bool sign_extend, gc_object_value& result) noexcept
        {
            auto const* layout{obj->owner->checked_type(obj->type_index, gc_type::composite_kind::struct_)};
            if(layout == nullptr || field_index >= layout->field_count || field_index >= obj->length)
            { return gc_object_status::out_of_bounds; }
            // [0, length) field index and owning type layout were checked above.
            auto const packed{layout->fields[field_index].storage.packed};
            if(layout->fields[field_index].mutable_)
            {
                object_lock lock{*obj};
                result = unpack(obj->values[field_index], packed, sign_extend);
            }
            else { result = unpack(obj->values[field_index], packed, sign_extend); }
            return gc_object_status::ok;
        }
#if defined(__GNUC__) || defined(__clang__)
        [[gnu::noinline, gnu::cold]]
#elif defined(_MSC_VER)
        __declspec(noinline)
#endif
        [[nodiscard]] gc_object_status struct_get_foreign(gc_reference reference,
            ::std::size_t field_index, bool sign_extend, gc_object_value& result) const noexcept
        {
            if(reference.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_struct ||
               reference.storage.ptr == nullptr) { return reference_error(reference); }
            auto* obj{checked_foreign_object(reference, gc_type::composite_kind::struct_)};
            return obj == nullptr ? reference_error(reference) :
                                    struct_get_object(obj, field_index, sign_extend, result);
        }
        // Return the low scalar word of the ordinary checked struct.get and
        // its status in one native integer. This is a bit-preserving view of
        // the existing full carrier; only the validated JIT emitter chooses
        // i32/f32 results. It does not reinterpret a token as an object address.
        template<bool SignExtend>
#if defined(__GNUC__) || defined(__clang__)
        [[gnu::always_inline]]
#endif
        [[nodiscard]] static inline ::std::uint64_t struct_get32_object(object* obj,
            ::std::size_t field_index) noexcept
        {
            auto const* layout{obj->owner->checked_type(obj->type_index, gc_type::composite_kind::struct_)};
            if(layout == nullptr || field_index >= layout->field_count || field_index >= obj->length)
            { return static_cast<::std::uint64_t>(gc_object_status::out_of_bounds) << 32u; }
            // [0, field_count) immutable live descriptors; [0, length) carriers.
            // [safe                                                           ]
            // ^^ field_index selects complete elements only after both checks.
            auto const& field{layout->fields[field_index]};
            ::std::uint32_t bits{};
            if(field.mutable_)
            {
                // A mutable field has exactly the ordinary getter's acquire /
                // release object lock; copy its four bytes while the lock lives.
                object_lock lock{*obj};
                bits = obj->values[field_index].template as<::std::uint32_t>();
            }
            else
            {
                // Immutable values were initialized before release publication.
                // A local/native reader runs in the same exclusion contract as
                // struct_get; no collection or owner teardown can race this read.
                bits = obj->values[field_index].template as<::std::uint32_t>();
            }
            if(field.storage.packed == gc_type::packed_kind::i8)
            {
                bits &= 0xffu;
                if constexpr(SignExtend) { bits = (bits ^ 0x80u) - 0x80u; }
            }
            else if(field.storage.packed == gc_type::packed_kind::i16)
            {
                bits &= 0xffffu;
                if constexpr(SignExtend) { bits = (bits ^ 0x8000u) - 0x8000u; }
            }
            // Unsigned arithmetic gives the exact modulo-2^32 packed extension.
            // An unpacked f32 keeps its NaN payload / signed zero without an FP ABI.
            return static_cast<::std::uint64_t>(bits);
        }
        template<bool SignExtend>
#if defined(__GNUC__) || defined(__clang__)
        [[gnu::noinline, gnu::cold]]
#elif defined(_MSC_VER)
        __declspec(noinline)
#endif
        [[nodiscard]] ::std::uint64_t struct_get32_foreign(gc_reference reference,
            ::std::size_t field_index) const noexcept
        {
            if(reference.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_struct ||
               reference.storage.ptr == nullptr)
            { return static_cast<::std::uint64_t>(reference_error(reference)) << 32u; }
            // [opaque key][global membership stripe][canonical strong lease]
            // [safe                                                       ]
            // ^^ checked_foreign_object proves membership and retains the owner
            // before this pointer can be used outside the lookup stripe.
            auto* obj{checked_foreign_object(reference, gc_type::composite_kind::struct_)};
            if(obj == nullptr)
            { return static_cast<::std::uint64_t>(reference_error(reference)) << 32u; }
            return struct_get32_object<SignExtend>(obj, field_index);
        }
        [[nodiscard]] static inline gc_object_status struct_set_object(object* obj,
            ::std::size_t field_index, gc_object_value value) noexcept
        {
            auto const* layout{obj->owner->checked_type(obj->type_index, gc_type::composite_kind::struct_)};
            if(layout == nullptr || field_index >= layout->field_count || field_index >= obj->length)
            { return gc_object_status::out_of_bounds; }
            // [0, length) field metadata and object slot are live.
            auto const& field{layout->fields[field_index]};
            if(!field.mutable_) { return gc_object_status::immutable_field; }
            if(field.storage.value.kind == gc_type::value_kind::reference)
            {
                if(!obj->owner->value_matches(value, field.storage, obj->owner))
                { return gc_object_status::invalid_value; }
                auto const lease{obj->owner->retain_embedded_reference(*obj, value, field.storage)};
                if(lease != gc_object_status::ok) { return lease; }
            }
            object_lock lock{*obj};
            obj->values[field_index] = pack(value, field.storage.packed);
            return gc_object_status::ok;
        }
#if defined(__GNUC__) || defined(__clang__)
        [[gnu::noinline, gnu::cold]]
#elif defined(_MSC_VER)
        __declspec(noinline)
#endif
        [[nodiscard]] gc_object_status struct_set_foreign(gc_reference reference,
            ::std::size_t field_index, gc_object_value value) noexcept
        {
            if(reference.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_struct ||
               reference.storage.ptr == nullptr) { return reference_error(reference); }
            auto* obj{checked_foreign_object(reference, gc_type::composite_kind::struct_)};
            return obj == nullptr ? reference_error(reference) :
                                    struct_set_object(obj, field_index, value);
        }

    private:
        struct checkpoint_staged_tag {};
        explicit gc_object_store(gc_type::recursive_type_section const& section,
            ::std::shared_ptr<gc_lease_owner> const& lease_owner,
            gc_function_type_match_callback function_type_match, checkpoint_staged_tag) noexcept :
            lease_owner_(lease_owner), function_type_match_(function_type_match)
        {
            // ^^ function_type_match_: copy trusted native code identity only;
            // it is immutable before any store/value can become observable.
            // A module may use only abstract references or pure function
            // recursive types. It still needs canonical IDs for rich imported
            // signatures, but cannot publish aggregates, so avoid a 64K index.
            bool has_aggregate_type{};
            for(auto const& group : section.groups)
            {
                for(auto const& type : group.types)
                {
                    if(type.kind == gc_type::composite_kind::struct_ ||
                       type.kind == gc_type::composite_kind::array)
                    { has_aggregate_type = true; break; }
                }
                if(has_aggregate_type) { break; }
            }
            if(has_aggregate_type)
            {
                membership_buckets_.reset(new(::std::nothrow) ::std::atomic<object*>[bucket_count]{});
                if(!membership_buckets_) { return; }
            }
            if(section.type_count > static_cast<::std::uint_least64_t>((::std::numeric_limits<::std::size_t>::max)()) /
                                     sizeof(type_layout)) { return; }
            layout_count_ = static_cast<::std::size_t>(section.type_count);
            if(layout_count_ != 0uz)
            {
                layouts_.reset(new(::std::nothrow) type_layout[layout_count_]{});
                if(!layouts_) { layout_count_ = 0uz; return; }
            }
            ::std::size_t index{};
            for(auto const& group : section.groups)
            {
                if(group.first_type_index != index || group.types.size() > layout_count_ - index) { return; }
                for(auto const& type : group.types)
                {
                    // [index, layout_count_) group metadata was bounded before this layout access.
                    auto& layout{layouts_[index]};
                    layout.kind = type.kind;
                    layout.field_count = type.fields.size();
                    if(type.supertypes.size() > 1uz || (!type.supertypes.empty() && type.supertypes[0] >= index)) { return; }
                    if(!type.supertypes.empty()) { layout.parent = type.supertypes[0]; }
                    if(type.kind == gc_type::composite_kind::array && layout.field_count != 1uz) { return; }
                    if(layout.field_count > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) /
                                            sizeof(gc_type::field_type)) { return; }
                    if(layout.field_count != 0uz)
                    {
                        layout.fields.reset(new(::std::nothrow) gc_type::field_type[layout.field_count]{});
                        if(!layout.fields) { return; }
                        for(::std::size_t field{}; field != layout.field_count; ++field)
                        {
                            // [0, field_count) both arrays contain this field; the copy owns no parser pointer.
                            layout.fields[field] = type.fields[field];
                            layout.has_reference_fields = layout.has_reference_fields ||
                                type.fields[field].storage.value.kind == gc_type::value_kind::reference;
                            layout.has_packed_fields = layout.has_packed_fields ||
                                type.fields[field].storage.packed != gc_type::packed_kind::none;
                            // Immutable defaultability, copied with the canonical schema.
                            auto const storage{type.fields[field].storage};
                            layout.has_nonnullable_reference_fields = layout.has_nonnullable_reference_fields ||
                                (storage.packed == gc_type::packed_kind::none &&
                                 storage.value.kind == gc_type::value_kind::reference && !storage.value.nullable);
                        }
                    }
                    ++index;
                }
            }
            valid_ = index == layout_count_;
            if(!valid_) { return; }
            if(layout_count_ == 0uz)
            {
                return;
            }
            ::uwvm2::utils::container::vector<::std::size_t> ids{};
            {
                canonical_guard guard{};
                auto const result{canonical_registry_.validate_and_intern(section, ids)};
                if(result.error != ::uwvm2::validation::standard::wasm3::recursive_type_validation_error::ok)
                { valid_ = false; return; }
            }
            if(ids.size() != layout_count_) { valid_ = false; return; }
            canonical_ids_.reset(new(::std::nothrow) ::std::size_t[layout_count_]);
            if(!canonical_ids_) { valid_ = false; return; }
            for(::std::size_t type{}; type != layout_count_; ++type)
            {
                // [0, layout_count_) both immutable canonical maps are complete.
                canonical_ids_[type] = ids.index_unchecked(type);
            }
#if defined(UWVM_EXPERIMENTAL_ARRAY_SET_LOCAL_REF_AUTH) && UWVM_EXPERIMENTAL_ARRAY_SET_LOCAL_REF_AUTH == 1
            if(!build_local_subtype_intervals()) { valid_ = false; return; }
#endif
#if defined(UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA == 1
            for(::std::size_t type{}; type != layout_count_; ++type)
            {
                // [layouts_,layouts_+layout_count_) owns immutable copied
                // layouts; canonical validation/interning above proved all
                // heap/value declarations BEFORE trace metadata is accepted.
                // [safe                                  ] type < layout_count_.
                auto& layout{layouts_[type]};
                if(layout.trace.build(layout.kind, layout.fields.get(), layout.field_count) !=
                   gc_trace_metadata_status::ok)
                { valid_ = false; return; }
            }
#endif
        }
    public:
        explicit gc_object_store(gc_type::recursive_type_section const& section,
            ::std::shared_ptr<gc_lease_owner> const& lease_owner = {},
            gc_function_type_match_callback function_type_match = nullptr) noexcept :
            gc_object_store(section,lease_owner,function_type_match,checkpoint_staged_tag{})
        {
            if(valid_) { register_cohort_member(); }
        }
        ~gc_object_store()
        {
#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1
            retire_private_compact_numeric_segments_after_drain();
#endif
            unregister_cohort_member();
            {
                slab_guard guard{*this};
                // Existing module drain excludes all native mutators. A
                // reserved construction is not a published GC object and must
                // have returned before teardown; never silently free its slot.
                if(slab_reserved_slots_ != 0uz) { ::std::terminate(); }
                slab_closing_ = true;
                if(slab_epoch_ != (::std::numeric_limits<::std::uint64_t>::max)()) { ++slab_epoch_; }
            }
            // Guest threads belonging to this module have drained. Each
            // foreign reader takes the same stripe as its target bucket and
            // promotes the owner's weak_ptr before releasing that stripe.
            // Other buckets can keep serving and publishing concurrently.
            auto* current{objects_.load(::std::memory_order_relaxed)};
            while(current != nullptr)
            {
                auto const index{bucket_index(current->token)};
                {
                    global_guard guard{index};
                    auto** link{::std::addressof(global_buckets_[index])};
                    while(*link != current)
                    {
                        if(*link == nullptr) { ::std::terminate(); }
                        // [registered global chain] this stripe excludes readers and writers.
                        // ^^ link advances to the next pointer slot before splicing.
                        link = ::std::addressof((*link)->global_next);
                    }
                    // [link]->[current]->[next registered global node]
                    // ^^ unlink current before the arena frees it.
                    *link = current->global_next;
                }
                // [module-owned object chain] current remains live until the
                // deletion loop below; next was initialized at publication.
                // ^^ advance only along this store's published objects.
                current = current->next;
            }
            // Guest threads have drained before module teardown. Remove every
            // wrapper owned by this module under the process-wide lock so a
            // concurrent foreign conversion cannot read a freed bridge.
            extern_bridge* retired_bridges{};
            {
                bridge_guard guard{};
                for(auto& bucket : bridge_token_buckets_)
                {
                    auto** token_link{::std::addressof(bucket)};
                    while(*token_link != nullptr)
                    {
                        auto* current_bridge{*token_link};
                        if(current_bridge->owner == this)
                        {
                            // [token_link]->[current_bridge]->[next registered bridge]
                            // [safe                                      ] bridge_lock_ excludes readers.
                            // ^^ splice token_link past the bridge before freeing it.
                            *token_link = current_bridge->token_next;
                            auto** inner_link{::std::addressof(
                                bridge_inner_buckets_[bridge_inner_bucket(current_bridge->inner)])};
                            while(*inner_link != current_bridge)
                            {
                                if(*inner_link == nullptr) { ::std::terminate(); }
                                // [registered inner chain] every next link remains live under bridge_lock_.
                                // ^^ inner_link advances to the address of the following pointer slot.
                                inner_link = ::std::addressof((*inner_link)->inner_next);
                            }
                            // [inner_link]->[current_bridge]->[next registered bridge]
                            // [safe                                      ] no reader can observe the splice.
                            // ^^ remove current_bridge from the second index.
                            *inner_link = current_bridge->inner_next;
                            // [detached bridge][retired private chain] both indexes no longer
                            // expose this node. Reuse token_next only after the two splices.
                            // ^^ defer deletion: inner_owner may synchronously destroy another
                            // store, whose destructor also acquires bridge_lock_.
                            current_bridge->token_next = retired_bridges;
                            retired_bridges = current_bridge;
                        }
                        else
                        {
                            // [registered token chain] current_bridge remains live under the lock.
                            // ^^ token_link advances to its next pointer slot.
                            token_link = ::std::addressof(current_bridge->token_next);
                        }
                    }
                }
            }
            while(retired_bridges != nullptr)
            {
                // [private detached bridge chain] no registry lock is held here.
                // ^^ save the next owned node before releasing the foreign store lease.
                auto* next{retired_bridges->token_next};
                delete retired_bridges;
                // [remaining detached chain] next was read before deletion; recursive
                // store teardown cannot touch these unregistered private nodes.
                // ^^ advance only to a still-owned node or null.
                retired_bridges = next;
            }
            // Runtime shutdown joins guest threads before destroying this store. Every next link
            // remains valid until its own node is deleted, including cyclic reference values.
            current = objects_.load(::std::memory_order_relaxed);
            while(current != nullptr)
            {
                auto* next{current->next};
                destroy_object(current);
                current = next;
            }
            {
                slab_guard guard{*this};
                if(slab_chunk_count_ != 0uz || slab_reserved_slots_ != 0uz ||
                   slab_allocated_slots_ != 0uz || slab_backing_bytes_ != 0uz) { ::std::terminate(); }
                for(auto* head : slab_all_) { if(head != nullptr) { ::std::terminate(); } }
                for(auto* head : slab_available_) { if(head != nullptr) { ::std::terminate(); } }
            }
            // No guest activation can acquire another root after module drain.
            // Detach once, then release tokens without holding this store lock.
            exn_root_node* roots{};
            {
                exn_roots_guard roots_guard{*this};
                roots = exn_roots_;
                exn_roots_ = nullptr;
                // No guest activation remains. Clear the secondary lookup
                // before deleting nodes from the detached teardown chain.
                exn_root_buckets_.reset();
            }
            while(roots != nullptr)
            {
                auto* next{roots->next};
                exn_token_entry* retired{};
                {
                    exn_guard registry_guard{};
                    auto* entry{roots->entry};
                    if(entry == nullptr || entry->root_count == 0uz) { ::std::terminate(); }
                    if(--entry->root_count == 0uz)
                    {
                        auto** link{::std::addressof(exn_buckets_[exn_bucket(entry->token)])};
                        while(*link != entry)
                        {
                            if(*link == nullptr) { ::std::terminate(); }
                            // [registered exn chain] exn_lock_ excludes lookup/removal.
                            // ^^ link advances to a live next pointer slot.
                            link = ::std::addressof((*link)->next);
                        }
                        // [link]->[retired]->[next registered exn token]
                        // ^^ remove from registry before freeing its value_ref.
                        *link = entry->next;
                        retired = entry;
                    }
                }
                delete roots;
                delete retired;
                // [detached module root chain] next was read before deletion.
                // ^^ roots advances only to its next owned node or null.
                roots = next;
            }
        }
        gc_object_store(gc_object_store const&) = delete;
        gc_object_store& operator=(gc_object_store const&) = delete;
        [[nodiscard]] inline bool valid() const noexcept { return valid_; }
        // Experimental stop-the-world aggregate collector. The caller owns
        // canonical strong pins for EVERY admitted store (including empty
        // stores), a complete precise root snapshot and an admission closure
        // covering native host readers, initialization and module teardown.
        // This API neither discovers activation roots nor stops readers. It
        // is intentionally not called from product allocation yet. Exn/extern
        // wrappers require a different typed tracing closure and reject here.
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
        [[nodiscard]] static inline gc_object_status collect_exclusive_aggregate_domain(
            ::std::shared_ptr<gc_object_store> const* stores, ::std::size_t store_count,
            gc_reference const* roots, ::std::size_t root_count,
            ::std::size_t& reclaimed) noexcept
        { return collect_exclusive_domain_impl(stores, store_count, roots, root_count, reclaimed, {}, nullptr); }

        // Native-only closed-domain API. Every external/activation immutable
        // owner must be included, not merely registry owners. Preserve the
        // snapshot and canonical generation pins through complete pause exit.
        // Release retired values only AFTER all root/registry/cohort/stop locks.
        // This method does not broaden production tag/native-escape admission.
        [[nodiscard]] static inline gc_object_status collect_exclusive_exception_domain(
            ::std::shared_ptr<gc_object_store> const* stores, ::std::size_t store_count,
            gc_reference const* roots, ::std::size_t root_count,
            ::std::span<::uwvm2::runtime::exception::value_ref const> native_values,
            ::std::size_t& reclaimed, retired_exception_batch& retired) noexcept
        { return collect_exclusive_domain_impl(stores, store_count, roots, root_count, reclaimed, native_values, ::std::addressof(retired)); }

#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1
        // Invoke ONLY within with_registered_roots while its actual census mutex
        // and native generation/owner pins remain held. Every ordinary external
        // value owner must already be registered, or collection stays forbidden.
        // No owning value snapshot is destroyed while the stop/admission locks
        // are held; complete immutable values are borrowed from real leases.
        [[nodiscard]] static inline gc_object_status collect_exclusive_registered_exception_domain(
            ::std::shared_ptr<gc_object_store> const* stores, ::std::size_t store_count,
            gc_reference const* roots, ::std::size_t root_count,
            ::uwvm2::runtime::exception::native_exception_root_domain::registered_view const& native_values,
            ::std::size_t& reclaimed, retired_exception_batch& retired) noexcept
        {
            // [domain-minted callback view] remains live and locked until return.
            // ^^ the private implementation never retains this native borrow.
            return collect_exclusive_domain_impl(stores, store_count, roots, root_count, reclaimed, {},
                ::std::addressof(retired), ::std::addressof(native_values));
        }
#endif

    private:
        [[nodiscard]] static inline gc_object_status collect_exclusive_domain_impl(
            ::std::shared_ptr<gc_object_store> const* stores, ::std::size_t store_count,
            gc_reference const* roots, ::std::size_t root_count, ::std::size_t& reclaimed,
            ::std::span<::uwvm2::runtime::exception::value_ref const> native_values,
            retired_exception_batch* retired_exceptions
#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1
            , ::uwvm2::runtime::exception::native_exception_root_domain::registered_view const* native_values_view = nullptr
#endif
            ) noexcept
#else
        [[nodiscard]] static inline gc_object_status collect_exclusive_aggregate_domain(
            ::std::shared_ptr<gc_object_store> const* stores, ::std::size_t store_count,
            gc_reference const* roots, ::std::size_t root_count,
            ::std::size_t& reclaimed) noexcept
#endif
        {
#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1

            reclaimed = 0uz;
            if((store_count != 0uz && stores == nullptr) ||
               (root_count != 0uz && roots == nullptr) ||
               store_count > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(*stores) ||
               root_count > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(*roots))
            { return gc_object_status::invalid_value; }
            for(::std::size_t index{}; index != store_count; ++index)
            {
                // [0,store_count) is the trusted caller's owning cohort array.
                if(!stores[index] || !stores[index]->valid_)
                { return gc_object_status::invalid_store; }
                auto canonical{stores[index]->weak_from_this().lock()};
                if(!canonical || stores[index].owner_before(canonical) || canonical.owner_before(stores[index]))
                { return gc_object_status::invalid_store; }
                for(::std::size_t earlier{}; earlier != index; ++earlier)
                {
                    if(stores[earlier].get() == stores[index].get())
                    { return gc_object_status::invalid_store; }
                }
                if(
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
                   retired_exceptions == nullptr &&
#endif
                   stores[index]->exn_roots_ != nullptr)
                { return gc_object_status::invalid_reference; }
            }
            // Admission stays closed through tracing and sweeping. Every live
            // entry must match one canonical caller pin, even if it has never
            // issued a token. Invalid stores cannot publish objects/roots.
#if defined(UWVM_EXPERIMENTAL_COMPACT_COLLECTION_DIRECTORY) && UWVM_EXPERIMENTAL_COMPACT_COLLECTION_DIRECTORY == 1
# include "compact_numeric/collection_directory.h"
#endif
            cohort_guard admission{};
            ::std::size_t registered{};
            for(auto* current{cohort_head_}; current != nullptr;)
            {
                bool included{};
                for(::std::size_t index{}; index != store_count; ++index)
                { included = included || current == stores[index].get(); }
                if(!included || registered == store_count)
                { return gc_object_status::invalid_reference; }
                ++registered;
                // [protected live store list] admission excludes unlink/free.
                // ^^ advance only to its initialized native next link or null.
                current = current->cohort_next_;
            }
            if(registered != store_count) { return gc_object_status::invalid_store; }
            {
                bridge_guard guard{};
                for(auto* head : bridge_token_buckets_)
                { if(head != nullptr) { return gc_object_status::invalid_reference; } }
            }
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
            exception_graph_state graph{retired_exceptions, native_values, stores, store_count
#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1
                , native_values_view
#endif
            };
            if(graph.enabled())
            {
                auto const prepared{graph.prepare_closed_registry()};
                if(prepared != gc_object_status::ok) { return prepared; }
            }
            else
#endif
            {
                exn_guard guard{};
                for(auto* head : exn_buckets_)
                { if(head != nullptr) { return gc_object_status::invalid_reference; } }
            }
            using ref_kind = ::uwvm2::object::global::wasm_ref_kind;
            auto non_object{[](gc_reference reference) noexcept
            {
                return reference.kind == ref_kind::wasm_null ||
                       reference.kind == ref_kind::wasm_i31 ||
                       reference.kind == ref_kind::wasm_func_imported ||
                       reference.kind == ref_kind::wasm_func_defined;
            }};
#if defined(UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP) && UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP == 1
            // Canonical/cohort checks above and the ORIGINAL caller's actual
            // exclusive stop stay live through preflight/mark/prune/sweep.
            // Reservation failure declines only this optimization; the original
            // route retains the original late error precedence.
            bool const local_membership{collection_local_membership_eligible(stores, store_count)};
#endif
            auto locate{[&](gc_reference reference) noexcept -> aggregate_object_view
            {
                if((reference.kind != ref_kind::wasm_struct && reference.kind != ref_kind::wasm_array) ||
                   reference.storage.ptr == nullptr) { return {}; }
                auto const kind{reference.kind == ref_kind::wasm_struct ?
                    gc_type::composite_kind::struct_ : gc_type::composite_kind::array};
                // [closed canonical cohort] use only these strong store pins;
                // never enter a cold reader's shared admission while exclusive
                // collection is already held. Local typed decode borrows only.
                if(kind == gc_type::composite_kind::struct_)
                {
#if defined(UWVM_EXPERIMENTAL_COMPACT_COLLECTION_DIRECTORY) && UWVM_EXPERIMENTAL_COMPACT_COLLECTION_DIRECTORY == 1
                    auto const compact{locate_compact_directory(reference)};
                    if(compact) { return compact; }
#else
                    for(::std::size_t index{}; index != store_count; ++index)
                    {
                        auto const compact{stores[index]->checked_local_compact_object(reference)};
                        if(compact) { return compact; }
                    }
#endif
                }
#if defined(UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP) && UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP == 1
                if(local_membership)
                {
                    auto* member{locate_local_collection_member(stores, store_count, reference, kind)};
                    // [authentic stopped member][same canonical pin]
                    // [safe ] this borrow dies before pause/admission release;
                    // no new lease or cached token pointer is exposed.
                    return member == nullptr ? aggregate_object_view{} :
                        aggregate_object_view{member, nullptr, member->owner, 0uz};
                }
#endif
                auto const bucket{bucket_index(reference.storage.ptr)};
                global_guard guard{bucket};
                for(auto* current{global_buckets_[bucket]}; current != nullptr;)
                {
                    // [registered global chain] stripe excludes unlink/free.
                    // The untrusted token is compared, never dereferenced.
                    if(current->token == reference.storage.ptr)
                    {
                        // A teardown that has already left admission may still
                        // have a protected global-index node. Compare its owner
                        // identity against canonical pins before returning it;
                        // never dereference an owner outside this closed cohort.
                        bool included{};
                        for(::std::size_t index{}; index != store_count; ++index)
                        { included = included || current->owner == stores[index].get(); }
                        return included && current->kind == kind ? aggregate_object_view{current, nullptr, current->owner, 0uz} : aggregate_object_view{};
                    }
                    // ^^ follow only the registered initialized native link.
                    current = current->global_next;
                }
                return {};
            }};
            auto validate_reference{[&](gc_reference reference) noexcept
            { return non_object(reference)
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
                     || graph.find(reference) != nullptr
#endif
                     || static_cast<bool>(locate(reference)); }};
            auto visit_fields_uncached{[](aggregate_object_view view, auto&& visitor) noexcept
            {
                if(!view) { return false; }
                if(view.compact != nullptr) { return true; } // Numeric leaf: zero reference edges.
                auto const& current{*view.legacy};
                auto const* layout{current.owner->checked_type(current.type_index, current.kind)};
                if(layout == nullptr || (current.length != 0uz && !current.values) ||
                   (current.kind == gc_type::composite_kind::struct_ && current.length != layout->field_count) ||
                   (current.kind == gc_type::composite_kind::array && layout->field_count != 1uz))
                { return false; }
#if defined(UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA == 1
                // [authentic owned header][canonical immutable type layout]
                // [safe ] this stopped closed-cohort span has no callback or
                // epoch change. The plan is a native-owned classification of
                // EXACTLY layout.fields, not guest/reference membership proof.
                if(!layout->trace.matches_shape(current.kind, layout->field_count)) { return false; }
#endif
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
                if(current.numeric_array_)
                {
                    auto const width{numeric_array_width(layout)};
                    // [real checked numeric array][original bounded byte tail]
                    // [safe ] this immutable representation has NO reference
                    // edges. Never index its byte payload as a carrier array.
                    return width != 0uz && current.length <=
                        (static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) -
                         object_value_offset) / width;
                }
#endif
#if defined(UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA == 1
                return visit_precise_reference_fields(current, *layout, visitor);
#else
                // Numeric layouts have no reference edges. Shape/storage checks above still run.
                // Copied layouts remain immutable across every collection epoch.
                if(!layout->has_reference_fields) { return true; }
                for(::std::size_t index{}; index != current.length; ++index)
                {
                    // [0,length) owns initialized values; the checked layout
                    // owns one array descriptor or exactly length struct fields.
                    auto const storage{layout->fields[current.kind == gc_type::composite_kind::array ? 0uz : index].storage};
                    if(storage.packed == gc_type::packed_kind::none &&
                       storage.value.kind == gc_type::value_kind::reference &&
                       !visitor(current.values[index].as<gc_reference>())) { return false; }
                }
#endif
                return true;
            }};

            auto visit_fields{[&](auto const& view, auto&& visitor) noexcept
            {
#if defined(UWVM_EXPERIMENTAL_GENERAL_GC_SLAB) && UWVM_EXPERIMENTAL_GENERAL_GC_SLAB == 1
                // Collector-only idempotent callbacks; closed cohort stays
                // stopped for this WHOLE traversal. Original shape checks and
                // every field read still run. Remember only the last successful
                // opaque token, never a header/pin or cross-traversal authority.
                bool previous_valid{};
                ref_kind previous_kind{};
                void* previous_token{};
                return visit_fields_uncached(view, [&](gc_reference reference) noexcept
                {
                    bool const aggregate{reference.kind == ref_kind::wasm_struct ||
                                         reference.kind == ref_kind::wasm_array ||
                                         reference.kind == ref_kind::wasm_exn};
                    if(aggregate && previous_valid && reference.kind == previous_kind &&
                       reference.storage.ptr == previous_token) { return true; }
                    if(!visitor(reference)) { return false; }
                    previous_valid = aggregate;
                    if(aggregate)
                    {
                        previous_kind = reference.kind;
                        previous_token = reference.storage.ptr;
                    }
                    return true;
                });
#else
                return visit_fields_uncached(view, visitor);
#endif
            }};
            ::std::size_t total_live_count{};
            constexpr auto max_live{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(object*)};
            for(::std::size_t index{}; index != store_count; ++index)
            {
                auto& store{*stores[index]};
                for(auto current{store.compact_numeric_segments_}; current;)
                {
                    auto owner{current->canonical_store_.lock()};
                    auto const* layout{store.checked_type(current->type_index_, gc_type::composite_kind::struct_)};
                    if(!owner || owner.get() != stores[index].get() ||
                       current->canonical_store_.owner_before(stores[index]) ||
                       stores[index].owner_before(current->canonical_store_) ||
                       !compact_layout(layout) ||
                       current->canonical_type_id_ != store.canonical_type_id(current->type_index_) ||
                       current->capacity_ != current->payload_.size() ||
                       current->initialized_frontier_.load(::std::memory_order_acquire) > current->capacity_ ||
                       !compact_numeric_range_geometry::valid(current->token_begin_, current->capacity_) ||
                       (layout->fields[0uz].storage.value.kind == gc_type::value_kind::i32 ?
                            current->kind_ != compact_numeric_kind::i32 : current->kind_ != compact_numeric_kind::f32))
                    { return gc_object_status::invalid_store; }
                    auto const prepared{current->begin_marks_while_stopped()};
                    if(prepared != compact_numeric_status::ok) { return compact_status(prepared); }
#if defined(UWVM_EXPERIMENTAL_COMPACT_COLLECTION_DIRECTORY) && UWVM_EXPERIMENTAL_COMPACT_COLLECTION_DIRECTORY == 1
                    if(compact_directory_count == max_compact_directory_count)
                    { return gc_object_status::size_overflow; }
                    ++compact_directory_count;
#endif
                    for(auto const& word : current->live_)
                    {
                        auto const count{static_cast<::std::size_t>(::std::popcount(word.load(::std::memory_order_relaxed)))};
                        if(count > max_live - total_live_count) { return gc_object_status::size_overflow; }
                        total_live_count += count;
                    }
                    // [strong canonical store chain] collection alone owns next
                    // mutations; preserve the next pin before dropping current.
                    current = current->owner_next_;
                }
            }
#if defined(UWVM_EXPERIMENTAL_COMPACT_COLLECTION_DIRECTORY) && UWVM_EXPERIMENTAL_COMPACT_COLLECTION_DIRECTORY == 1
            auto const directory_status{build_compact_directory()};
            if(directory_status != gc_object_status::ok) { return directory_status; }
#endif
            ::std::size_t object_count{};
            auto constexpr max_work{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(object*)};
            // Validate the entire graph and precise roots before reclamation.
            // Failure can clear marks, but cannot delete or invalidate a token.
            for(::std::size_t index{}; index != store_count; ++index)
            {
                for(auto* current{stores[index]->objects_.load(::std::memory_order_acquire)}; current != nullptr;)
                {
                    if(current->owner != stores[index].get() || !visit_fields(aggregate_object_view{current, nullptr, current->owner, 0uz}, validate_reference))
                    { return gc_object_status::invalid_reference; }
                    if(object_count == max_work) { return gc_object_status::size_overflow; }
                    if(total_live_count == max_live) { return gc_object_status::size_overflow; }
                    ++total_live_count;
                    ++object_count;
                    current->marked = false;
                    // [quiescent store-owned object chain] canonical pins keep
                    // every header alive until the sweep completes.
                    // ^^ advance only through its initialized owned next link.
                    current = current->next;
                }
            }
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
            if(!graph.validate_all_values(validate_reference)) { return gc_object_status::invalid_reference; }
#endif
            // At most one work entry per proved live object. Work storage is
            // collection-only, bounded by current live headers, and freed after
            // this pass; no per-issued-token bitmap/index accumulates forever.
            ::std::unique_ptr<object*[]> work{};
            if(object_count != 0uz)
            {
                work.reset(new(::std::nothrow) object*[object_count]);
                if(!work)
                {
                    // Preserve the old invalid-root-before-work-OOM ordering.
                    // Successful allocation authenticates roots in mark below;
                    // this cold failure path cannot enter epoch commit/sweep.
                    for(::std::size_t index{}; index != root_count; ++index)
                    {
                        // [roots,roots+root_count) is the checked, complete
                        // stopped snapshot; index<root_count borrows one live
                        // carrier. No caller storage changes before return.
                        if(!validate_reference(roots[index])) { return gc_object_status::invalid_reference; }
                    }
                    return gc_object_status::out_of_memory;
                }
            }
            ::std::size_t work_count{};
            auto mark{[&](gc_reference reference) noexcept
            {
                if(non_object(reference)) { return true; }
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
                if(reference.kind == ref_kind::wasm_exn) { return graph.mark(reference); }
#endif
                auto const child{locate(reference)};
                if(!child) { return false; }
                if(child.compact != nullptr)
                {
                    // [authentic canonical descriptor][its exact live slot]
                    // [safe ] locate returned this pair inside the same closed
                    // pause/exclusive admission. No callback, poll, unlock or
                    // retirement intervenes; mark only its bounded side bitmap.
                    // A foreign root uses its actual in-cohort owning store.
                    return child.compact->mark_authenticated_slot_while_stopped(
                        child.compact_slot) == compact_numeric_status::ok;
                }
                if(child.legacy->marked) { return true; }
                if(work_count == object_count) { ::std::terminate(); }
                child.legacy->marked = true;
                // [0,object_count) owns native work slots; each object is added
                // once, and work_count is proved below the allocated bound.
                work[work_count++] = child.legacy;
                return true;
            }};
            for(::std::size_t index{}; index != root_count; ++index)
            {
                // [roots,roots+root_count) checked snapshot remains stopped.
                // [safe ] index<root_count borrows one complete carrier; mark
                // authenticates membership before use. A later failure may
                // leave temporary marks, but cannot reclaim or change any token.
                if(!mark(roots[index])) { return gc_object_status::invalid_reference; }
            }
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
            if(!graph.visit_native_values(mark)) { return gc_object_status::invalid_reference; }
#endif
            while(work_count != 0uz
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
                  || graph.has_work()
#endif
                 )
            {
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
                if(work_count == 0uz)
                {
                    if(!graph.pop_and_trace(mark)) { return gc_object_status::invalid_reference; }
                    continue;
                }
#endif
                // [0,work_count) contains proved live native object pointers.
                // ^^ pop a complete initialized slot before tracing its fields.
                auto* current{work[--work_count]};
                if(!visit_fields(aggregate_object_view{current, nullptr, current->owner, 0uz}, mark)) { return gc_object_status::invalid_reference; }
            }
            // The closed-cohort caller has stopped all mutators. Validate all
            // reservation/epoch states before mutating any store, then invalidate
            // future cached allocation tickets before unlinking/freeing objects.
            for(::std::size_t index{}; index != store_count; ++index)
            { if(!stores[index]->can_advance_slab_epoch_exclusive()) { return gc_object_status::invalid_store; } }
            for(::std::size_t index{}; index != store_count; ++index)
            { stores[index]->advance_slab_epoch_exclusive(); }
            for(::std::size_t index{}; index != store_count; ++index)
            {
                auto& store{*stores[index]};
#if defined(UWVM_EXPERIMENTAL_GENERAL_GC_SLAB) && UWVM_EXPERIMENTAL_GENERAL_GC_SLAB == 1
                sweep_retirement_batch retirement{store};
#endif
                object* kept{};
                object** tail{::std::addressof(kept)};
                for(auto* current{store.objects_.load(::std::memory_order_relaxed)}; current != nullptr;)
                {
                    // [owned live headers] save next before possibly freeing current.
                    auto* next{current->next};
                    if(current->marked)
                    {
                        current->marked = false;
                        // [native kept pointer slot] tail belongs to this sweep.
                        // ^^ append before moving tail to current's owned next slot.
                        *tail = current;
                        tail = ::std::addressof(current->next);
                        // Old foreign values must not pin otherwise dead stores
                        // forever after field overwrite. Only current validated
                        // typed aggregate fields justify an object-owned lease.
                        current->value_leases.prune_exclusive([&](gc_object_store const* leased) noexcept
                        {
                            bool used{};
                            auto const valid{visit_fields(aggregate_object_view{current, nullptr, current->owner, 0uz}, [&](gc_reference reference) noexcept
                            {
                                if(!non_object(reference))
                                {
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
                                    if(reference.kind == ref_kind::wasm_exn)
                                    {
                                        auto const* issuer{graph.issuer(reference)};
                                        if(issuer == nullptr) { return false; }
                                        used = used || issuer == leased;
                                        return true;
                                    }
#endif
                                    auto const member{locate(reference)};
                                    if(!member) { return false; }
                                    used = used || member.owner() == leased;
                                }
                                return true;
                            })};
                            if(!valid) { ::std::terminate(); }
                            return used;
                        });
                    }
                    else
                    {
                        auto const bucket_index_value{bucket_index(current->token)};
                        auto& bucket{store.membership_buckets_[bucket_index_value]};
                        auto* head{bucket.load(::std::memory_order_relaxed)};
                        auto** link{::std::addressof(head)};
                        while(*link != current)
                        {
                            if(*link == nullptr) { ::std::terminate(); }
                            // [protected local membership] every predecessor is live.
                            // ^^ move to a proved native hash_next pointer slot.
                            link = ::std::addressof((*link)->hash_next);
                        }
                        // [predecessor][retired][successor] guest token stops resolving.
                        // ^^ splice local membership before releasing object values.
                        *link = current->hash_next;
                        bucket.store(head, ::std::memory_order_release);
                        {
                            global_guard guard{bucket_index_value};
                            auto** global_link{::std::addressof(global_buckets_[bucket_index_value])};
                            while(*global_link != current)
                            {
                                if(*global_link == nullptr) { ::std::terminate(); }
                                // [protected foreign membership] every node is live.
                                // ^^ move to a proved native global_next pointer slot.
                                global_link = ::std::addressof((*global_link)->global_next);
                            }
                            // [global predecessor][retired][global successor]
                            // ^^ splice foreign membership before freeing the native header.
                            *global_link = current->global_next;
                        }
                        // Every store is still canonically pinned. Releasing a
                        // foreign value lease cannot destroy a later cohort node.
#if defined(UWVM_EXPERIMENTAL_GENERAL_GC_SLAB) && UWVM_EXPERIMENTAL_GENERAL_GC_SLAB == 1
                        retirement.destroy_unlinked(current);
#else
                        destroy_object(current);
#endif
                        ++reclaimed;
                    }
                    // [remaining owned headers] next was read before deletion.
                    // ^^ advance only to a still-live native header or null.
                    current = next;
                }
                // [last kept pointer slot] terminate before publishing the rebuilt chain.
                // ^^ tail names kept or the final retained object's next member.
                *tail = nullptr;
                store.objects_.store(kept, ::std::memory_order_release);
            }
#if defined(UWVM_EXPERIMENTAL_COMPACT_COLLECTION_DIRECTORY) && UWVM_EXPERIMENTAL_COMPACT_COLLECTION_DIRECTORY == 1
            // All locate/lease-prune calls are finished. Invalidate the native
            // directory BEFORE any descriptor can unlink/retire/free; its raw
            // records are never dereferenced after this point. Array deletion
            // remains outside cohort_guard and destroys no descriptor owner.
            compact_directory_count = 0uz;
#endif
            for(::std::size_t index{}; index != store_count; ++index)
            {
                auto& store{*stores[index]};
                // [store head or a live descriptor's strong next member]
                // ^^ link always names an actual owning shared_ptr slot.
                auto* link{::std::addressof(store.compact_numeric_segments_)};
                while(*link)
                {
                    auto current{*link};
                    ::std::size_t dead{};
                    auto const swept{current->sweep_unmarked_while_stopped(dead)};
                    if(swept != compact_numeric_status::ok) { ::std::terminate(); }
                    if(dead > total_live_count - reclaimed) { ::std::terminate(); }
                    reclaimed += dead;
                    bool empty{true};
                    for(auto const& word : current->live_)
                    { empty = empty && word.load(::std::memory_order_relaxed) == 0u; }
                    if(empty)
                    {
                        // [owned predecessor][empty range][owned successor]
                        // ^^ unlink locally, unregister globally, then free real
                        // payload and side metadata. Dead IDs are NEVER reused.
                        *link = ::std::move(current->owner_next_);
                        if(current->begin_retire() != compact_numeric_status::ok ||
                           current->finish_retire() != compact_numeric_status::ok)
                        { ::std::terminate(); }
                    }
                    else
                    {
                        // [retained live range] move to its initialized owning
                        // next member while the current node remains pinned.
                        // ^^ next iteration may retire ONLY that successor.
                        link = ::std::addressof(current->owner_next_);
                    }
                }
                // Every VM/native reader remains stopped until after this
                // release head publication. No old descriptor borrow survives
                // this sweep/poll epoch, even though guest token IDs are stable.
                store.compact_numeric_head_.store(store.compact_numeric_segments_.get(), ::std::memory_order_release);
            }
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
            graph.sweep_registry_after_heap_commit();
#endif
            return gc_object_status::ok;
        
#else

            reclaimed = 0uz;
            if((store_count != 0uz && stores == nullptr) ||
               (root_count != 0uz && roots == nullptr) ||
               store_count > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(*stores) ||
               root_count > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(*roots))
            { return gc_object_status::invalid_value; }
            for(::std::size_t index{}; index != store_count; ++index)
            {
                // [0,store_count) is the trusted caller's owning cohort array.
                if(!stores[index] || !stores[index]->valid_)
                { return gc_object_status::invalid_store; }
                auto canonical{stores[index]->weak_from_this().lock()};
                if(!canonical || stores[index].owner_before(canonical) || canonical.owner_before(stores[index]))
                { return gc_object_status::invalid_store; }
                for(::std::size_t earlier{}; earlier != index; ++earlier)
                {
                    if(stores[earlier].get() == stores[index].get())
                    { return gc_object_status::invalid_store; }
                }
                if(
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
                   retired_exceptions == nullptr &&
#endif
                   stores[index]->exn_roots_ != nullptr)
                { return gc_object_status::invalid_reference; }
            }
            // Admission stays closed through tracing and sweeping. Every live
            // entry must match one canonical caller pin, even if it has never
            // issued a token. Invalid stores cannot publish objects/roots.
            cohort_guard admission{};
            ::std::size_t registered{};
            for(auto* current{cohort_head_}; current != nullptr;)
            {
                bool included{};
                for(::std::size_t index{}; index != store_count; ++index)
                { included = included || current == stores[index].get(); }
                if(!included || registered == store_count)
                { return gc_object_status::invalid_reference; }
                ++registered;
                // [protected live store list] admission excludes unlink/free.
                // ^^ advance only to its initialized native next link or null.
                current = current->cohort_next_;
            }
            if(registered != store_count) { return gc_object_status::invalid_store; }
            {
                bridge_guard guard{};
                for(auto* head : bridge_token_buckets_)
                { if(head != nullptr) { return gc_object_status::invalid_reference; } }
            }
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
            exception_graph_state graph{retired_exceptions, native_values, stores, store_count
#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1
                , native_values_view
#endif
            };
            if(graph.enabled())
            {
                auto const prepared{graph.prepare_closed_registry()};
                if(prepared != gc_object_status::ok) { return prepared; }
            }
            else
#endif
            {
                exn_guard guard{};
                for(auto* head : exn_buckets_)
                { if(head != nullptr) { return gc_object_status::invalid_reference; } }
            }
            using ref_kind = ::uwvm2::object::global::wasm_ref_kind;
            auto non_object{[](gc_reference reference) noexcept
            {
                return reference.kind == ref_kind::wasm_null ||
                       reference.kind == ref_kind::wasm_i31 ||
                       reference.kind == ref_kind::wasm_func_imported ||
                       reference.kind == ref_kind::wasm_func_defined;
            }};
#if defined(UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP) && UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP == 1
            // Canonical/cohort checks above and the ORIGINAL caller's actual
            // exclusive stop stay live through preflight/mark/prune/sweep.
            // Reservation failure declines only this optimization; the original
            // route retains the original late error precedence.
            bool const local_membership{collection_local_membership_eligible(stores, store_count)};
#endif
            auto locate{[&](gc_reference reference) noexcept -> object*
            {
                if((reference.kind != ref_kind::wasm_struct && reference.kind != ref_kind::wasm_array) ||
                   reference.storage.ptr == nullptr) { return nullptr; }
                auto const kind{reference.kind == ref_kind::wasm_struct ?
                    gc_type::composite_kind::struct_ : gc_type::composite_kind::array};
#if defined(UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP) && UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP == 1
                if(local_membership)
                { return locate_local_collection_member(stores, store_count, reference, kind); }
#endif
                auto const bucket{bucket_index(reference.storage.ptr)};
                global_guard guard{bucket};
                for(auto* current{global_buckets_[bucket]}; current != nullptr;)
                {
                    // [registered global chain] stripe excludes unlink/free.
                    // The untrusted token is compared, never dereferenced.
                    if(current->token == reference.storage.ptr)
                    {
                        // A teardown that has already left admission may still
                        // have a protected global-index node. Compare its owner
                        // identity against canonical pins before returning it;
                        // never dereference an owner outside this closed cohort.
                        bool included{};
                        for(::std::size_t index{}; index != store_count; ++index)
                        { included = included || current->owner == stores[index].get(); }
                        return included && current->kind == kind ? current : nullptr;
                    }
                    // ^^ follow only the registered initialized native link.
                    current = current->global_next;
                }
                return nullptr;
            }};
            auto validate_reference{[&](gc_reference reference) noexcept
            { return non_object(reference)
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
                     || graph.find(reference) != nullptr
#endif
                     || locate(reference) != nullptr; }};
            auto visit_fields_uncached{[](object const& current, auto&& visitor) noexcept
            {
                auto const* layout{current.owner->checked_type(current.type_index, current.kind)};
                if(layout == nullptr || (current.length != 0uz && !current.values) ||
                   (current.kind == gc_type::composite_kind::struct_ && current.length != layout->field_count) ||
                   (current.kind == gc_type::composite_kind::array && layout->field_count != 1uz))
                { return false; }
#if defined(UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA == 1
                // [authentic stopped legacy header][immutable canonical layout]
                // [safe ] no callback/poll/unlock or mutation intervenes. Shape
                // agreement checks privately owned plan consistency only;
                // identity comes from private type_layout embedding and its
                // canonical construction. EVERY real reference still passes
                // the original visitor; shape is never reference authority.
                if(!layout->trace.matches_shape(current.kind, layout->field_count)) { return false; }
#endif
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
                if(current.numeric_array_)
                {
                    auto const width{numeric_array_width(layout)};
                    // [real checked numeric array][original bounded byte tail]
                    // [safe ] no typed carrier or root can be borrowed from
                    // this representation; canonical type proves zero edges.
                    return width != 0uz && current.length <=
                        (static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) -
                         object_value_offset) / width;
                }
#endif
#if defined(UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA == 1
                return visit_precise_reference_fields(current, *layout, visitor);
#else
                // Numeric layouts have no reference edges. Shape/storage checks above still run.
                // Copied layouts remain immutable across every collection epoch.
                if(!layout->has_reference_fields) { return true; }
                for(::std::size_t index{}; index != current.length; ++index)
                {
                    // [0,length) owns initialized values; the checked layout
                    // owns one array descriptor or exactly length struct fields.
                    auto const storage{layout->fields[current.kind == gc_type::composite_kind::array ? 0uz : index].storage};
                    if(storage.packed == gc_type::packed_kind::none &&
                       storage.value.kind == gc_type::value_kind::reference &&
                       !visitor(current.values[index].as<gc_reference>())) { return false; }
                }
#endif
                return true;
            }};

            auto visit_fields{[&](auto const& view, auto&& visitor) noexcept
            {
#if defined(UWVM_EXPERIMENTAL_GENERAL_GC_SLAB) && UWVM_EXPERIMENTAL_GENERAL_GC_SLAB == 1
                // Collector-only idempotent callbacks; closed cohort stays
                // stopped for this WHOLE traversal. Original shape checks and
                // every field read still run. Remember only the last successful
                // opaque token, never a header/pin or cross-traversal authority.
                bool previous_valid{};
                ref_kind previous_kind{};
                void* previous_token{};
                return visit_fields_uncached(view, [&](gc_reference reference) noexcept
                {
                    bool const aggregate{reference.kind == ref_kind::wasm_struct ||
                                         reference.kind == ref_kind::wasm_array ||
                                         reference.kind == ref_kind::wasm_exn};
                    if(aggregate && previous_valid && reference.kind == previous_kind &&
                       reference.storage.ptr == previous_token) { return true; }
                    if(!visitor(reference)) { return false; }
                    previous_valid = aggregate;
                    if(aggregate)
                    {
                        previous_kind = reference.kind;
                        previous_token = reference.storage.ptr;
                    }
                    return true;
                });
#else
                return visit_fields_uncached(view, visitor);
#endif
            }};
            ::std::size_t object_count{};
            auto constexpr max_work{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(object*)};
            // Validate the entire graph and precise roots before reclamation.
            // Failure can clear marks, but cannot delete or invalidate a token.
            for(::std::size_t index{}; index != store_count; ++index)
            {
                for(auto* current{stores[index]->objects_.load(::std::memory_order_acquire)}; current != nullptr;)
                {
                    if(current->owner != stores[index].get() || !visit_fields(*current, validate_reference))
                    { return gc_object_status::invalid_reference; }
                    if(object_count == max_work) { return gc_object_status::size_overflow; }
                    ++object_count;
                    current->marked = false;
                    // [quiescent store-owned object chain] canonical pins keep
                    // every header alive until the sweep completes.
                    // ^^ advance only through its initialized owned next link.
                    current = current->next;
                }
            }
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
            if(!graph.validate_all_values(validate_reference)) { return gc_object_status::invalid_reference; }
#endif
            // At most one work entry per proved live object. Work storage is
            // collection-only, bounded by current live headers, and freed after
            // this pass; no per-issued-token bitmap/index accumulates forever.
            ::std::unique_ptr<object*[]> work{};
            if(object_count != 0uz)
            {
                work.reset(new(::std::nothrow) object*[object_count]);
                if(!work)
                {
                    // Preserve the old invalid-root-before-work-OOM ordering.
                    // Successful allocation authenticates roots in mark below;
                    // this cold failure path cannot enter epoch commit/sweep.
                    for(::std::size_t index{}; index != root_count; ++index)
                    {
                        // [roots,roots+root_count) is the checked, complete
                        // stopped snapshot; index<root_count borrows one live
                        // carrier. No caller storage changes before return.
                        if(!validate_reference(roots[index])) { return gc_object_status::invalid_reference; }
                    }
                    return gc_object_status::out_of_memory;
                }
            }
            ::std::size_t work_count{};
            auto mark{[&](gc_reference reference) noexcept
            {
                if(non_object(reference)) { return true; }
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
                if(reference.kind == ref_kind::wasm_exn) { return graph.mark(reference); }
#endif
                auto* child{locate(reference)};
                if(child == nullptr) { return false; }
                if(child->marked) { return true; }
                if(work_count == object_count) { ::std::terminate(); }
                child->marked = true;
                // [0,object_count) owns native work slots; each object is added
                // once, and work_count is proved below the allocated bound.
                work[work_count++] = child;
                return true;
            }};
            for(::std::size_t index{}; index != root_count; ++index)
            {
                // [roots,roots+root_count) checked snapshot remains stopped.
                // [safe ] index<root_count borrows one complete carrier; mark
                // authenticates membership before use. A later failure may
                // leave temporary marks, but cannot reclaim or change any token.
                if(!mark(roots[index])) { return gc_object_status::invalid_reference; }
            }
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
            if(!graph.visit_native_values(mark)) { return gc_object_status::invalid_reference; }
#endif
            while(work_count != 0uz
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
                  || graph.has_work()
#endif
                 )
            {
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
                if(work_count == 0uz)
                {
                    if(!graph.pop_and_trace(mark)) { return gc_object_status::invalid_reference; }
                    continue;
                }
#endif
                // [0,work_count) contains proved live native object pointers.
                // ^^ pop a complete initialized slot before tracing its fields.
                auto* current{work[--work_count]};
                if(!visit_fields(*current, mark)) { return gc_object_status::invalid_reference; }
            }
            // The closed-cohort caller has stopped all mutators. Validate all
            // reservation/epoch states before mutating any store, then invalidate
            // future cached allocation tickets before unlinking/freeing objects.
            for(::std::size_t index{}; index != store_count; ++index)
            { if(!stores[index]->can_advance_slab_epoch_exclusive()) { return gc_object_status::invalid_store; } }
            for(::std::size_t index{}; index != store_count; ++index)
            { stores[index]->advance_slab_epoch_exclusive(); }
            for(::std::size_t index{}; index != store_count; ++index)
            {
                auto& store{*stores[index]};
#if defined(UWVM_EXPERIMENTAL_GENERAL_GC_SLAB) && UWVM_EXPERIMENTAL_GENERAL_GC_SLAB == 1
                sweep_retirement_batch retirement{store};
#endif
                object* kept{};
                object** tail{::std::addressof(kept)};
                for(auto* current{store.objects_.load(::std::memory_order_relaxed)}; current != nullptr;)
                {
                    // [owned live headers] save next before possibly freeing current.
                    auto* next{current->next};
                    if(current->marked)
                    {
                        current->marked = false;
                        // [native kept pointer slot] tail belongs to this sweep.
                        // ^^ append before moving tail to current's owned next slot.
                        *tail = current;
                        tail = ::std::addressof(current->next);
                        // Old foreign values must not pin otherwise dead stores
                        // forever after field overwrite. Only current validated
                        // typed aggregate fields justify an object-owned lease.
                        current->value_leases.prune_exclusive([&](gc_object_store const* leased) noexcept
                        {
                            bool used{};
                            auto const valid{visit_fields(*current, [&](gc_reference reference) noexcept
                            {
                                if(!non_object(reference))
                                {
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
                                    if(reference.kind == ref_kind::wasm_exn)
                                    {
                                        auto const* issuer{graph.issuer(reference)};
                                        if(issuer == nullptr) { return false; }
                                        used = used || issuer == leased;
                                        return true;
                                    }
#endif
                                    auto* member{locate(reference)};
                                    if(member == nullptr) { return false; }
                                    used = used || member->owner == leased;
                                }
                                return true;
                            })};
                            if(!valid) { ::std::terminate(); }
                            return used;
                        });
                    }
                    else
                    {
                        auto const bucket_index_value{bucket_index(current->token)};
                        auto& bucket{store.membership_buckets_[bucket_index_value]};
                        auto* head{bucket.load(::std::memory_order_relaxed)};
                        auto** link{::std::addressof(head)};
                        while(*link != current)
                        {
                            if(*link == nullptr) { ::std::terminate(); }
                            // [protected local membership] every predecessor is live.
                            // ^^ move to a proved native hash_next pointer slot.
                            link = ::std::addressof((*link)->hash_next);
                        }
                        // [predecessor][retired][successor] guest token stops resolving.
                        // ^^ splice local membership before releasing object values.
                        *link = current->hash_next;
                        bucket.store(head, ::std::memory_order_release);
                        {
                            global_guard guard{bucket_index_value};
                            auto** global_link{::std::addressof(global_buckets_[bucket_index_value])};
                            while(*global_link != current)
                            {
                                if(*global_link == nullptr) { ::std::terminate(); }
                                // [protected foreign membership] every node is live.
                                // ^^ move to a proved native global_next pointer slot.
                                global_link = ::std::addressof((*global_link)->global_next);
                            }
                            // [global predecessor][retired][global successor]
                            // ^^ splice foreign membership before freeing the native header.
                            *global_link = current->global_next;
                        }
                        // Every store is still canonically pinned. Releasing a
                        // foreign value lease cannot destroy a later cohort node.
#if defined(UWVM_EXPERIMENTAL_GENERAL_GC_SLAB) && UWVM_EXPERIMENTAL_GENERAL_GC_SLAB == 1
                        retirement.destroy_unlinked(current);
#else
                        destroy_object(current);
#endif
                        ++reclaimed;
                    }
                    // [remaining owned headers] next was read before deletion.
                    // ^^ advance only to a still-live native header or null.
                    current = next;
                }
                // [last kept pointer slot] terminate before publishing the rebuilt chain.
                // ^^ tail names kept or the final retained object's next member.
                *tail = nullptr;
                store.objects_.store(kept, ::std::memory_order_release);
            }
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
            graph.sweep_registry_after_heap_commit();
#endif
            return gc_object_status::ok;
        
#endif
        }
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) && UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH == 1
    public:
#endif
        // Catch_ref creates one module-rooted token. Its payload is an integer
        // identity; a guest-supplied pointer can never be dereferenced by lookup.
        [[nodiscard]] inline gc_object_status make_exn_reference(
            ::uwvm2::runtime::exception::value_ref const& value,
            gc_reference& result) const noexcept
        {
            if(!valid_) { return gc_object_status::invalid_store; }
            if(!value) { return gc_object_status::invalid_value; }
#if defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) && UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES == 1
            auto record{::uwvm2::runtime::exception::external_exception_handle::record_from_privileged_native(value)};
            if(!record) { return gc_object_status::invalid_value; }
#endif
            auto owner{weak_from_this()};
            if(owner.expired()) { return gc_object_status::invalid_store; }
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
            // The value's real external block still pins this lifetime through
            // capture; generic internal records produce no source authority.
            auto provenance{::uwvm2::runtime::exception::external_exception_handle::source_native_provenance(value)};
#endif
            ::std::unique_ptr<exn_token_entry> entry{new(::std::nothrow) exn_token_entry{}};
            ::std::unique_ptr<exn_root_node> root{new(::std::nothrow) exn_root_node{}};
            if(!entry || !root) { return gc_object_status::out_of_memory; }
#if defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) && UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES == 1
            entry->value = ::std::move(record);
#else
            entry->value = value;
#endif
            entry->owner = ::std::move(owner);
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
            entry->source_origin = provenance.lifetime;
            entry->source_native_leaf_certificate = provenance.certificate;
#endif
            exn_guard registry_guard{};
            if(next_exn_id_ > token_payload_mask) { return gc_object_status::size_overflow; }
            exn_roots_guard roots_guard{*this};
            if(!exn_root_buckets_)
            {
                exn_root_buckets_.reset(new(::std::nothrow) exn_root_bucket_array{});
                if(!exn_root_buckets_) { return gc_object_status::out_of_memory; }
            }
            auto const identity{exn_token_prefix | next_exn_id_++};
            entry->token = identity;
            entry->root_count = 1uz;
            auto& bucket{exn_buckets_[exn_bucket(identity)]};
            // [fully initialized entry][registered exn bucket]
            // ^^ publish under exn_lock_ before producing a guest-visible token.
            entry->next = bucket;
            auto* published{entry.release()};
            bucket = published;
            root->entry = published;
            // [module root head] insertion retains this token until reset.
            // ^^ root advances to the old live head before publication.
            auto const bucket_index{exn_bucket(identity)};
            root->bucket_next = (*exn_root_buckets_)[bucket_index];
            (*exn_root_buckets_)[bucket_index] = root.get();
            root->next = exn_roots_;
            exn_roots_ = root.release();
            result.storage.ptr = reinterpret_cast<void*>(identity);
            result.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_exn;
            return gc_object_status::ok;
        }
        [[nodiscard]] static inline ::uwvm2::runtime::exception::value_ref
            lookup_exn_reference(gc_reference reference) noexcept
        {
            if(reference.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_exn ||
               !exn_token_shape(reference.storage.ptr)) { return {}; }
            exn_guard guard{};
            auto const* entry{find_exn_locked(reference.storage.ptr)};
#if defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) && UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES == 1
            // Compatibility lookup is INTERNAL/UNKNOWN; fresh raw throw_ref
            // cannot become a registered producer merely from a recipient token.
            return entry == nullptr ? ::uwvm2::runtime::exception::value_ref{} : entry->value.native_untracked_owner();
#else
            return entry == nullptr ? ::uwvm2::runtime::exception::value_ref{} : entry->value;
#endif
        }
#if defined(UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS) && UWVM_EXPERIMENTAL_LOCAL_FULL_GC_EXCEPTIONS == 1
        // Only the issuing canonical store + exact captured source control block
        // authorize token re-materialization. No integer or equal tag signature does.
        [[nodiscard]] ::uwvm2::runtime::exception::immutable_record_ref
            lookup_local_source_exception(gc_reference reference,
                ::std::shared_ptr<::uwvm2::runtime::exception::external_exception_lifetime const> const& expected,
                void const** leaf_certificate = nullptr) const noexcept
        {
            // [optional native output pointer slot] actual runtime stack storage;
            // no guest address or advancing pointer is accepted by this native API.
            if(leaf_certificate != nullptr) { *leaf_certificate = nullptr; }
            if(reference.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_exn ||
               !exn_token_shape(reference.storage.ptr) || !expected) { return {}; }
            ::std::shared_ptr<gc_object_store const> issuer;
            ::std::shared_ptr<::uwvm2::runtime::exception::external_exception_lifetime const> origin;
            ::uwvm2::runtime::exception::immutable_record_ref record;
            {
                exn_guard guard{};
                // [actual registered native node/null] lookup hashes opaque token
                // bytes; no guest address is dereferenced or advanced here.
                auto const* entry{find_exn_locked(reference.storage.ptr)};
                if(entry == nullptr) { return {}; }
                issuer = entry->owner.lock(); origin = entry->source_origin.lock();
                auto const canonical{weak_from_this()};
                if(issuer && issuer.get() == this && !issuer.owner_before(canonical) && !canonical.owner_before(issuer) &&
                   origin && origin.get() == expected.get() && !origin.owner_before(expected) && !expected.owner_before(origin))
                {
                    record = entry->value;
                    // [native output pointer slot] one complete initialized slot;
                    // certificate was captured from the exact genuine external block.
                    if(leaf_certificate != nullptr) { *leaf_certificate = entry->source_native_leaf_certificate; }
                }
            }
            // Unsuccessful foreign issuer/source temporary pins release only
            // AFTER exn_guard, so their native destructors never reenter its lock.
            return record;
        }
#endif
        // A local reference is already owned by the issuing module. Taking a
        // shared_ptr back to this store from one of its exn payload fields
        // would create store -> token -> value -> store and prevent teardown.
        // Foreign values still receive an independent root.
        [[nodiscard]] inline ::uwvm2::runtime::exception::instance_root
            root_reference(gc_reference reference) const noexcept
        {
#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1

            using kind = ::uwvm2::object::global::wasm_ref_kind;
            if(reference.kind == kind::wasm_struct || reference.kind == kind::wasm_array)
            {
                auto const object{checked_aggregate_object(reference, reference.kind == kind::wasm_struct ?
                    gc_type::composite_kind::struct_ : gc_type::composite_kind::array)};
                if(!object || object.owner() == this) { return {}; }
                auto owner{object.owner()->weak_from_this().lock()};
                return ::std::static_pointer_cast<void const>(::std::move(owner));
            }
            if(reference.kind == kind::wasm_exn)
            {
                {
                    exn_guard guard{};
                    auto const* entry{find_exn_locked(reference.storage.ptr)};
                    if(entry == nullptr || entry->owner.lock().get() == this) { return {}; }
                }
                auto value{lookup_exn_reference(reference)};
                return ::std::static_pointer_cast<void const>(::std::move(value));
            }
            if(reference.kind == kind::wasm_extern && bridge_token_shape(reference.storage.ptr))
            {
                bridge_guard guard{};
                auto const* bridge{find_bridge_token_locked(reference.storage.ptr)};
                if(bridge == nullptr) { return {}; }
                if(bridge->owner == this) { return {}; }
                auto owner{bridge->owner->weak_from_this().lock()};
                return ::std::static_pointer_cast<void const>(::std::move(owner));
            }
            return {};
        
#else

            using kind = ::uwvm2::object::global::wasm_ref_kind;
            if(reference.kind == kind::wasm_struct || reference.kind == kind::wasm_array)
            {
                auto const* object{checked_object(reference, reference.kind == kind::wasm_struct ?
                    gc_type::composite_kind::struct_ : gc_type::composite_kind::array)};
                if(object == nullptr) { return {}; }
                if(object->owner == this) { return {}; }
                auto owner{object->owner->weak_from_this().lock()};
                return ::std::static_pointer_cast<void const>(::std::move(owner));
            }
            if(reference.kind == kind::wasm_exn)
            {
                {
                    exn_guard guard{};
                    auto const* entry{find_exn_locked(reference.storage.ptr)};
                    if(entry == nullptr || entry->owner.lock().get() == this) { return {}; }
                }
                auto value{lookup_exn_reference(reference)};
                return ::std::static_pointer_cast<void const>(::std::move(value));
            }
            if(reference.kind == kind::wasm_extern && bridge_token_shape(reference.storage.ptr))
            {
                bridge_guard guard{};
                auto const* bridge{find_bridge_token_locked(reference.storage.ptr)};
                if(bridge == nullptr) { return {}; }
                if(bridge->owner == this) { return {}; }
                auto owner{bridge->owner->weak_from_this().lock()};
                return ::std::static_pointer_cast<void const>(::std::move(owner));
            }
            return {};
        
#endif
        }

#if defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) && UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION == 1
        // Cold actual-source payload builder only. Caller owns the real store
        // AND runtime_gc_entry_admission reader lease. No returned pointer, no
        // foreign retain/callback/allocation, and no collection authorization.
        [[nodiscard]] bool exception_payload_is_local_member(gc_reference reference) const noexcept
        {
            if(!valid_) { return false; }
            using kind=::uwvm2::object::global::wasm_ref_kind;
            switch(reference.kind)
            {
                case kind::wasm_null: case kind::wasm_i31: return true;
                case kind::wasm_struct:
#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1
                    if(checked_local_compact_object(reference)) { return true; }
#endif
                    return checked_local_object(reference,gc_type::composite_kind::struct_)!=nullptr;
                case kind::wasm_array:
                    return checked_local_object(reference,gc_type::composite_kind::array)!=nullptr;
                default: return false; // exn/extern/funcref need separate complete origin proof
            }
        }
#endif

        // Stable canonical IDs are shared by stores in this linking domain.
        // Exact ID equality is suitable for rich function-signature identity;
        // value subtype checks should continue to use reference_type_matches.
        [[nodiscard]] inline ::std::size_t canonical_type_id(::std::uint_least32_t local_index) const noexcept
        {
            if(!valid_ || local_index >= layout_count_ || !canonical_ids_)
            { return (::std::numeric_limits<::std::size_t>::max)(); }
            // [0, layout_count_) constructor filled this immutable canonical ID map.
            return canonical_ids_[local_index];
        }
        [[nodiscard]] static inline bool canonical_defined_type_equal(gc_object_store const* lhs,
            ::std::uint_least32_t lhs_index, gc_object_store const* rhs,
            ::std::uint_least32_t rhs_index) noexcept
        {
            if(lhs == nullptr || rhs == nullptr) { return false; }
            auto const lhs_id{lhs->canonical_type_id(lhs_index)};
            return lhs_id != (::std::numeric_limits<::std::size_t>::max)() &&
                   lhs_id == rhs->canonical_type_id(rhs_index);
        }
        // Cold call-target admission only. Both indices designate complete,
        // immutable function declarations, never guest object addresses.
        // Table publication projects this relation into a caller-owned interval;
        // generated calls perform an integer check without a registry lock.
        [[nodiscard]] static inline bool canonical_function_type_matches(
            gc_object_store const* actual_owner, ::std::uint_least32_t actual,
            gc_object_store const* expected_owner, ::std::uint_least32_t expected) noexcept
        {
            if(actual_owner == nullptr || expected_owner == nullptr ||
               actual_owner->checked_type(actual, gc_type::composite_kind::function) == nullptr ||
               expected_owner->checked_type(expected, gc_type::composite_kind::function) == nullptr)
            { return false; }
            // [checked function layout in each pinned store] respective one-past
            // [safe                                       ] indices were bounded above;
            // no layout pointer escapes the canonical comparison or is advanced here.
            return canonical_subtype(actual_owner, actual, expected_owner, expected);
        }
        // Cold declared-value admission across two actual pinned module stores.
        // A local flat index alone cannot identify a type in another module.
        // This proves only the Core 3 declaration relation, never native value
        // membership, root liveness or checkpoint execution authority.
        [[nodiscard]] static inline bool canonical_value_type_matches(
            gc_object_store const* actual_owner, gc_type::core_value_type actual,
            gc_object_store const* expected_owner, gc_type::core_value_type expected) noexcept
        {
            using kind = gc_type::value_kind;
            using heap = gc_type::abstract_heap_type;
            namespace match = ::uwvm2::validation::standard::wasm3::recursive_validation_details;
            if(actual.kind != expected.kind) { return false; }
            if(actual.kind != kind::reference) { return actual.kind <= kind::v128 && actual == expected; }
            if(actual.nullable && !expected.nullable) { return false; }
            if(!actual.heap.is_defined() && !expected.heap.is_defined())
            { return match::abstract_matches(actual.heap, expected.heap); }
            auto const valid_defined{[](gc_object_store const* owner, gc_type::heap_type type) noexcept
            {
                return owner != nullptr && owner->valid_ && type.is_defined() &&
                    static_cast<::std::uint_least64_t>(type.code) < owner->layout_count_ &&
                    static_cast<::std::uint_least64_t>(type.code) <= UINT_LEAST32_MAX;
            }};
            if(actual.heap.is_defined() && !valid_defined(actual_owner, actual.heap)) { return false; }
            if(expected.heap.is_defined() && !valid_defined(expected_owner, expected.heap)) { return false; }
            if(actual.heap.is_defined() && expected.heap.is_defined())
            {
                return canonical_subtype(actual_owner, static_cast<::std::uint_least32_t>(actual.heap.code),
                    expected_owner, static_cast<::std::uint_least32_t>(expected.heap.code));
            }
            if(actual.heap.is_defined())
            {
                // [actual immutable owned type layouts0..N] end
                // [safe] full flat-index and u32 bounds BEFORE metadata selection;
                // this is a declaration record, never a guest object address.
                auto const composite{actual_owner->layouts_[static_cast<::std::uint_least32_t>(actual.heap.code)].kind};
                auto const base{composite == gc_type::composite_kind::function ? heap::func :
                    composite == gc_type::composite_kind::struct_ ? heap::struct_ : heap::array};
                return match::abstract_matches({static_cast<::std::int_least64_t>(base)}, expected.heap);
            }
            // [expected immutable owned type layouts0..N] end
            // [safe] complete bounds above BEFORE selecting the expected family.
            auto const composite{expected_owner->layouts_[static_cast<::std::uint_least32_t>(expected.heap.code)].kind};
            auto const bottom{composite == gc_type::composite_kind::function ? heap::nofunc : heap::none};
            return actual.heap.code == static_cast<::std::int_least64_t>(bottom);
        }
        // This checked predicate is for validated cast/test instructions and host bridges.
        // It verifies aggregate-object membership before reading any payload as an object.
        [[nodiscard]] inline bool reference_type_matches(gc_reference reference,
            gc_type::core_value_type expected) const noexcept
        {
            if(!valid_ || expected.kind != gc_type::value_kind::reference) { return false; }
            auto const heap_code{expected.heap.code};
            if(heap_code >= 0)
            {
                // A validated defined heap is an exact flat type index. Host callers cannot
                // narrow an arbitrary signed-64 value into an unrelated local u32 index.
                if(static_cast<::std::uint_least64_t>(heap_code) >= layout_count_) { return false; }
            }
            else if(heap_code < static_cast<::std::int_least64_t>(gc_type::abstract_heap_type::exn) ||
                    heap_code > static_cast<::std::int_least64_t>(gc_type::abstract_heap_type::noexn))
            { return false; }
            return reference_matches(reference, expected);
        }

        // Compiler-only cast witness, never a Wasm value/guest address. 0 is a
        // failed cast; 1 is a successful cast with no native field borrow. A
        // larger result is checked initialized immutable numeric storage in
        // this LOCAL store: a legacy carrier array, or ONLY a single raw32 cell
        // for a compact one-field target. The compact borrow is valid for one
        // immediate four-byte field-zero load; it is NEVER a carrier array and
        // cannot justify advancing by sizeof(gc_object_value). Retain the
        // complete reference and consume this borrow before ANY call, allocation,
        // collection/pause point, mutation or control-flow change. It must not
        // retain it in locals, tables, globals, a Wasm operand or a host handle.
        // This first witness accepts only a 1..8-field immutable numeric target
        // prefix. An actual subtype may have other fields after this prefix;
        // no borrow consumer may access them. Larger/mutable/reference/foreign/null
        // cases keep ordinary checked
        // field access, including its ownership and mutation-lock protocol.
        [[nodiscard]] inline ::std::uintptr_t native_immutable_numeric_struct_cast_values(
            gc_reference reference, ::std::uint_least32_t expected_index,
            bool nullable) const noexcept
        {
#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1

            using kind = ::uwvm2::object::global::wasm_ref_kind;
            auto const* expected{checked_type(expected_index, gc_type::composite_kind::struct_)};
            if(expected == nullptr) { return 0u; }
            if(reference.kind == kind::wasm_null) { return nullable ? 1u : 0u; }
            if(auto const view{checked_local_compact_object(reference)}; view)
            {
                if(!canonical_subtype(this, view.type_index(), this, expected_index)) { return 0u; }
                auto const* actual{checked_type(view.type_index(), gc_type::composite_kind::struct_)};
                if(expected->field_count != 1uz || !compact_layout(actual) ||
                   !compact_layout(expected) || actual->fields[0uz].storage != expected->fields[0uz].storage)
                { return 1u; }
                // [one raw32 CELL][one-past] exactly FOUR bytes, not a carrier
                // array. A target with two or more fields never receives this
                // borrow. The emitter may load only field zero before ANY call,
                // poll, allocation or control change; real outer entry/root
                // exclusion pins this nonmoving range through that one load.
                auto const* address{view.compact->payload_.address_of_cell(view.compact_slot)};
                auto const raw{reinterpret_cast<::std::uintptr_t>(address)};
                return raw > 1u ? raw : 1u;
            }
            auto* header{checked_local_object(reference, gc_type::composite_kind::struct_)};
            if(header == nullptr)
            {
                // A foreign object must retain the canonical foreign owner.
                // The ordinary predicate does that; do not export its body as
                // a witness or infer liveness from an opaque guest token.
                return reference_type_matches(reference,
                    {gc_type::value_kind::reference, {expected_index}, nullable}) ? 1u : 0u;
            }
            if(!canonical_subtype(header->owner, header->type_index, this, expected_index))
            { return 0u; }
            auto const* actual{checked_type(header->type_index, gc_type::composite_kind::struct_)};
            if(actual == nullptr || expected->field_count == 0uz || expected->field_count > 8uz ||
               expected->field_count > header->length || expected->field_count > actual->field_count ||
               header->values.get() == nullptr) { return 1u; }
            for(::std::size_t index{}; index != expected->field_count; ++index)
            {
                // [canonical target prefix: field_count][actual live fields]
                // [safe                                                   ]
                // both immutable layouts and the full actual array extent were
                // checked above; no object/carrier pointer advances here.
                auto const& want{expected->fields[index]};
                auto const& have{actual->fields[index]};
                if(want.mutable_ || have.mutable_ || want.storage != have.storage)
                { return 1u; }
                if(want.storage.packed == gc_type::packed_kind::none &&
                   want.storage.value.kind != gc_type::value_kind::i32 &&
                   want.storage.value.kind != gc_type::value_kind::f32)
                { return 1u; }
            }
            // [LOCAL canonical header][actual initialized carrier ARRAY] end
            // [safe                                                   ] the
            // executing module owns this store, the immutable fields cannot be
            // concurrently written, and the native caller excludes collection
            // for this leaf borrow. Its original array pointer is preserved;
            // no header-derived arithmetic, foreign raw pointer or recycled
            // object address is used as a guest reference.
            auto const address{reinterpret_cast<::std::uintptr_t>(header->values.get())};
            return address > 1u ? address : 1u;
        
#else

            using kind = ::uwvm2::object::global::wasm_ref_kind;
            auto const* expected{checked_type(expected_index, gc_type::composite_kind::struct_)};
            if(expected == nullptr) { return 0u; }
            if(reference.kind == kind::wasm_null) { return nullable ? 1u : 0u; }
            auto* header{checked_local_object(reference, gc_type::composite_kind::struct_)};
            if(header == nullptr)
            {
                // A foreign object must retain the canonical foreign owner.
                // The ordinary predicate does that; do not export its body as
                // a witness or infer liveness from an opaque guest token.
                return reference_type_matches(reference,
                    {gc_type::value_kind::reference, {expected_index}, nullable}) ? 1u : 0u;
            }
            if(!canonical_subtype(header->owner, header->type_index, this, expected_index))
            { return 0u; }
            auto const* actual{checked_type(header->type_index, gc_type::composite_kind::struct_)};
            if(actual == nullptr || expected->field_count == 0uz || expected->field_count > 8uz ||
               expected->field_count > header->length || expected->field_count > actual->field_count ||
               header->values.get() == nullptr) { return 1u; }
            for(::std::size_t index{}; index != expected->field_count; ++index)
            {
                // [canonical target prefix: field_count][actual live fields]
                // [safe                                                   ]
                // both immutable layouts and the full actual array extent were
                // checked above; no object/carrier pointer advances here.
                auto const& want{expected->fields[index]};
                auto const& have{actual->fields[index]};
                if(want.mutable_ || have.mutable_ || want.storage != have.storage)
                { return 1u; }
                if(want.storage.packed == gc_type::packed_kind::none &&
                   want.storage.value.kind != gc_type::value_kind::i32 &&
                   want.storage.value.kind != gc_type::value_kind::f32)
                { return 1u; }
            }
            // [LOCAL canonical header][actual initialized carrier ARRAY] end
            // [safe                                                   ] the
            // executing module owns this store, the immutable fields cannot be
            // concurrently written, and the native caller excludes collection
            // for this leaf borrow. Its original array pointer is preserved;
            // no header-derived arithmetic, foreign raw pointer or recycled
            // object address is used as a guest reference.
            auto const address{reinterpret_cast<::std::uintptr_t>(header->values.get())};
            return address > 1u ? address : 1u;
        
#endif
        }
        // Call when a GC/extern reference first crosses a module boundary,
        // before the source module can unload. Reading it later also acquires
        // a lease, but a later lookup cannot resurrect a retired arena.
        [[nodiscard]] inline gc_object_status retain_gc_reference(gc_reference reference) const noexcept
        {
#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1

            using kind = ::uwvm2::object::global::wasm_ref_kind;
            if(!valid_) { return gc_object_status::invalid_store; }
            if(reference.kind == kind::wasm_struct || reference.kind == kind::wasm_array)
            {
                auto const object{checked_aggregate_object(reference, reference.kind == kind::wasm_struct ?
                    gc_type::composite_kind::struct_ : gc_type::composite_kind::array)};
                return !object ? gc_object_status::invalid_reference : gc_object_status::ok;
            }
            if(reference.kind == kind::wasm_extern)
            {
                if(reference.storage.ptr == nullptr) { return gc_object_status::invalid_reference; }
                if(bridge_token_shape(reference.storage.ptr))
                {
                    bridge_guard guard{};
                    auto const* bridge{find_bridge_token_locked(reference.storage.ptr)};
                    if(bridge == nullptr) { return gc_object_status::invalid_reference; }
                    return retain_foreign_bridge_locked(bridge);
                }
                return gc_object_status::ok; // Opaque host payload; never dereferenced.
            }
            if(reference.kind == kind::wasm_exn) { return retain_exn_reference(reference); }
            if(reference.kind == kind::wasm_null || reference.kind == kind::wasm_i31 ||
               reference.kind == kind::wasm_func || reference.kind == kind::wasm_func_imported ||
               reference.kind == kind::wasm_func_defined)
            { return gc_object_status::ok; }
            return gc_object_status::invalid_reference;
        
#else

            using kind = ::uwvm2::object::global::wasm_ref_kind;
            if(!valid_) { return gc_object_status::invalid_store; }
            if(reference.kind == kind::wasm_struct || reference.kind == kind::wasm_array)
            {
                auto const* object{checked_object(reference, reference.kind == kind::wasm_struct ?
                    gc_type::composite_kind::struct_ : gc_type::composite_kind::array)};
                return object == nullptr ? gc_object_status::invalid_reference : gc_object_status::ok;
            }
            if(reference.kind == kind::wasm_extern)
            {
                if(reference.storage.ptr == nullptr) { return gc_object_status::invalid_reference; }
                if(bridge_token_shape(reference.storage.ptr))
                {
                    bridge_guard guard{};
                    auto const* bridge{find_bridge_token_locked(reference.storage.ptr)};
                    if(bridge == nullptr) { return gc_object_status::invalid_reference; }
                    return retain_foreign_bridge_locked(bridge);
                }
                return gc_object_status::ok; // Opaque host payload; never dereferenced.
            }
            if(reference.kind == kind::wasm_exn) { return retain_exn_reference(reference); }
            if(reference.kind == kind::wasm_null || reference.kind == kind::wasm_i31 ||
               reference.kind == kind::wasm_func || reference.kind == kind::wasm_func_imported ||
               reference.kind == kind::wasm_func_defined)
            { return gc_object_status::ok; }
            return gc_object_status::invalid_reference;
        
#endif
        }
        // Core 3 any.convert_extern. A host extern payload is opaque: only a
        // token proven to be in the live bridge index may be unwrapped.
        [[nodiscard]] inline gc_object_status any_convert_extern(gc_reference source,
            gc_reference& result) const noexcept
        {
            using kind = ::uwvm2::object::global::wasm_ref_kind;
            if(!valid_) { return gc_object_status::invalid_store; }
            if(source.kind == kind::wasm_null) { result = source; return gc_object_status::ok; }
            if(source.kind != kind::wasm_extern || source.storage.ptr == nullptr)
            { return gc_object_status::invalid_reference; }
            {
                bridge_guard guard{};
                auto const* bridge{find_bridge_token_locked(source.storage.ptr)};
                if(bridge != nullptr)
                {
                    auto const retention{retain_foreign_bridge_locked(bridge)};
                    if(retention != gc_object_status::ok) { return retention; }
                    // [registered live bridge] its owner has not run teardown
                    // while bridge_lock_ is held; copy the full 16-byte carrier.
                    result = bridge->inner;
                    return gc_object_status::ok;
                }
            }
            // A former VM token must not silently become a host external value
            // after its originating module is destroyed. Arbitrary host
            // pointers are otherwise never read or dereferenced.
            if(bridge_token_shape(source.storage.ptr)) { return gc_object_status::invalid_reference; }
            result = source;
            return gc_object_status::ok;
        }
        // Core 3 extern.convert_any. Existing wrappers are canonical across
        // live modules. Aggregate payloads must be published by this store or
        // already represented by a checked bridge from another live store.
        [[nodiscard]] inline gc_object_status extern_convert_any(gc_reference source,
            gc_reference& result) const noexcept
        {
#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1

            using kind = ::uwvm2::object::global::wasm_ref_kind;
            if(!valid_) { return gc_object_status::invalid_store; }
            if(source.kind == kind::wasm_null) { result = source; return gc_object_status::ok; }
            if(source.kind == kind::wasm_extern)
            {
                if(source.storage.ptr == nullptr) { return gc_object_status::invalid_reference; }
                if(bridge_token_shape(source.storage.ptr))
                {
                    bridge_guard guard{};
                    auto const* bridge{find_bridge_token_locked(source.storage.ptr)};
                    if(bridge == nullptr) { return gc_object_status::invalid_reference; }
                    auto const retention{retain_foreign_bridge_locked(bridge)};
                    if(retention != gc_object_status::ok) { return retention; }
                }
                result = source; // Opaque host external values preserve pointer identity.
                return gc_object_status::ok;
            }
            bool locally_published{source.kind == kind::wasm_i31};
            ::std::shared_ptr<gc_object_store const> inner_owner{};
            if(source.kind == kind::wasm_struct || source.kind == kind::wasm_array)
            {
                auto const object{checked_aggregate_object(source, source.kind == kind::wasm_struct ?
                    gc_type::composite_kind::struct_ : gc_type::composite_kind::array)};
                locally_published = static_cast<bool>(object);
                if(object && object.owner() != this)
                {
                    inner_owner = object.owner()->weak_from_this().lock();
                    if(!inner_owner) { return gc_object_status::invalid_reference; }
                }
            }
            else if(source.kind != kind::wasm_i31) { return gc_object_status::invalid_reference; }
            {
                bridge_guard guard{};
                auto const* existing{find_bridge_inner_locked(source)};
                if(existing != nullptr)
                {
                    auto const retention{retain_foreign_bridge_locked(existing)};
                    if(retention != gc_object_status::ok) { return retention; }
                    result.storage.ptr = existing->token;
                    result.kind = kind::wasm_extern;
                    return gc_object_status::ok;
                }
            }
            if(!locally_published) { return gc_object_status::invalid_reference; }
            // The candidate owns a foreign store lease. Keep its RAII owner
            // outside the guard scope so every failed/racing publication drops
            // that lease only after bridge_lock_ has been released.
            ::std::unique_ptr<extern_bridge> fresh{new(::std::nothrow) extern_bridge{}};
            if(!fresh) { return gc_object_status::out_of_memory; }
            fresh->owner = this;
            fresh->inner = source;
            fresh->inner_owner = ::std::move(inner_owner);
            {
                bridge_guard guard{};
                auto const* existing{find_bridge_inner_locked(source)};
                if(existing != nullptr)
                {
                    auto const retention{retain_foreign_bridge_locked(existing)};
                    if(retention != gc_object_status::ok)
                    { return retention; }
                    result.storage.ptr = existing->token;
                    result.kind = kind::wasm_extern;
                    return gc_object_status::ok;
                }
                if(next_bridge_id_ > token_payload_mask)
                { return gc_object_status::size_overflow; }
                fresh->token = reinterpret_cast<void*>(token_prefix | next_bridge_id_++);
                auto const token_index{bridge_token_bucket(fresh->token)};
                auto const inner_index{bridge_inner_bucket(source)};
                // [fresh is fully initialized][old token bucket head]
                // ^^ publish under bridge_lock_; no guest pointer is dereferenced.
                fresh->token_next = bridge_token_buckets_[token_index];
                bridge_token_buckets_[token_index] = fresh.get();
                // [fresh is registered in token index][old inner bucket head]
                // ^^ second index becomes visible before releasing the same lock.
                fresh->inner_next = bridge_inner_buckets_[inner_index];
                bridge_inner_buckets_[inner_index] = fresh.get();
                result.storage.ptr = fresh->token;
                result.kind = kind::wasm_extern;
                // [both live indexes own the fully initialized bridge] the
                // successful publication transfers this native allocation once.
                // ^^ release the RAII pointer only after the result is initialized.
                static_cast<void>(fresh.release());
            }
            return gc_object_status::ok;
        
#else

            using kind = ::uwvm2::object::global::wasm_ref_kind;
            if(!valid_) { return gc_object_status::invalid_store; }
            if(source.kind == kind::wasm_null) { result = source; return gc_object_status::ok; }
            if(source.kind == kind::wasm_extern)
            {
                if(source.storage.ptr == nullptr) { return gc_object_status::invalid_reference; }
                if(bridge_token_shape(source.storage.ptr))
                {
                    bridge_guard guard{};
                    auto const* bridge{find_bridge_token_locked(source.storage.ptr)};
                    if(bridge == nullptr) { return gc_object_status::invalid_reference; }
                    auto const retention{retain_foreign_bridge_locked(bridge)};
                    if(retention != gc_object_status::ok) { return retention; }
                }
                result = source; // Opaque host external values preserve pointer identity.
                return gc_object_status::ok;
            }
            bool locally_published{source.kind == kind::wasm_i31};
            ::std::shared_ptr<gc_object_store const> inner_owner{};
            if(source.kind == kind::wasm_struct || source.kind == kind::wasm_array)
            {
                auto const* object{checked_object(source, source.kind == kind::wasm_struct ?
                    gc_type::composite_kind::struct_ : gc_type::composite_kind::array)};
                locally_published = object != nullptr;
                if(object != nullptr && object->owner != this)
                {
                    inner_owner = object->owner->weak_from_this().lock();
                    if(!inner_owner) { return gc_object_status::invalid_reference; }
                }
            }
            else if(source.kind != kind::wasm_i31) { return gc_object_status::invalid_reference; }
            {
                bridge_guard guard{};
                auto const* existing{find_bridge_inner_locked(source)};
                if(existing != nullptr)
                {
                    auto const retention{retain_foreign_bridge_locked(existing)};
                    if(retention != gc_object_status::ok) { return retention; }
                    result.storage.ptr = existing->token;
                    result.kind = kind::wasm_extern;
                    return gc_object_status::ok;
                }
            }
            if(!locally_published) { return gc_object_status::invalid_reference; }
            // The candidate owns a foreign store lease. Keep its RAII owner
            // outside the guard scope so every failed/racing publication drops
            // that lease only after bridge_lock_ has been released.
            ::std::unique_ptr<extern_bridge> fresh{new(::std::nothrow) extern_bridge{}};
            if(!fresh) { return gc_object_status::out_of_memory; }
            fresh->owner = this;
            fresh->inner = source;
            fresh->inner_owner = ::std::move(inner_owner);
            {
                bridge_guard guard{};
                auto const* existing{find_bridge_inner_locked(source)};
                if(existing != nullptr)
                {
                    auto const retention{retain_foreign_bridge_locked(existing)};
                    if(retention != gc_object_status::ok)
                    { return retention; }
                    result.storage.ptr = existing->token;
                    result.kind = kind::wasm_extern;
                    return gc_object_status::ok;
                }
                if(next_bridge_id_ > token_payload_mask)
                { return gc_object_status::size_overflow; }
                fresh->token = reinterpret_cast<void*>(token_prefix | next_bridge_id_++);
                auto const token_index{bridge_token_bucket(fresh->token)};
                auto const inner_index{bridge_inner_bucket(source)};
                // [fresh is fully initialized][old token bucket head]
                // ^^ publish under bridge_lock_; no guest pointer is dereferenced.
                fresh->token_next = bridge_token_buckets_[token_index];
                bridge_token_buckets_[token_index] = fresh.get();
                // [fresh is registered in token index][old inner bucket head]
                // ^^ second index becomes visible before releasing the same lock.
                fresh->inner_next = bridge_inner_buckets_[inner_index];
                bridge_inner_buckets_[inner_index] = fresh.get();
                result.storage.ptr = fresh->token;
                result.kind = kind::wasm_extern;
                // [both live indexes own the fully initialized bridge] the
                // successful publication transfers this native allocation once.
                // ^^ release the RAII pointer only after the result is initialized.
                static_cast<void>(fresh.release());
            }
            return gc_object_status::ok;
        
#endif
        }
        [[nodiscard]] inline bool type_kind(::std::uint_least32_t type_index,
                                            gc_type::composite_kind& result) const noexcept
        {
            if(!valid_ || type_index >= layout_count_) { return false; }
            // [0, layout_count_) index selects a live immutable type layout.
            result = layouts_[type_index].kind;
            return true;
        }
        struct numeric_slab_statistics
        {
            ::std::uint64_t epoch{};
            ::std::size_t chunks{};
            ::std::size_t reserved_slots{};
            ::std::size_t allocated_slots{};
            ::std::size_t free_slots{};
            ::std::size_t backing_bytes{};
            bool closing{};
            bool tls_owner_cache{};
        };
        [[nodiscard]] inline numeric_slab_statistics native_numeric_slab_statistics() const noexcept
        {
            slab_guard guard{*this};
            return {slab_epoch_, slab_chunk_count_, slab_reserved_slots_, slab_allocated_slots_,
                slab_chunk_count_ * numeric_slab_slots - slab_reserved_slots_ - slab_allocated_slots_,
                slab_backing_bytes_, slab_closing_, false};
        }
#if defined(UWVM2TEST_GC_NUMERIC_SLAB_RESERVED_PROBE)
        // Fault/overflow qualification only. No production or guest API can
        // reset generations; the test must own an empty, closed native store.
        [[nodiscard]] inline bool native_test_set_slab_generation_exclusive(::std::uint64_t generation) noexcept
        {
            slab_guard guard{*this};
            if(generation == 0u || slab_closing_ || slab_chunk_count_ != 0uz ||
               slab_reserved_slots_ != 0uz || slab_allocated_slots_ != 0uz) { return false; }
            next_slab_generation_ = generation;
            return true;
        }
#endif
        struct numeric_slab_object_location
        {
            ::std::uintptr_t header{};
            ::std::uintptr_t values{};
            ::std::uintptr_t chunk{};
            ::std::uint64_t generation{};
            bool pooled{};
        };
        struct numeric_slab_storage_location
        {
            ::std::uintptr_t envelope{};
            ::std::uintptr_t original_byte_array{};
            ::std::uintptr_t metadata{};
            ::std::size_t backing_bytes{};
            ::std::size_t slot_offset{};
            ::std::size_t value_offset{};
            ::std::size_t metadata_offset{};
            ::std::size_t slot_index{};
            bool pooled{};
        };
        // Native component diagnostics only; the caller must exclude collection.
        // Prove exact local membership first. Never promote a foreign owner,
        // lease a store, or expose any native address to a Wasm instruction.
        [[nodiscard]] inline bool native_numeric_slab_location(gc_reference reference,
            numeric_slab_object_location& result) const noexcept
        {
            auto* header{checked_local_object(reference, gc_type::composite_kind::struct_)};
            if(header == nullptr) { return false; }
            auto* envelope{allocation_header_for(header)};
            auto& metadata{envelope->metadata};
            slab_guard guard{*this};
            if(metadata_state(metadata) != slab_slot_state::allocated) { return false; }
            auto* chunk{active_metadata_chunk(metadata)};
            if(chunk != nullptr && (chunk->owner != this ||
               metadata_generation(metadata) != chunk->generation)) { return false; }
            result = {reinterpret_cast<::std::uintptr_t>(header),
                reinterpret_cast<::std::uintptr_t>(header->values.get()),
                reinterpret_cast<::std::uintptr_t>(chunk), metadata_generation(metadata), chunk != nullptr};
            return true;
        }

        // Proof-only native diagnostics under the same no-collection contract.
        // Return addresses as numbers for the fixture, never guest references.
        [[nodiscard]] inline bool native_numeric_slab_storage_location(gc_reference reference,
            numeric_slab_storage_location& result) const noexcept
        {
            auto* header{checked_local_object(reference, gc_type::composite_kind::struct_)};
            if(header == nullptr) { return false; }
            // [live local header]<->[live union]<->[live envelope]
            // ^^ locate through containment while the member is still alive.
            auto* envelope{allocation_header_for(header)};
            auto const& metadata{envelope->metadata};
            slab_guard guard{*this};
            if(metadata_state(metadata) != slab_slot_state::allocated) { return false; }
            auto* chunk{active_metadata_chunk(metadata)};
            if(chunk != nullptr && (chunk->owner != this ||
               metadata_generation(metadata) != chunk->generation)) { return false; }
            auto const index{chunk == nullptr ? 0uz : metadata_slot_index(metadata)};
            auto* backing{chunk == nullptr ? static_cast<::std::byte*>(metadata.owner_or_storage) : chunk->backing};
            auto const offset{chunk == nullptr ? 0uz : chunk_slots_offset + index * chunk->stride};
            auto const bytes{chunk == nullptr ? object_value_offset + header->length * sizeof(gc_object_value) : chunk->bytes};
            result = {reinterpret_cast<::std::uintptr_t>(envelope), reinterpret_cast<::std::uintptr_t>(backing),
                reinterpret_cast<::std::uintptr_t>(::std::addressof(metadata)), bytes, offset,
                object_value_offset, offsetof(allocation_header, metadata), index, chunk != nullptr};
            return true;
        }

#if defined(UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE) && UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE == 1
        [[nodiscard]] inline ::std::size_t managed_wide_numeric_array_pressure_length(
            ::std::uint_least32_t type_index, ::std::size_t length) const noexcept
        {
            // A native scheduling hint from this store's immutable schema.
            // References/narrow arrays keep the original count policy; earlier
            // tracing increases their scan cost without a cache footprint win.
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
            if(length < 256uz) { return 0uz; }
            auto const* layout{checked_type(type_index, gc_type::composite_kind::array)};
            return numeric_array_width(layout) >= 8uz ? length : 0uz;
#else
            return 0uz;
#endif
        }
#endif

        [[nodiscard]] inline bool field_count(::std::uint_least32_t type_index,
                                               ::std::size_t& result) const noexcept
        {
            if(!valid_ || type_index >= layout_count_) { return false; }
            // [0, layout_count_) index selects a live immutable type layout.
            result = layouts_[type_index].field_count;
            return true;
        }
        [[nodiscard]] inline gc_type::field_type const* field_at(::std::uint_least32_t type_index,
                                                                  ::std::size_t field_index) const noexcept
        {
            if(!valid_ || type_index >= layout_count_) { return nullptr; }
            // [0, layout_count_) index selects a live immutable type layout.
            auto const& layout{layouts_[type_index]};
            if(field_index >= layout.field_count) { return nullptr; }
            // [0, field_count) field pointer stays valid for this store's lifetime.
            return layout.fields.get() + field_index;
        }

        // The generated aggregate ABI supplies count COMPLETE native 16-byte
        // carriers. Its emitter performed endian conversion and rooted every
        // reference before the allocation poll. This is never a compact stack
        // representation or an arbitrary byte prefix.
        [[nodiscard]] inline bool generated_struct_input_has_complete_carriers(
            ::std::uint_least32_t type_index, ::std::size_t count) const noexcept
        {
            auto const* layout{checked_type(type_index, gc_type::composite_kind::struct_)};
            return layout != nullptr && layout->field_count == count;
        }

    private:
        [[nodiscard]] static inline bool field_is_reference(gc_type::storage_type storage) noexcept
        { return storage.packed == gc_type::packed_kind::none && storage.value.kind == gc_type::value_kind::reference; }

        template<typename Visitor>
        [[nodiscard]] static inline bool visit_constructor_reference_fields(
            type_layout const& layout, Visitor&& visitor) noexcept
        {
#if defined(UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA == 1
# if defined(UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA == 1
            return layout.trace.visit_struct_reference_fields(layout.field_count, visitor);
# else
            if(!layout.trace.matches_shape(gc_type::composite_kind::struct_, layout.field_count)) { return false; }
            auto const references{layout.trace.struct_reference_count()};
            for(::std::size_t position{}; position != references; ++position)
            {
                ::std::size_t field{};
                if(!layout.trace.reference_field_at(position, layout.field_count, field) || !visitor(field)) { return false; }
            }
            return true;
# endif
#else
            // Exact-one opt-in: no owned trace metadata is assumed when the
            // product disables it. Every original dynamic check stays intact.
            for(::std::size_t field{}; field != layout.field_count; ++field)
            {
                if(field_is_reference(layout.fields[field].storage) && !visitor(field)) { return false; }
            }
            return true;
#endif
        }

        [[nodiscard]] inline gc_object_status finish_copied_struct(object* fresh,
            type_layout const& layout, gc_reference& result) noexcept
        {
            // The immutable trace plan selects actual reference fields, never
            // payload addresses. Authentication and independent foreign/exn
            // leases remain the ORIGINAL operation for every selected value.
            if(layout.has_reference_fields)
            {
                auto lease_status{gc_object_status::ok};
                auto const visited{visit_constructor_reference_fields(layout,
                    [&](::std::size_t field) noexcept
                    {
                        lease_status = retain_embedded_reference(*fresh, fresh->values[field], layout.fields[field].storage);
                        return lease_status == gc_object_status::ok;
                    })};
                if(!visited)
                {
                    destroy_object(fresh);
                    return lease_status == gc_object_status::ok ? gc_object_status::invalid_type : lease_status;
                }
            }
            if(layout.has_packed_fields)
            {
                for(::std::size_t field{}; field != layout.field_count; ++field)
                {
                    auto const packed{layout.fields[field].storage.packed};
                    if(packed != gc_type::packed_kind::none)
                    { fresh->values[field] = pack(fresh->values[field], packed); }
                }
            }
            return publish(fresh, result);
        }

    public:
        [[nodiscard]] inline gc_object_status struct_new_from_generated_carrier_bytes(
            ::std::uint_least32_t type_index, ::std::byte const* bytes, ::std::size_t count,
            gc_reference& result) noexcept
        {
            auto const* layout{checked_type(type_index, gc_type::composite_kind::struct_)};
            if(layout == nullptr) { return gc_object_status::invalid_type; }
            if(count != layout->field_count || (count != 0uz && bytes == nullptr))
            { return gc_object_status::invalid_value; }
            if(count > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(gc_object_value))
            { return gc_object_status::size_overflow; }
            // Reject every invalid dynamic reference BEFORE allocation, just
            // like the typed constructor. Numeric/packed representations accept
            // all bits. A source byte buffer is never reinterpreted as T[].
            if(layout->has_reference_fields)
            {
                auto const valid_references{visit_constructor_reference_fields(*layout,
                    [&](::std::size_t field) noexcept
                    {
                        gc_object_value value{};
                        ::std::memcpy(::std::addressof(value), bytes + field * sizeof(gc_object_value), sizeof(value));
                        return value_matches(value, layout->fields[field].storage);
                    })};
                if(!valid_references) { return gc_object_status::invalid_value; }
            }
            object* fresh{};
            auto const status{allocate(type_index, gc_type::composite_kind::struct_, count, fresh, bytes)};
            if(status != gc_object_status::ok) { return status; }
            return finish_copied_struct(fresh, *layout, result);
        }

    private:
        [[nodiscard]] inline gc_object_status struct_new_small(::std::uint_least32_t type_index,
            gc_object_value const* inputs, ::std::size_t count, gc_reference& result) noexcept
        {
#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1

            auto const* layout{checked_type(type_index, gc_type::composite_kind::struct_)};
            if(layout == nullptr) { return gc_object_status::invalid_type; }
            if(count != layout->field_count || (count != 0uz && inputs == nullptr))
            { return gc_object_status::invalid_value; }
            if(layout->has_reference_fields)
            {
                for(::std::size_t i{}; i != count; ++i)
                {
                    // Every reference keeps its original dynamic validation.
                    if(!value_matches(inputs[i], layout->fields[i].storage)) { return gc_object_status::invalid_value; }
                }
            }
            if(compact_layout(layout) && compact_numeric_payload::has_raw_four_byte_layout && !weak_from_this().expired())
            {
                // [inputs,inputs+1) the complete carrier extent was checked.
                // Copy its low raw32 bits without allocating a legacy header.
                return allocate_compact_numeric(type_index, inputs[0uz].as<::std::uint32_t>(), result);
            }
            object* fresh{};
            auto const status{allocate(type_index, gc_type::composite_kind::struct_, count, fresh)};
            if(status != gc_object_status::ok) { return status; }
            if(!layout->has_reference_fields && !layout->has_packed_fields)
            {
                // The immutable schema proves complete numeric carriers:
                // pack(none) is identity and no lease can be created. Both
                // arrays have count live, distinct 16-byte elements; allocate
                // checked the multiplication. Fresh storage is unpublished.
                // memcpy preserves SNaN/v128 bits and uses the native library.
                if(count != 0uz)
                { ::std::memcpy(fresh->values.get(), inputs, count * sizeof(gc_object_value)); }
                return publish(fresh, result);
            }
            for(::std::size_t i{}; i != count; ++i)
            {
                // [0, count) fresh owns count live slots and the validated field metadata.
                auto const lease{retain_embedded_reference(*fresh, inputs[i], layout->fields[i].storage)};
                if(lease != gc_object_status::ok) { destroy_object(fresh); return lease; }
                fresh->values[i] = pack(inputs[i], layout->fields[i].storage.packed);
            }
            return publish(fresh, result);
        
#else

            auto const* layout{checked_type(type_index, gc_type::composite_kind::struct_)};
            if(layout == nullptr) { return gc_object_status::invalid_type; }
            if(count != layout->field_count || (count != 0uz && inputs == nullptr))
            { return gc_object_status::invalid_value; }
            if(layout->has_reference_fields)
            {
                for(::std::size_t i{}; i != count; ++i)
                {
                    // Every reference keeps its original dynamic validation.
                    if(!value_matches(inputs[i], layout->fields[i].storage)) { return gc_object_status::invalid_value; }
                }
            }
            object* fresh{};
            auto const status{allocate(type_index, gc_type::composite_kind::struct_, count, fresh)};
            if(status != gc_object_status::ok) { return status; }
            if(!layout->has_reference_fields && !layout->has_packed_fields)
            {
                // The immutable schema proves complete numeric carriers:
                // pack(none) is identity and no lease can be created. Both
                // arrays have count live, distinct 16-byte elements; allocate
                // checked the multiplication. Fresh storage is unpublished.
                // memcpy preserves SNaN/v128 bits and uses the native library.
                if(count != 0uz)
                { ::std::memcpy(fresh->values.get(), inputs, count * sizeof(gc_object_value)); }
                return publish(fresh, result);
            }
            for(::std::size_t i{}; i != count; ++i)
            {
                // [0, count) fresh owns count live slots and the validated field metadata.
                auto const lease{retain_embedded_reference(*fresh, inputs[i], layout->fields[i].storage)};
                if(lease != gc_object_status::ok) { destroy_object(fresh); return lease; }
                fresh->values[i] = pack(inputs[i], layout->fields[i].storage.packed);
            }
            return publish(fresh, result);
        
#endif
        }
    public:
        [[nodiscard]] inline gc_object_status struct_new(::std::uint_least32_t type_index,
            gc_object_value const* inputs, ::std::size_t count, gc_reference& result) noexcept
        {
            // Preserve the measured small-aggregate path, including reference leases.
            if(count <= 8uz) { return struct_new_small(type_index, inputs, count, result); }
            auto const* layout{checked_type(type_index, gc_type::composite_kind::struct_)};
            if(layout == nullptr) { return gc_object_status::invalid_type; }
            if(count != layout->field_count || (count != 0uz && inputs == nullptr))
            { return gc_object_status::invalid_value; }
            if(layout->has_reference_fields)
            {
                auto const valid_references{visit_constructor_reference_fields(*layout,
                    [&](::std::size_t field) noexcept
                    { return value_matches(inputs[field], layout->fields[field].storage); })};
                if(!valid_references) { return gc_object_status::invalid_value; }
            }
#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1
            if(compact_layout(layout) && compact_numeric_payload::has_raw_four_byte_layout && !weak_from_this().expired())
            { return allocate_compact_numeric(type_index, inputs[0uz].as<::std::uint32_t>(), result); }
#endif
            object* fresh{};
            auto const* bytes{reinterpret_cast<::std::byte const*>(inputs)};
            auto const status{allocate(type_index, gc_type::composite_kind::struct_, count, fresh, bytes)};
            if(status != gc_object_status::ok) { return status; }
            return finish_copied_struct(fresh, *layout, result);
        }
        // Interpreter path after its register ring has been flushed. The caller proves
        // [stack_begin, stack_begin + stack_bytes) belongs to the operand stack;
        // this method checks every offset against stack_bytes before reading it.
        [[nodiscard]] inline gc_object_status struct_new_from_stack(::std::uint_least32_t type_index,
            ::std::byte const* stack_begin, ::std::size_t stack_bytes, gc_reference& result) noexcept
        {
#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1

            auto const* layout{checked_type(type_index, gc_type::composite_kind::struct_)};
            if(layout == nullptr) { return gc_object_status::invalid_type; }
            if(stack_bytes != 0uz && stack_begin == nullptr) { return gc_object_status::invalid_value; }
            ::std::size_t expected{};
            for(::std::size_t i{}; i != layout->field_count; ++i)
            {
                auto const width{gc_object_value::wasm_value_size(
                    layout->fields[i].storage.packed == gc_type::packed_kind::none ?
                    layout->fields[i].storage.value.kind : gc_type::value_kind::i32)};
                if(width == 0uz || width > stack_bytes - expected) { return gc_object_status::invalid_value; }
                expected += width;
            }
            if(expected != stack_bytes) { return gc_object_status::invalid_value; }
            if(compact_layout(layout) && compact_numeric_payload::has_raw_four_byte_layout && !weak_from_this().expired())
            {
                // [stack_begin,stack_begin+4) expected==stack_bytes above and
                // the single unpacked numeric field prove the complete extent.
                ::std::uint32_t bits{};
                ::std::memcpy(::std::addressof(bits), stack_begin, sizeof(bits));
                return allocate_compact_numeric(type_index, bits, result);
            }
            object* fresh{};
            auto const status{allocate(type_index, gc_type::composite_kind::struct_, layout->field_count, fresh)};
            if(status != gc_object_status::ok) { return status; }
            ::std::size_t offset{};
            for(::std::size_t i{}; i != layout->field_count; ++i)
            {
                auto const storage{layout->fields[i].storage};
                auto const width{gc_object_value::wasm_value_size(
                    storage.packed == gc_type::packed_kind::none ? storage.value.kind : gc_type::value_kind::i32)};
                gc_object_value value{};
                // [stack_begin, stack_begin + stack_bytes) contains this complete width;
                // expected==stack_bytes and the preceding bounded sum prove offset+width.
                ::std::memcpy(value.bits.data(), stack_begin + offset, width);
                if(!value_matches(value, storage)) { destroy_object(fresh); return gc_object_status::invalid_value; }
                auto const lease{retain_embedded_reference(*fresh, value, storage)};
                if(lease != gc_object_status::ok) { destroy_object(fresh); return lease; }
                // [0, field_count) fresh owns the corresponding initialized slot.
                fresh->values[i] = pack(value, storage.packed);
                offset += width;
                // [stack_begin, stack_begin + stack_bytes] offset may now be one-past.
            }
            return publish(fresh, result);
        
#else

            auto const* layout{checked_type(type_index, gc_type::composite_kind::struct_)};
            if(layout == nullptr) { return gc_object_status::invalid_type; }
            if(stack_bytes != 0uz && stack_begin == nullptr) { return gc_object_status::invalid_value; }
            ::std::size_t expected{};
            for(::std::size_t i{}; i != layout->field_count; ++i)
            {
                auto const width{gc_object_value::wasm_value_size(
                    layout->fields[i].storage.packed == gc_type::packed_kind::none ?
                    layout->fields[i].storage.value.kind : gc_type::value_kind::i32)};
                if(width == 0uz || width > stack_bytes - expected) { return gc_object_status::invalid_value; }
                expected += width;
            }
            if(expected != stack_bytes) { return gc_object_status::invalid_value; }
            object* fresh{};
            auto const status{allocate(type_index, gc_type::composite_kind::struct_, layout->field_count, fresh)};
            if(status != gc_object_status::ok) { return status; }
            ::std::size_t offset{};
            for(::std::size_t i{}; i != layout->field_count; ++i)
            {
                auto const storage{layout->fields[i].storage};
                auto const width{gc_object_value::wasm_value_size(
                    storage.packed == gc_type::packed_kind::none ? storage.value.kind : gc_type::value_kind::i32)};
                gc_object_value value{};
                // [stack_begin, stack_begin + stack_bytes) contains this complete width;
                // expected==stack_bytes and the preceding bounded sum prove offset+width.
                ::std::memcpy(value.bits.data(), stack_begin + offset, width);
                if(!value_matches(value, storage)) { destroy_object(fresh); return gc_object_status::invalid_value; }
                auto const lease{retain_embedded_reference(*fresh, value, storage)};
                if(lease != gc_object_status::ok) { destroy_object(fresh); return lease; }
                // [0, field_count) fresh owns the corresponding initialized slot.
                fresh->values[i] = pack(value, storage.packed);
                offset += width;
                // [stack_begin, stack_begin + stack_bytes] offset may now be one-past.
            }
            return publish(fresh, result);
        
#endif
        }
        [[nodiscard]] inline gc_object_status struct_new_default(::std::uint_least32_t type_index,
                                                                   gc_reference& result) noexcept
        {
#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1

            auto const* layout{checked_type(type_index, gc_type::composite_kind::struct_)};
            if(layout == nullptr) { return gc_object_status::invalid_type; }
            if(layout->has_nonnullable_reference_fields) { return gc_object_status::invalid_value; }
            if(compact_layout(layout) && compact_numeric_payload::has_raw_four_byte_layout && !weak_from_this().expired())
            { return allocate_compact_numeric(type_index, 0u, result); }
            object* fresh{};
            auto const status{allocate(type_index, gc_type::composite_kind::struct_, layout->field_count, fresh)};
            if(status != gc_object_status::ok) { return status; }
            // allocate started and value-initialized every real carrier. Numeric
            // and packed defaults already have their exact zero byte representation.
            // Only actual reference fields need a native null-reference factory;
            // do not assume that a pointer's representation is all-zero bytes.
            if(layout->has_reference_fields && !visit_constructor_reference_fields(*layout,
                [&](::std::size_t field) noexcept
                { fresh->values[field] = default_value(layout->fields[field].storage); return true; }))
            { destroy_object(fresh); return gc_object_status::invalid_type; }
            return publish(fresh, result);
        
#else

            auto const* layout{checked_type(type_index, gc_type::composite_kind::struct_)};
            if(layout == nullptr) { return gc_object_status::invalid_type; }
            if(layout->has_nonnullable_reference_fields) { return gc_object_status::invalid_value; }
            object* fresh{};
            auto const status{allocate(type_index, gc_type::composite_kind::struct_, layout->field_count, fresh)};
            if(status != gc_object_status::ok) { return status; }
            // allocate started and value-initialized every real carrier. Numeric
            // and packed defaults already have their exact zero byte representation.
            // Only actual reference fields need a native null-reference factory;
            // do not assume that a pointer's representation is all-zero bytes.
            if(layout->has_reference_fields && !visit_constructor_reference_fields(*layout,
                [&](::std::size_t field) noexcept
                { fresh->values[field] = default_value(layout->fields[field].storage); return true; }))
            { destroy_object(fresh); return gc_object_status::invalid_type; }
            return publish(fresh, result);
        
#endif
        }
        [[nodiscard]] inline gc_object_status array_new(::std::uint_least32_t type_index,
            gc_object_value value, ::std::size_t length, gc_reference& result) noexcept
        {
            auto const* layout{checked_type(type_index, gc_type::composite_kind::array)};
            if(layout == nullptr) { return gc_object_status::invalid_type; }
            if(!value_matches(value, layout->fields[0].storage)) { return gc_object_status::invalid_value; }
            object* fresh{};
            auto const status{allocate(type_index, gc_type::composite_kind::array, length, fresh)};
            if(status != gc_object_status::ok) { return status; }
            if(length != 0uz)
            {
                auto const lease{retain_embedded_reference(*fresh, value, layout->fields[0].storage)};
                if(lease != gc_object_status::ok) { destroy_object(fresh); return lease; }
            }
            auto const packed{pack(value, layout->fields[0].storage.packed)};
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
            fill_array_values(*fresh,0uz,length,packed,layout->fields[0uz].storage);
#else
            for(::std::size_t i{};i != length;++i) { fresh->values[i]=packed; }
#endif
            return publish(fresh, result);
        }
        [[nodiscard]] inline gc_object_status array_new_default(::std::uint_least32_t type_index,
            ::std::size_t length, gc_reference& result) noexcept
        {
            auto const* layout{checked_type(type_index, gc_type::composite_kind::array)};
            if(layout == nullptr) { return gc_object_status::invalid_type; }
            auto const storage{layout->fields[0].storage};
            if(storage.packed == gc_type::packed_kind::none &&
               storage.value.kind == gc_type::value_kind::reference && !storage.value.nullable)
            { return gc_object_status::invalid_value; }
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
            // Keep the established byte-array path at the small checked entry.
            // It already uses one libc memset; an outlined general constructor
            // added stalls without reducing its payload or publication work.
            if(storage.packed == gc_type::packed_kind::i8)
            { return array_new(type_index, default_value(storage), length, result); }
#endif
            return initialize_default_array_payload(type_index, length, storage, result);
        }
        [[nodiscard]] inline gc_object_status array_new_fixed(::std::uint_least32_t type_index,
            gc_object_value const* inputs, ::std::size_t length, gc_reference& result) noexcept
        {
            auto const* layout{checked_type(type_index, gc_type::composite_kind::array)};
            if(layout == nullptr) { return gc_object_status::invalid_type; }
            if(length != 0uz && inputs == nullptr) { return gc_object_status::invalid_value; }
            for(::std::size_t i{}; i != length; ++i)
            {
                // [0, length) caller supplied exactly length initialized inputs.
                if(!value_matches(inputs[i], layout->fields[0].storage)) { return gc_object_status::invalid_value; }
            }
            object* fresh{};
            auto const status{allocate(type_index, gc_type::composite_kind::array, length, fresh)};
            if(status != gc_object_status::ok) { return status; }
            for(::std::size_t i{}; i != length; ++i)
            {
                // [0, length) fresh owns length live slots.
                auto const lease{retain_embedded_reference(*fresh, inputs[i], layout->fields[0].storage)};
                if(lease != gc_object_status::ok) { destroy_object(fresh); return lease; }
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
                store_array_value(*fresh, i, pack(inputs[i], layout->fields[0uz].storage.packed),
                    layout->fields[0uz].storage);
#else
                fresh->values[i] = pack(inputs[i], layout->fields[0].storage.packed);
#endif
            }
            return publish(fresh, result);
        }
        [[nodiscard]] inline gc_object_status array_new_fixed_from_stack(::std::uint_least32_t type_index,
            ::std::byte const* stack_begin, ::std::size_t stack_bytes, ::std::size_t length,
            gc_reference& result) noexcept
        {
            auto const* layout{checked_type(type_index, gc_type::composite_kind::array)};
            if(layout == nullptr) { return gc_object_status::invalid_type; }
            auto const storage{layout->fields[0].storage};
            auto const width{gc_object_value::wasm_value_size(
                storage.packed == gc_type::packed_kind::none ? storage.value.kind : gc_type::value_kind::i32)};
            if(width == 0uz || length > stack_bytes / width || length * width != stack_bytes ||
               (stack_bytes != 0uz && stack_begin == nullptr))
            { return gc_object_status::invalid_value; }
            object* fresh{};
            auto const status{allocate(type_index, gc_type::composite_kind::array, length, fresh)};
            if(status != gc_object_status::ok) { return status; }
            for(::std::size_t i{}; i != length; ++i)
            {
                // i < length and length*width==stack_bytes prove this complete source slot.
                gc_object_value value{};
                ::std::memcpy(value.bits.data(), stack_begin + i * width, width);
                if(!value_matches(value, storage)) { destroy_object(fresh); return gc_object_status::invalid_value; }
                auto const lease{retain_embedded_reference(*fresh, value, storage)};
                if(lease != gc_object_status::ok) { destroy_object(fresh); return lease; }
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
                store_array_value(*fresh, i, pack(value, storage.packed), storage);
#else
                fresh->values[i] = pack(value, storage.packed);
#endif
            }
            return publish(fresh, result);
        }
        // The source bytes are a retained immutable data-segment snapshot. Source offsets
        // are byte offsets; array lengths and destination offsets count elements.
        [[nodiscard]] inline gc_object_status array_new_data(::std::uint_least32_t type_index,
            ::std::byte const* source, ::std::size_t source_size, ::std::size_t source_offset,
            ::std::size_t length, gc_reference& result) noexcept
        {
            auto const* layout{checked_type(type_index, gc_type::composite_kind::array)};
            if(layout == nullptr) { return gc_object_status::invalid_type; }
            auto const storage{layout->fields[0].storage};
            if(storage.value.kind == gc_type::value_kind::reference) { return gc_object_status::invalid_type; }
            auto const width{gc_object_value::field_storage_size(storage)};
            if(width == 0uz || width > sizeof(gc_object_value) ||
               length > (::std::numeric_limits<::std::size_t>::max)() / width)
            { return gc_object_status::size_overflow; }
            auto const bytes{length * width};
            if(source_offset > source_size || bytes > source_size - source_offset)
            { return gc_object_status::out_of_bounds; }
            if(bytes != 0uz && source == nullptr) { return gc_object_status::invalid_value; }
            object* fresh{};
            auto const status{allocate(type_index, gc_type::composite_kind::array, length, fresh)};
            if(status != gc_object_status::ok) { return status; }
            for(::std::size_t i{}; i != length; ++i)
            {
                // [source + source_offset, + bytes) is complete: i*width < bytes.
                // ^^ the element pointer advances by one proven in-range width.
                auto const value{data_element_value(source + source_offset + i * width, storage)};
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
                store_array_value(*fresh, i, pack(value, storage.packed), storage);
#else
                fresh->values[i] = pack(value, storage.packed);
#endif
            }
            return publish(fresh, result);
        }
        [[nodiscard]] inline gc_object_status array_init_data(::std::uint_least32_t type_index,
            gc_reference reference, ::std::size_t destination_offset, ::std::byte const* source,
            ::std::size_t source_size, ::std::size_t source_offset, ::std::size_t length) noexcept
        {
            auto* obj{checked_object(reference, gc_type::composite_kind::array)};
            if(obj == nullptr) { return reference_error(reference); }
            auto const* declared{checked_type(type_index, gc_type::composite_kind::array)};
            auto const* actual{obj->owner->checked_type(obj->type_index, gc_type::composite_kind::array)};
            if(declared == nullptr || actual == nullptr ||
               !canonical_subtype(obj->owner, obj->type_index, this, type_index))
            { return gc_object_status::invalid_type; }
            auto const storage{actual->fields[0].storage};
            if(storage.value.kind == gc_type::value_kind::reference || !actual->fields[0].mutable_)
            { return gc_object_status::invalid_type; }
            auto const width{gc_object_value::field_storage_size(storage)};
            if(width == 0uz || width > sizeof(gc_object_value) ||
               length > (::std::numeric_limits<::std::size_t>::max)() / width)
            { return gc_object_status::size_overflow; }
            auto const bytes{length * width};
            if(destination_offset > obj->length || length > obj->length - destination_offset ||
               source_offset > source_size || bytes > source_size - source_offset)
            { return gc_object_status::out_of_bounds; }
            if(bytes != 0uz && source == nullptr) { return gc_object_status::invalid_value; }
            object_lock lock{*obj};
            for(::std::size_t i{}; i != length; ++i)
            {
                // Both destination element and source byte ranges were checked by subtraction.
                // ^^ source + source_offset + i*width stays inside the immutable segment.
                auto const value{data_element_value(source + source_offset + i * width, storage)};
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
                store_array_value(*obj, destination_offset + i, pack(value, storage.packed), storage);
#else
                obj->values[destination_offset + i] = pack(value, storage.packed);
#endif
            }
            return gc_object_status::ok;
        }
        [[nodiscard]] inline gc_object_status array_new_elements(::std::uint_least32_t type_index,
            gc_reference const* source, ::std::size_t length, gc_reference& result) noexcept
        {
            auto const* layout{checked_type(type_index, gc_type::composite_kind::array)};
            if(layout == nullptr || layout->fields[0].storage.value.kind != gc_type::value_kind::reference)
            { return gc_object_status::invalid_type; }
            if(length != 0uz && source == nullptr) { return gc_object_status::invalid_value; }
            auto const storage{layout->fields[0].storage};
            for(::std::size_t i{}; i != length; ++i)
            {
                // [0, length) source references are a fully materialized segment window.
                if(!reference_matches(source[i], storage.value)) { return gc_object_status::invalid_value; }
            }
            object* fresh{};
            auto const status{allocate(type_index, gc_type::composite_kind::array, length, fresh)};
            if(status != gc_object_status::ok) { return status; }
            for(::std::size_t i{}; i != length; ++i)
            {
                // [0, length) fresh and source each own one complete reference carrier.
                auto const lease{retain_embedded_reference(*fresh,
                    gc_object_value::reference(source[i]), storage)};
                if(lease != gc_object_status::ok) { destroy_object(fresh); return lease; }
                fresh->values[i] = gc_object_value::reference(source[i]);
            }
            return publish(fresh, result);
        }
        [[nodiscard]] inline gc_object_status array_init_elements(::std::uint_least32_t type_index,
            gc_reference reference, ::std::size_t destination_offset,
            gc_reference const* source, ::std::size_t length) noexcept
        {
            auto* obj{checked_object(reference, gc_type::composite_kind::array)};
            if(obj == nullptr) { return reference_error(reference); }
            auto const* declared{checked_type(type_index, gc_type::composite_kind::array)};
            auto const* actual{obj->owner->checked_type(obj->type_index, gc_type::composite_kind::array)};
            if(declared == nullptr || actual == nullptr ||
               !canonical_subtype(obj->owner, obj->type_index, this, type_index))
            { return gc_object_status::invalid_type; }
            auto const storage{actual->fields[0].storage};
            if(storage.value.kind != gc_type::value_kind::reference || !actual->fields[0].mutable_)
            { return gc_object_status::invalid_type; }
            if(destination_offset > obj->length || length > obj->length - destination_offset)
            { return gc_object_status::out_of_bounds; }
            if(length != 0uz && source == nullptr) { return gc_object_status::invalid_value; }
            for(::std::size_t i{}; i != length; ++i)
            {
                // [0, length) source was materialized from a checked segment window.
                if(!reference_matches(source[i], storage.value, obj->owner)) { return gc_object_status::invalid_value; }
                auto const lease{retain_embedded_reference(*obj,
                    gc_object_value::reference(source[i]), storage)};
                if(lease != gc_object_status::ok) { return lease; }
            }
            object_lock lock{*obj};
            for(::std::size_t i{}; i != length; ++i)
            {
                // [destination_offset, +length) fits the immutable object length.
                obj->values[destination_offset + i] = gc_object_value::reference(source[i]);
            }
            return gc_object_status::ok;
        }
        [[nodiscard]] inline gc_object_status struct_get(gc_reference reference, ::std::size_t field_index,
            bool sign_extend, gc_object_value& result) const noexcept
        {
#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1

            if(reference.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_struct ||
               reference.storage.ptr == nullptr) { return reference_error(reference); }
            auto const view{checked_aggregate_object(reference, gc_type::composite_kind::struct_)};
            if(!view) { return reference_error(reference); }
            return view.compact != nullptr ? compact_get(view, field_index, result) :
                struct_get_object(view.legacy, field_index, sign_extend, result);
        
#else

            if((reference.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_struct) |
               (reference.storage.ptr == nullptr))
            { return reference_error(reference); }
            auto* obj{checked_local_object(reference, gc_type::composite_kind::struct_)};
            if(obj == nullptr) [[unlikely]]
            { return struct_get_foreign(reference, field_index, sign_extend, result); }
            return struct_get_object(obj, field_index, sign_extend, result);
        
#endif
        }
        template<bool SignExtend>
#if defined(__GNUC__) || defined(__clang__)
        [[gnu::always_inline]]
#endif
        [[nodiscard]] inline ::std::uint64_t struct_get32(gc_reference reference,
            ::std::size_t field_index) const noexcept
        {
#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1

            if(reference.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_struct ||
               reference.storage.ptr == nullptr)
            { return static_cast<::std::uint64_t>(reference_error(reference)) << 32u; }
            auto const view{checked_aggregate_object(reference, gc_type::composite_kind::struct_)};
            if(!view) { return static_cast<::std::uint64_t>(reference_error(reference)) << 32u; }
            if(view.legacy != nullptr) { return struct_get32_object<SignExtend>(view.legacy, field_index); }
            if(field_index != 0uz) { return static_cast<::std::uint64_t>(gc_object_status::out_of_bounds) << 32u; }
            ::std::uint32_t bits{};
            auto const status{compact_status(view.compact->payload_.load(view.compact_slot, bits))};
            // [one immutable unpacked raw32 field] sign extension is irrelevant
            // for this shape. Preserve every i32/f32 raw bit and the status ABI.
            return static_cast<::std::uint64_t>(bits) | (static_cast<::std::uint64_t>(status) << 32u);
        
#else

            if((reference.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_struct) |
               (reference.storage.ptr == nullptr))
            { return static_cast<::std::uint64_t>(reference_error(reference)) << 32u; }
            // [published local bucket chain] the full token is compared as a key.
            // [safe                         ] forged/stale keys never become
            // ^^ object pointers; only checked_local_object can yield this node.
            auto* obj{checked_local_object(reference, gc_type::composite_kind::struct_)};
            if(obj == nullptr) [[unlikely]]
            { return struct_get32_foreign<SignExtend>(reference, field_index); }
            return struct_get32_object<SignExtend>(obj, field_index);
        
#endif
        }
        [[nodiscard]] inline gc_object_status struct_set(gc_reference reference, ::std::size_t field_index,
            gc_object_value value) noexcept
        {
#if defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) && UWVM_EXPERIMENTAL_COMPACT_NUMERIC == 1

            if(reference.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_struct ||
               reference.storage.ptr == nullptr) { return reference_error(reference); }
            auto const view{checked_aggregate_object(reference, gc_type::composite_kind::struct_)};
            if(!view) { return reference_error(reference); }
            if(view.compact != nullptr)
            { return field_index == 0uz ? gc_object_status::immutable_field : gc_object_status::out_of_bounds; }
            return struct_set_object(view.legacy, field_index, value);
        
#else

            if((reference.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_struct) |
               (reference.storage.ptr == nullptr))
            { return reference_error(reference); }
            auto* obj{checked_local_object(reference, gc_type::composite_kind::struct_)};
            if(obj == nullptr) [[unlikely]]
            { return struct_set_foreign(reference, field_index, value); }
            auto const* layout{checked_type(obj->type_index, gc_type::composite_kind::struct_)};
            if(layout == nullptr || field_index >= layout->field_count || field_index >= obj->length)
            { return gc_object_status::out_of_bounds; }
            // [0, field_count) field_index selects a live field and object slot.
            auto const& field{layout->fields[field_index]};
            if(!field.mutable_) { return gc_object_status::immutable_field; }
            if(field.storage.value.kind == gc_type::value_kind::reference)
            {
#if defined(UWVM_EXPERIMENTAL_STRUCT_SET_LOCAL_REF_AUTH) && UWVM_EXPERIMENTAL_STRUCT_SET_LOCAL_REF_AUTH == 1
                // The destination is an authenticated member of this pinned
                // store. Reuse only this operation's ordinary same-owner source
                // membership/type check; the bool carries no pointer or lease.
                bool local_authenticated{};
                if(!value_matches_with_local_auth<true>(
                    value, field.storage, this, ::std::addressof(local_authenticated)))
                { return gc_object_status::invalid_value; }
                if(!local_authenticated)
                {
                    // Compact, foreign, exception, extern and function values
                    // keep the complete original retention/status path.
                    auto const lease{retain_embedded_reference(*obj, value, field.storage)};
                    if(lease != gc_object_status::ok) { return lease; }
                }
                // The original object lock remains below. No poll, callback,
                // collection or owner release can occur after local proof.
#else
                if(!value_matches(value, field.storage, this))
                { return gc_object_status::invalid_value; }
                auto const lease{retain_embedded_reference(*obj, value, field.storage)};
                if(lease != gc_object_status::ok) { return lease; }
#endif
            }
            object_lock lock{*obj};
            obj->values[field_index] = pack(value, field.storage.packed);
            return gc_object_status::ok;
        
#endif
        }
        [[nodiscard]] inline gc_object_status array_length(gc_reference reference, ::std::size_t& result) const noexcept
        {
            auto* obj{checked_object(reference, gc_type::composite_kind::array)};
            if(obj == nullptr) { return reference_error(reference); }
            result = obj->length;
            return gc_object_status::ok;
        }
#if defined(UWVM2TEST_GC_PACKED_NUMERIC_ARRAY_PROBE)
        // Native component proof only. This reports the actual requested
        // backing extent, not malloc overhead, RSS, a lease or a guest address.
        // The fixture owns the complete live store and excludes collection.
        [[nodiscard]] inline bool native_test_array_requested_extent(gc_reference reference,
            ::std::size_t& element_bytes, ::std::size_t& backing_bytes, bool& packed_numeric) const noexcept
        {
            auto const* header{checked_local_object(reference, gc_type::composite_kind::array)};
            if(header == nullptr) { return false; }
            auto width{sizeof(gc_object_value)};
            bool raw{};
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
            if(header->numeric_array_)
            {
                width = numeric_array_width(checked_type(header->type_index, header->kind));
                if(width == 0uz) { return false; }
                raw = true;
            }
#endif
            if(header->length > (static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) -
                object_value_offset) / width) { return false; }
            element_bytes = width;
            backing_bytes = object_value_offset + header->length * width;
            packed_numeric = raw;
            return true;
        }
#endif
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
# include "numeric_array_get32.h"
#endif
        [[nodiscard]] inline gc_object_status array_get(gc_reference reference, ::std::size_t index,
            bool sign_extend, gc_object_value& result) const noexcept
        {
            auto* obj{checked_object(reference, gc_type::composite_kind::array)};
            if(obj == nullptr) { return reference_error(reference); }
            if(index >= obj->length) { return gc_object_status::out_of_bounds; }
            auto const* layout{obj->owner->checked_type(obj->type_index, gc_type::composite_kind::array)};
            if(layout == nullptr) { return gc_object_status::invalid_type; }
            if(layout->fields[0].mutable_)
            {
                object_lock lock{*obj};
                // [0, length) index was checked and is stable for the object's lifetime.
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
                result = unpack(load_array_value(*obj, index, layout->fields[0uz].storage),
                    layout->fields[0uz].storage.packed, sign_extend);
#else
                result = unpack(obj->values[index], layout->fields[0].storage.packed, sign_extend);
#endif
            }
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
            else { result = unpack(load_array_value(*obj, index, layout->fields[0uz].storage),
                layout->fields[0uz].storage.packed, sign_extend); }
#else
            else { result = unpack(obj->values[index], layout->fields[0].storage.packed, sign_extend); }
#endif
            return gc_object_status::ok;
        }
#if defined(UWVM_EXPERIMENTAL_NUMERIC_ARRAY_SET32) && UWVM_EXPERIMENTAL_NUMERIC_ARRAY_SET32 == 1
# include "numeric_array_set32.h"
#endif
        [[nodiscard]] inline gc_object_status array_set(gc_reference reference, ::std::size_t index,
            gc_object_value value) noexcept
        {
            auto* obj{checked_object(reference, gc_type::composite_kind::array)};
            if(obj == nullptr) { return reference_error(reference); }
            if(index >= obj->length) { return gc_object_status::out_of_bounds; }
            auto const* layout{obj->owner->checked_type(obj->type_index, gc_type::composite_kind::array)};
            if(layout == nullptr) { return gc_object_status::invalid_type; }
            auto const& field{layout->fields[0]};
            if(!field.mutable_) { return gc_object_status::immutable_field; }
#if defined(UWVM_EXPERIMENTAL_ARRAY_SET_LOCAL_REF_AUTH) && UWVM_EXPERIMENTAL_ARRAY_SET_LOCAL_REF_AUTH == 1
            // [actual checked destination][one private same-operation result]
            // [safe ] only THIS store's reference array may request local proof.
            // ^^ initialize a bool output slot, never a guest carrier/root.
            bool local_authenticated{};
            bool const local_destination{obj->owner == this &&
                field.storage.packed == gc_type::packed_kind::none &&
                field.storage.value.kind == gc_type::value_kind::reference};
            bool matched{};
            if(local_destination)
            {
                using ref_kind = ::uwvm2::object::global::wasm_ref_kind;
                using heap = gc_type::abstract_heap_type;
                auto const source{value.as<gc_reference>()};
                object const* ordinary{};
                if(source.kind == ref_kind::wasm_struct || source.kind == ref_kind::wasm_array)
                {
                    // Authenticate the opaque token before reading any source metadata.
                    // The acquire membership lookup and pinned store exclude stale objects.
                    // Keep authentication in this hot callsite after separating scalar/reference CFGs.
                    // A statement hint changes no verification semantics or architecture-specific code.
#if __has_cpp_attribute(clang::always_inline)
                    [[clang::always_inline]]
#endif
                    ordinary = checked_local_object(source, source.kind == ref_kind::wasm_struct ?
                        gc_type::composite_kind::struct_ : gc_type::composite_kind::array);
                }
                if(ordinary != nullptr && ordinary->owner == this)
                {
                    auto const& expected{field.storage.value};
                    if(expected.heap.is_defined())
                    {
                        if(expected.heap.code <= static_cast<::std::int_least64_t>((::std::numeric_limits<::std::uint_least32_t>::max)()))
                        {
                            auto const expected_index{static_cast<::std::uint_least32_t>(expected.heap.code)};
                            // Immutable same-store indices prove the most common exact match.
                            // Distinct equivalent declarations and subtypes retain canonical matching.
                            matched = (ordinary->type_index < layout_count_ && ordinary->type_index == expected_index) ||
                                canonical_subtype(this, ordinary->type_index, this, expected_index);
                        }
                    }
                    else
                    {
                        auto const code{expected.heap.code};
                        matched = code == static_cast<::std::int_least64_t>(heap::any) ||
                            code == static_cast<::std::int_least64_t>(heap::eq) ||
                            code == static_cast<::std::int_least64_t>(source.kind == ref_kind::wasm_struct ? heap::struct_ : heap::array);
                    }
                    local_authenticated = matched;
                }
                else
                {
                    matched = value_matches_with_local_auth<true>(value, field.storage, obj->owner,
                        ::std::addressof(local_authenticated));
                }
            }
            else
            {
                // Keep scalar/packed and foreign destinations on the original verifier path.
                // Do not decode numeric carrier bytes as a reference or repeat reference tests.
                matched = value_matches(value, field.storage, obj->owner);
            }
            if(!matched) { return gc_object_status::invalid_value; }
            // Null, compact, foreign, function, extern and exception values
            // keep the complete ORIGINAL retention query/lease/status order.
            // A proved ordinary local source needs no independent owner lease.
            // The original retainer returns immediately for scalar/packed storage.
            // Decide that immutable storage predicate here; preserve all reference lease/status work.
            if(!local_authenticated &&
               field.storage.packed == gc_type::packed_kind::none &&
               field.storage.value.kind == gc_type::value_kind::reference)
            {
                auto const lease{retain_embedded_reference(*obj, value, field.storage)};
                if(lease != gc_object_status::ok) { return lease; }
            }
            // No allocator/poll/callback/owner release may enter this interval.
            // The original object_lock below still excludes concurrent writers.
#else
            if(!value_matches(value, field.storage, obj->owner)) { return gc_object_status::invalid_value; }
            auto const lease{retain_embedded_reference(*obj, value, field.storage)};
            if(lease != gc_object_status::ok) { return lease; }
#endif
            object_lock lock{*obj};
            // [0, length) index was checked and the array never changes length.
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
            // Keep the payload write in this checked callsite when the generated
            // raw-carrier bridge is inlined; preserve packing and native memcpy.
#if defined(UWVM_EXPERIMENTAL_GC_ARRAY_SET_RAW_CARRIER_ABI) && UWVM_EXPERIMENTAL_GC_ARRAY_SET_RAW_CARRIER_ABI == 1 && !defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
#if __has_cpp_attribute(clang::always_inline)
            [[clang::always_inline]]
#endif
#endif
            store_array_value(*obj, index, pack(value, field.storage.packed), field.storage);
#else
            obj->values[index] = pack(value, field.storage.packed);
#endif
            return gc_object_status::ok;
        }
        [[nodiscard]] inline gc_object_status array_fill(gc_reference reference, ::std::size_t offset,
            gc_object_value value, ::std::size_t count) noexcept
        {
            auto* obj{checked_object(reference, gc_type::composite_kind::array)};
            if(obj == nullptr) { return reference_error(reference); }
            if(offset > obj->length || count > obj->length - offset) { return gc_object_status::out_of_bounds; }
            auto const* layout{obj->owner->checked_type(obj->type_index, gc_type::composite_kind::array)};
            if(layout == nullptr) { return gc_object_status::invalid_type; }
            auto const& field{layout->fields[0]};
            if(!field.mutable_) { return gc_object_status::immutable_field; }
            if(!value_matches(value, field.storage, obj->owner)) { return gc_object_status::invalid_value; }
            if(count != 0uz)
            {
                auto const lease{retain_embedded_reference(*obj, value, field.storage)};
                if(lease != gc_object_status::ok) { return lease; }
            }
            auto const packed{pack(value, field.storage.packed)};
            object_lock lock{*obj};
            for(::std::size_t i{}; i != count; ++i)
            {
                // [offset, offset + count) checked by subtraction, so addition cannot overflow.
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
                store_array_value(*obj, offset + i, packed, field.storage);
#else
                obj->values[offset + i] = packed;
#endif
            }
            return gc_object_status::ok;
        }
        [[nodiscard]] inline gc_object_status array_copy(gc_reference destination, ::std::size_t dst_offset,
            gc_reference source, ::std::size_t src_offset, ::std::size_t count) noexcept
        {
            auto* dst{checked_object(destination, gc_type::composite_kind::array)};
            auto* src{checked_object(source, gc_type::composite_kind::array)};
            if(dst == nullptr) { return reference_error(destination); }
            if(src == nullptr) { return reference_error(source); }
            if(dst_offset > dst->length || count > dst->length - dst_offset ||
               src_offset > src->length || count > src->length - src_offset)
            { return gc_object_status::out_of_bounds; }
            auto const* dst_layout{dst->owner->checked_type(dst->type_index, gc_type::composite_kind::array)};
            auto const* src_layout{src->owner->checked_type(src->type_index, gc_type::composite_kind::array)};
            if(dst_layout == nullptr || src_layout == nullptr) { return gc_object_status::invalid_type; }
            if(!dst_layout->fields[0].mutable_) { return gc_object_status::immutable_field; }
            // Numeric storage must match exactly. Reference storage may be covariant;
            // every source payload is checked against the destination heap/nullability
            // while both arrays are locked, before any destination slot is changed.
            auto const dst_storage{dst_layout->fields[0].storage};
            auto const src_storage{src_layout->fields[0].storage};
            auto const reference_copy{dst_storage.packed == gc_type::packed_kind::none &&
                                      src_storage.packed == gc_type::packed_kind::none &&
                                      dst_storage.value.kind == gc_type::value_kind::reference &&
                                      src_storage.value.kind == gc_type::value_kind::reference};
            if(!reference_copy && dst_storage != src_storage)
            { return gc_object_status::invalid_value; }
            if(count == 0uz) { return gc_object_status::ok; }
            auto copy = [&]() noexcept -> gc_object_status
            {
                if(reference_copy)
                {
                    for(::std::size_t i{}; i != count; ++i)
                    {
                        // [src_offset, src_offset + count) is checked against src->length.
                        // ^^ each payload stays live while src's mutation lock is held.
                        if(!reference_matches(
                            src->values[src_offset + i].as<gc_reference>(), dst_storage.value, dst->owner))
                        { return gc_object_status::invalid_value; }
                        auto const lease{retain_embedded_reference(*dst,
                            src->values[src_offset + i], dst_storage)};
                        if(lease != gc_object_status::ok) { return lease; }
                    }
                }
                // Both ranges are bounded. memmove preserves overlapping self-copy semantics.
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
                copy_array_values(*dst, dst_offset, *src, src_offset, count, dst_storage);
#else
                ::std::memmove(dst->values.get() + dst_offset, src->values.get() + src_offset,
                               count * sizeof(gc_object_value));
#endif
                return gc_object_status::ok;
            };
            if(dst == src) { object_lock lock{*dst}; return copy(); }
            else
            {
                // std::less would impose the same stable total order, but integer addresses
                // avoid evaluating an unrelated-pointer relational comparison.
                if(reinterpret_cast<::std::uintptr_t>(dst) < reinterpret_cast<::std::uintptr_t>(src))
                { object_lock first{*dst}; object_lock second{*src}; return copy(); }
                else { object_lock first{*src}; object_lock second{*dst}; return copy(); }
            }
        }
    };

    // Unmangled, pointer-only ABI for LLVM-generated calls. Keep these wrappers address-taken
    // by the runtime symbol registrar; JIT code never owns or deletes an object.
    extern "C" [[nodiscard]] inline gc_object_status uwvm2_gc_struct_new(gc_object_store* store,
        ::std::uint_least32_t type_index, gc_object_value const* inputs, ::std::size_t count,
        gc_reference* result) noexcept
    {
        if(store == nullptr) { return gc_object_status::invalid_store; }
        if(result == nullptr) { return gc_object_status::invalid_value; }
        return store->struct_new(type_index, inputs, count, *result);
    }
    extern "C" [[nodiscard]] inline bool uwvm2_gc_reference_type_matches(gc_object_store const* store,
        gc_reference const* reference, ::std::int_least64_t heap_code, bool nullable) noexcept
    {
        if(store == nullptr || reference == nullptr) { return false; }
        return store->reference_type_matches(*reference,
            {gc_type::value_kind::reference, {heap_code}, nullable});
    }
    extern "C" [[nodiscard]] inline gc_object_status uwvm2_gc_retain_reference(gc_object_store const* store,
        gc_reference const* reference) noexcept
    {
        if(store == nullptr || reference == nullptr) { return gc_object_status::invalid_store; }
        return store->retain_gc_reference(*reference);
    }
    extern "C" [[nodiscard]] inline gc_object_status uwvm2_gc_any_convert_extern(
        gc_object_store const* store, gc_reference const* source, gc_reference* result) noexcept
    {
        if(store == nullptr) { return gc_object_status::invalid_store; }
        if(source == nullptr || result == nullptr) { return gc_object_status::invalid_value; }
        return store->any_convert_extern(*source, *result);
    }
    extern "C" [[nodiscard]] inline gc_object_status uwvm2_gc_extern_convert_any(
        gc_object_store const* store, gc_reference const* source, gc_reference* result) noexcept
    {
        if(store == nullptr) { return gc_object_status::invalid_store; }
        if(source == nullptr || result == nullptr) { return gc_object_status::invalid_value; }
        return store->extern_convert_any(*source, *result);
    }
    extern "C" [[nodiscard]] inline gc_object_status uwvm2_gc_struct_new_default(gc_object_store* store,
        ::std::uint_least32_t type_index, gc_reference* result) noexcept
    {
        if(store == nullptr) { return gc_object_status::invalid_store; }
        if(result == nullptr) { return gc_object_status::invalid_value; }
        return store->struct_new_default(type_index, *result);
    }
    extern "C" [[nodiscard]] inline gc_object_status uwvm2_gc_array_new(gc_object_store* store,
        ::std::uint_least32_t type_index, gc_object_value const* value, ::std::size_t length,
        gc_reference* result) noexcept
    {
        if(store == nullptr) { return gc_object_status::invalid_store; }
        if(value == nullptr || result == nullptr) { return gc_object_status::invalid_value; }
        return store->array_new(type_index, *value, length, *result);
    }
    extern "C" [[nodiscard]] inline gc_object_status uwvm2_gc_array_new_default(gc_object_store* store,
        ::std::uint_least32_t type_index, ::std::size_t length, gc_reference* result) noexcept
    {
        if(store == nullptr) { return gc_object_status::invalid_store; }
        if(result == nullptr) { return gc_object_status::invalid_value; }
        return store->array_new_default(type_index, length, *result);
    }
    extern "C" [[nodiscard]] inline gc_object_status uwvm2_gc_array_new_fixed(gc_object_store* store,
        ::std::uint_least32_t type_index, gc_object_value const* inputs, ::std::size_t length,
        gc_reference* result) noexcept
    {
        if(store == nullptr) { return gc_object_status::invalid_store; }
        if(result == nullptr) { return gc_object_status::invalid_value; }
        return store->array_new_fixed(type_index, inputs, length, *result);
    }
    extern "C" [[nodiscard]] inline gc_object_status uwvm2_gc_struct_get(gc_object_store* store,
        gc_reference reference, ::std::size_t field, bool sign_extend, gc_object_value* result) noexcept
    {
        if(store == nullptr) { return gc_object_status::invalid_store; }
        if(result == nullptr) { return gc_object_status::invalid_value; }
        return store->struct_get(reference, field, sign_extend, *result);
    }
    extern "C" [[nodiscard]] inline gc_object_status uwvm2_gc_struct_set(gc_object_store* store,
        gc_reference reference, ::std::size_t field, gc_object_value const* value) noexcept
    {
        if(store == nullptr) { return gc_object_status::invalid_store; }
        if(value == nullptr) { return gc_object_status::invalid_value; }
        return store->struct_set(reference, field, *value);
    }
    extern "C" [[nodiscard]] inline gc_object_status uwvm2_gc_array_get(gc_object_store* store,
        gc_reference reference, ::std::size_t index, bool sign_extend, gc_object_value* result) noexcept
    {
        if(store == nullptr) { return gc_object_status::invalid_store; }
        if(result == nullptr) { return gc_object_status::invalid_value; }
        return store->array_get(reference, index, sign_extend, *result);
    }
    extern "C" [[nodiscard]] inline gc_object_status uwvm2_gc_array_set(gc_object_store* store,
        gc_reference reference, ::std::size_t index, gc_object_value const* value) noexcept
    {
        if(store == nullptr) { return gc_object_status::invalid_store; }
        if(value == nullptr) { return gc_object_status::invalid_value; }
        return store->array_set(reference, index, *value);
    }
    extern "C" [[nodiscard]] inline gc_object_status uwvm2_gc_array_length(gc_object_store* store,
        gc_reference reference, ::std::size_t* result) noexcept
    {
        if(store == nullptr) { return gc_object_status::invalid_store; }
        if(result == nullptr) { return gc_object_status::invalid_value; }
        return store->array_length(reference, *result);
    }
    extern "C" [[nodiscard]] inline gc_object_status uwvm2_gc_array_fill(gc_object_store* store,
        gc_reference reference, ::std::size_t offset, gc_object_value const* value,
        ::std::size_t count) noexcept
    {
        if(store == nullptr) { return gc_object_status::invalid_store; }
        if(value == nullptr) { return gc_object_status::invalid_value; }
        return store->array_fill(reference, offset, *value, count);
    }
    extern "C" [[nodiscard]] inline gc_object_status uwvm2_gc_array_copy(gc_object_store* store,
        gc_reference destination, ::std::size_t dst_offset, gc_reference source,
        ::std::size_t src_offset, ::std::size_t count) noexcept
    {
        if(store == nullptr) { return gc_object_status::invalid_store; }
        return store->array_copy(destination, dst_offset, source, src_offset, count);
    }
#include "gc_staged_publication.h"
}
