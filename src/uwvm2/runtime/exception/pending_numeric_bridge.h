// PRIVATE native / LLVM ABI experiment. No production route imports this file.
// The prepared registry, native chain, context and execution island are REAL
// stack-scoped native objects. Generated guest code passes this same context
// explicitly as its hidden first argument via an OWNED R2 header; no pending-state TLS.
#pragma once
#ifndef UWVM_MODULE
#include <uwvm2/runtime/exception/pending_island.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <type_traits>
#include <utility>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::exception::pending_experiment::numeric_guest_bridge
{
    using word = ::std::uintptr_t;
    using unary_abi = word (*)(word) noexcept;
    using binary_abi = word (*)(word, word) noexcept;
    using ternary_abi = word (*)(word, word, word) noexcept;
    using quaternary_abi = word (*)(word, word, word, word) noexcept;
    static_assert(::std::numeric_limits<word>::digits == ::std::numeric_limits<::std::size_t>::digits);
    inline constexpr word empty{0u}, pending{1u}, presence_error_bias{2u};

    [[nodiscard]] inline constexpr word result(status value) noexcept { return static_cast<word>(value); }
    [[nodiscard]] inline constexpr word presence_error(status value) noexcept
    { return presence_error_bias + result(value); }

    // Semantic names are explicit and different even when two helpers have an
    // identical FunctionType. Never register these using PRETTY_FUNCTION shape
    // alone: publish and copy both have four uintptr arguments.
    inline constexpr char8_t has_pending_semantic[] = u8"uwvm2_pending_numeric_has_pending_r2";
    inline constexpr char8_t publish_semantic[] = u8"uwvm2_pending_numeric_publish_r2";
    inline constexpr char8_t matches_semantic[] = u8"uwvm2_pending_numeric_matches_r2";
    inline constexpr char8_t copy_semantic[] = u8"uwvm2_pending_numeric_copy_r2";
    inline constexpr char8_t clear_semantic[] = u8"uwvm2_pending_numeric_clear_r2";
    inline constexpr char8_t append_exit_semantic[] = u8"uwvm2_pending_numeric_append_exit_r2";


    namespace native_details { struct header_access; }

    // R2-only native hidden argument. This REAL owned object contains no
    // shared/weak owner and is standard-layout. NEVER derive an offset into
    // pending_context: that slow object contains shared_ptr/arrays/island state.
    // Only sealed same-generation numeric cores may read phase directly, on
    // the native owner thread between successful construct and unique destroy.
    // This is not guest memory, an arbitrary host pointer authenticator, or a
    // concurrent debugger/GC query. Native callers must retain the live owner.
    class numeric_header final
    {
        word phase_{presence_error_bias};
        pending_context* context_{};
        friend struct native_details::header_access;
    public:
        // The real native island must already be active on this thread. Initial
        // numeric-empty admission is checked cold; existing/ref/inactive/busy
        // state never activates a header or enters generated code.
        explicit numeric_header(pending_context& context) noexcept
            : context_{::std::addressof(context)}
        {
            numeric_leaf_state borrowed{};
            if(context.borrow_numeric_state(borrowed) == status::no_pending) { phase_ = empty; }
        }
        numeric_header(numeric_header const&) = delete;
        numeric_header& operator=(numeric_header const&) = delete;
        numeric_header(numeric_header&&) = delete;
        numeric_header& operator=(numeric_header&&) = delete;
        ~numeric_header() noexcept
        {
            // [live member objects][header retires BEFORE slow context/island]
            // [safe                                                       ]
            // No hidden argument may be borrowed across native owner cleanup.
            // These writes do not extend the lifetime of a retired header.
            phase_ = presence_error_bias;
            context_ = nullptr;
        }
        [[nodiscard]] static constexpr ::std::size_t phase_offset() noexcept
        { return offsetof(numeric_header, phase_); }
        [[nodiscard]] bool activated_on_owner() const noexcept
        { return context_ != nullptr && phase_ < presence_error_bias; }
        [[nodiscard]] bool pending_on_owner() const noexcept { return phase_ == pending; }

        // COLD owner-thread boundary only. TraceBuilder may genuinely allocate/
        // throw, but may NOT mutate/reenter this numeric island or retain slow
        // spans. Keep the original context/caught registration; publish empty
        // only AFTER successful materialization; a failure/throw leaves phase
        // and pending intact for the wrapper's real C++ cleanup edge.
        template<class TraceBuilder>
        [[nodiscard]] status materialize_in_registered(caught_payload& target, TraceBuilder&& builder)
        {
            if(context_ == nullptr) { return status::invalid_registry; }
            numeric_leaf_state borrowed{};
            auto const admitted{context_->borrow_numeric_state(borrowed)};
            if(admitted != status::ok) { return admitted; }
            if(phase_ != pending) { return status::invalid_registry; }
            auto const materialized{context_->materialize_in_registered(target,
                ::std::forward<TraceBuilder>(builder))};
            if(materialized == status::ok) { phase_ = empty; }
            return materialized;
        }
    };
    static_assert(::std::is_standard_layout_v<numeric_header>);
    static_assert(numeric_header::phase_offset() == 0uz);
    static_assert(alignof(numeric_header) >= alignof(word));
    static_assert(::std::is_nothrow_constructible_v<numeric_header, pending_context&>);
    static_assert(::std::is_nothrow_destructible_v<numeric_header>);

    namespace native_details
    {
        struct header_access
        {
            [[nodiscard]] static pending_context* slow_context(numeric_header const& header) noexcept
            { return header.context_; }
            [[nodiscard]] static word phase_on_owner(numeric_header const& header) noexcept
            { return header.phase_; }
            // Only already-validated nonthrowing numeric leaf success reaches
            // these stores. No callback/collection/native reentry lies between
            // the slow operation's commit and the matching header publication.
            static void publish_pending(numeric_header& header) noexcept { header.phase_ = pending; }
            static void publish_empty(numeric_header& header) noexcept { header.phase_ = empty; }
        };

        // TRUSTED R2 native ABI admission, never a Wasm linear-memory pointer.
        // The caller supplies the real live header returned by owner_construct;
        // alignment does NOT authenticate arbitrary mapped/forged host words.
        [[nodiscard]] inline numeric_header* header(word address) noexcept
        {
            if(address == 0u || address % alignof(numeric_header) != 0u) { return nullptr; }
            // [placement-created entry owner's real numeric_header][its extent]
            // [safe                                                         ]
            // Reverse only the constructor's native pointer-to-integer cast.
            return reinterpret_cast<numeric_header*>(address);
        }

        [[nodiscard]] inline status borrow(word address, numeric_header*& borrowed_header,
            pending_context*& slow, numeric_leaf_state& state) noexcept
        {
            // [constructor-returned live native header][complete extent]
            // [safe ] Reverse only a retained owner's native word; this ABI
            // never authenticates a guest/forged/stale pointer.
            borrowed_header = header(address);
            // [output borrow reset][no referent read or lifetime extension]
            // [safe ] The failure result cannot retain an old slow borrow.
            slow = nullptr;
            state = {};
            if(borrowed_header == nullptr) { return status::invalid_registry; }
            // [live header's immutable slow pointer][live owner context]
            // [safe ] Member initialization precedes publication; the owner's
            // unique destroy may not race this borrowed native leaf.
            slow = header_access::slow_context(*borrowed_header);
            if(slow == nullptr) { return status::invalid_registry; }
            // Slow admission retains leaf_access: inactive, wrong-thread and
            // nested-child-busy fail BEFORE phase or field reads. Existing/ref
            // owners, registry signatures, widths and empty roots keep their
            // original full checks. No shared_ptr is copied/promoted here.
            auto const admitted{slow->borrow_numeric_state_for_bridge(state)};
            if(admitted != status::ok && admitted != status::no_pending) { return admitted; }
            auto const projected{header_access::phase_on_owner(*borrowed_header)};
            if((admitted == status::ok && projected != pending) ||
               (admitted == status::no_pending && projected != empty))
            { return status::invalid_registry; }
            return admitted;
        }

        [[nodiscard]] inline status tuple_size(::std::span<payload_kind const> signature,
            ::std::size_t& bytes) noexcept
        {
            bytes = 0uz;
            if(signature.size() > max_payload_fields) { return status::size_overflow; }
            for(auto kind: signature)
            {
                if(kind != payload_kind::i32 && kind != payload_kind::i64 && kind != payload_kind::f32 &&
                   kind != payload_kind::f64 && kind != payload_kind::v128)
                { return status::host_codec_required; }
                auto const width{payload_width(kind)};
                if(width == 0uz) { return status::invalid_signature; }
                if(width > (::std::numeric_limits<::std::size_t>::max)() - bytes) { return status::size_overflow; }
                bytes += width;
            }
            return status::ok;
        }

        // TRUSTED NATIVE tuple/destination admission. The real generated caller
        // owns a complete object/byte-array extent at this address, alive until
        // this leaf returns. Exact length + arithmetic checks never prove that
        // an arbitrary host integer is mapped; guest addresses are not allowed.
        [[nodiscard]] inline status extent(word address, word bytes,
            word header_address, word context_address) noexcept
        {
            if(bytes == 0u) { return status::ok; }
            if(address == 0u) { return status::invalid_reference; }
            if(bytes > (::std::numeric_limits<word>::max)() - address ||
               sizeof(numeric_header) > (::std::numeric_limits<word>::max)() - header_address ||
               sizeof(pending_context) > (::std::numeric_limits<word>::max)() - context_address)
            { return status::size_overflow; }
            auto const end{address + bytes};
            auto const header_end{header_address + sizeof(numeric_header)};
            auto const context_end{context_address + sizeof(pending_context)};
            // Both genuine objects are excluded BEFORE the first tuple read or
            // destination write. Copy cannot corrupt phase/the slow pointer,
            // and publish cannot consume bytes from its own header or context.
            if((address < header_end && header_address < end) ||
               (address < context_end && context_address < end))
            { return status::invalid_reference; }
            return status::ok;
        }

    }

    extern "C"
    {
        // Presence uses 0 / 1; errors are >= 2, never equivalent to true.
        // Native probes/cold users must check errors separately. Sealed R2 LLVM
        // calls instead read the owned header; they do NOT call this slow query.
        [[nodiscard]] inline word uwvm2_pending_numeric_has_pending_r2(word header_address) noexcept
        {
            numeric_header* header{};
            pending_context* owner{};
            numeric_leaf_state borrowed{};
            auto const admitted{native_details::borrow(header_address, header, owner, borrowed)};
            if(admitted == status::no_pending) { return empty; }
            if(admitted == status::ok) { return pending; }
            return presence_error(admitted);
        }

        // tuple layout: fields packed consecutively, no alignment/padding;
        // i32/f32 4, i64/f64 8, v128 16 bytes in native byte representation.
        // No FP operation is applied to the raw bits (including signaling NaN).
        [[nodiscard]] inline word uwvm2_pending_numeric_publish_r2(word header_address,
            word tag_index, word tuple_address, word tuple_bytes) noexcept
        {
            numeric_header* header{};
            pending_context* owner{};
            numeric_leaf_state borrowed{};
            auto const current{native_details::borrow(header_address, header, owner, borrowed)};
            if(current == status::ok) { return result(status::busy); }
            if(current != status::no_pending) { return result(current); }
            ::std::span<payload_kind const> signature{};
            ::std::size_t required{};
            auto const schema{owner->borrow_numeric_signature(static_cast<::std::size_t>(tag_index), signature, &required)};
            if(schema != status::ok) { return result(schema); }
#if !defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) || UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH != 1
            auto const shape{native_details::tuple_size(signature, required)};
            if(shape != status::ok) { return result(shape); }
#endif
            if(tuple_bytes != required) { return result(status::invalid_signature); }
            auto const bounds{native_details::extent(tuple_address, tuple_bytes, header_address, reinterpret_cast<word>(owner))};
            if(bounds != status::ok) { return result(bounds); }
#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1
            auto const* input{reinterpret_cast<::std::byte const*>(tuple_address)};
            auto const published{owner->publish_numeric_tuple(static_cast<::std::size_t>(tag_index), {input, required})};
#else
            static_assert(::std::is_nothrow_default_constructible_v<payload_field>);
            static_assert(::std::is_nothrow_destructible_v<payload_field>);
            static_assert(max_payload_fields <= static_cast<::std::size_t>(
                (::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(payload_field));
            // [complete aligned native byte array][count <= max_payload_fields]
            // [safe                                                          ]
            // Start ONLY the actual payload prefix. Standard nonallocating
            // placement array-new has zero array-cookie overhead; the original
            // bytes provide the whole proved count * sizeof(payload_field)
            // extent. No 64-slot default construction/destruction is needed
            // for a single-field event. No object exists in the unused tail.
            alignas(payload_field) ::std::byte decoded_storage[sizeof(payload_field) * max_payload_fields];
            auto* decoded{::new(static_cast<void*>(decoded_storage)) payload_field[signature.size()]{} };
            struct decoded_prefix_owner
            {
                payload_field* fields;
                ::std::size_t count;
                ~decoded_prefix_owner() noexcept
                {
                    for(::std::size_t index{}; index != count; ++index)
                    {
                        // [live placement-created field array][index < count]
                        // [safe                                               ]
                        // End each started lifetime on every leaf return; no
                        // field in the unused byte tail is read or destroyed.
                        ::std::destroy_at(fields + index);
                    }
                }
            } decoded_owner{decoded, signature.size()};
            // [trusted owned native tuple][required bytes checked][end]
            // [safe                                                    ]
            // Empty schema never dereferences or advances a null pointer.
            auto const* input{reinterpret_cast<::std::byte const*>(tuple_address)};
            for(::std::size_t index{}; index != signature.size(); ++index)
            {
                auto const width{payload_width(signature[index])};
                // [fully preflighted tuple][current width-byte field][end]
                // [safe                                                ]
                // The summed widths equal the caller's COMPLETE native extent.
                auto field{payload_field::numeric(signature[index], {input, width})};
                if(!field) { return result(status::invalid_signature); }
                decoded[index] = ::std::move(*field);
                // [complete current field][next field or one-past tuple end]
                // [safe                  ] advance remains in this owned byte
                // array; index/count and all cumulative widths were preflighted.
                input += width;
            }
            // All fields have empty owners and numeric kinds; this validator is
            // never called. The original publication still checks every field
            // before moving any slot. No C++ throw, heap allocation, collection,
            // native unwind/backtrace, callback or guest reentry is performed.
            auto reject_reference = [](::std::size_t, ::std::size_t, reference) noexcept { return false; };
            auto const published{owner->publish_fresh(static_cast<::std::size_t>(tag_index),
                {decoded, signature.size()}, reject_reference)};
#endif
            if(published == status::ok) { native_details::header_access::publish_pending(*header); }
            return result(published);
        }

        [[nodiscard]] inline word uwvm2_pending_numeric_matches_r2(word header_address, word tag_index) noexcept
        {
            numeric_header* header{};
            pending_context* owner{};
            numeric_leaf_state borrowed{};
            auto const admitted{native_details::borrow(header_address, header, owner, borrowed)};
            if(admitted != status::ok) { return result(admitted); }
            // Original context compares the genuine admitted tag INSTANCE,
            // not equal signatures, integer tags or host pointer resemblance.
            return result(owner->matches(static_cast<::std::size_t>(tag_index)));
        }

        [[nodiscard]] inline word uwvm2_pending_numeric_copy_r2(word header_address, word tag_index,
            word destination_address, word destination_bytes) noexcept
        {
            numeric_header* header{};
            pending_context* owner{};
            numeric_leaf_state borrowed{};
            auto const admitted{native_details::borrow(header_address, header, owner, borrowed)};
            if(admitted != status::ok) { return result(admitted); }
            auto const matched{owner->matches(static_cast<::std::size_t>(tag_index))};
            if(matched != status::ok) { return result(matched); }
            ::std::size_t required{};
#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1
            required = borrowed.exact_numeric_bytes;
#else
            auto const shape{native_details::tuple_size(borrowed.signature, required)};
            if(shape != status::ok) { return result(shape); }
#endif
            if(destination_bytes != required) { return result(status::invalid_signature); }
            auto const bounds{native_details::extent(destination_address, destination_bytes, header_address, reinterpret_cast<word>(owner))};
            if(bounds != status::ok) { return result(bounds); }
            // [caller-owned COMPLETE native destination][preflighted end]
            // [safe                                                   ]
            // Do not form pointer arithmetic or call memcpy for empty payload.
            auto* output{reinterpret_cast<::std::byte*>(destination_address)};
#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1
            if(borrowed.packed)
            {
                if(required != 0uz) { ::std::memcpy(output,borrowed.packed_tuple.data(),required); }
            }
            else
#endif
            for(auto const& field: borrowed.fields)
            {
                auto const bits{field.bits()};
                // [live constructed context field bytes][owned output field]
                // [safe                                ] kinds, counts, widths,
                // owners and WHOLE output extent were checked before this copy.
                ::std::memcpy(output, bits.data(), bits.size());
                // [current written field][next field or one-past output end]
                // [safe                 ] summed widths equal checked extent.
                output += bits.size();
            }
            // Pending remains intact. LLVM must load the typed SSA values and
            // publish any required precise roots BEFORE calling the clear leaf.
            return result(status::ok);
        }

        [[nodiscard]] inline word uwvm2_pending_numeric_clear_r2(word header_address) noexcept
        {
            numeric_header* header{};
            pending_context* owner{};
            numeric_leaf_state borrowed{};
            auto const admitted{native_details::borrow(header_address, header, owner, borrowed)};
            if(admitted != status::ok && admitted != status::no_pending) { return result(admitted); }
            // Only fresh numeric slots with EMPTY owners reach this leaf;
            // dropping an observable value/foreign owner requires fallback.
            auto const cleared{owner->clear()};
            if(cleared == status::ok) { native_details::header_access::publish_empty(*header); }
            return result(cleared);
        }

        [[nodiscard]] inline word uwvm2_pending_numeric_append_exit_r2(word header_address,
            word module_id, word function_index) noexcept
        {
            numeric_header* header{};
            pending_context* owner{};
            numeric_leaf_state borrowed{};
            auto const admitted{native_details::borrow(header_address, header, owner, borrowed)};
            if(admitted != status::ok) { return result(admitted); }
            // Logical IDs only: no stack addresses or delayed native borrows.
            // Original 64-entry prefix/truncation semantics are retained.
            return result(owner->append_exceptional_exit({static_cast<::std::size_t>(module_id),
                static_cast<::std::size_t>(function_index)}));
        }
    }

    static_assert(::std::is_same_v<decltype(&uwvm2_pending_numeric_has_pending_r2), unary_abi>);
    static_assert(::std::is_same_v<decltype(&uwvm2_pending_numeric_publish_r2), quaternary_abi>);
    static_assert(::std::is_same_v<decltype(&uwvm2_pending_numeric_matches_r2), binary_abi>);
    static_assert(::std::is_same_v<decltype(&uwvm2_pending_numeric_copy_r2), quaternary_abi>);
    static_assert(::std::is_same_v<decltype(&uwvm2_pending_numeric_clear_r2), unary_abi>);
    static_assert(::std::is_same_v<decltype(&uwvm2_pending_numeric_append_exit_r2), ternary_abi>);
}
#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1
# include "pending_numeric_take.h"
#endif
