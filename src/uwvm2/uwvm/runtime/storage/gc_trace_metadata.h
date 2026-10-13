/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <limits>
# include <memory>
# include <new>
# include <utility>
# include <uwvm2/parser/wasm/standard/wasm3/type/recursive_type.h>
#if defined(UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA == 1
# include <bit>
# include <cstdint>
# include <type_traits>
#endif
#endif
#if defined(UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA == 1
# if !defined(UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA) || UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA != 1
#  error "Inline GC trace metadata requires the owned precise trace plan"
# endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::storage
{
    namespace gc_trace_type = ::uwvm2::parser::wasm::standard::wasm3::type;

    enum class gc_trace_metadata_status : unsigned
    {
        ok, invalid_layout, size_overflow, out_of_memory
    };
#if defined(UWVM2TEST_GC_TRACE_METADATA_ALLOCATION_PROBE) && UWVM2TEST_GC_TRACE_METADATA_ALLOCATION_PROBE == 1
    // Correctness-only rendezvous for exact allocation-failure provenance.
    // Absent from all product/performance builds; never an admission token.
    extern "C" void gc_trace_metadata_allocation_probe(::std::size_t) noexcept
        asm("uwvm2_test_gc_trace_metadata_allocation_probe");
#endif

    // Private native trace plan, not a reference decoder or guest authority.
    // The store builds it from its OWN immutable copied layout only AFTER
    // canonical recursive-type validation. A caller supplies complete live
    // field storage; a count check cannot authenticate arbitrary native memory.
    // Published store layouts never rebuild it. No parser pointers survive.
    class gc_trace_metadata
    {
        ::std::unique_ptr<::std::size_t[]> reference_fields_{};
        ::std::size_t field_count_{};
        ::std::size_t reference_count_{};
        gc_trace_type::composite_kind kind_{};
        bool ready_{};
        bool array_reference_{};
#if defined(UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA == 1
        // Experimental native classification only: one bit per actual struct
        // field for layouts of at most 64 fields. It never stores a guest
        // reference or authenticates a token. Larger layouts retain the owned
        // index array, and the stopped collector still authenticates every edge.
        ::std::uint64_t inline_reference_fields_{};
#endif

    public:
        gc_trace_metadata() noexcept = default;
        gc_trace_metadata(gc_trace_metadata const&) = delete;
        gc_trace_metadata& operator=(gc_trace_metadata const&) = delete;

        [[nodiscard]] inline gc_trace_metadata_status build(
            gc_trace_type::composite_kind kind,
            gc_trace_type::field_type const* fields, ::std::size_t field_count) noexcept
        {
            // [old privately owned index array] [no published store readers]
            // ^^ retire its unique ownership before an attempted rebuild. Any
            // failure leaves no usable plan and no borrowed parser pointer.
            reference_fields_.reset();
            ready_ = false;
            array_reference_ = false;
            field_count_ = 0uz;
            reference_count_ = 0uz;
#if defined(UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA == 1
            inline_reference_fields_ = 0u;
#endif
            if(kind > gc_trace_type::composite_kind::array ||
               (kind == gc_trace_type::composite_kind::function && field_count != 0uz) ||
               (kind == gc_trace_type::composite_kind::array && field_count != 1uz) ||
               (field_count != 0uz && fields == nullptr))
            { return gc_trace_metadata_status::invalid_layout; }
            auto constexpr max_bytes{static_cast<::std::size_t>(
                (::std::numeric_limits<::std::ptrdiff_t>::max)())};
            if(field_count > max_bytes / sizeof(gc_trace_type::field_type) ||
               field_count > max_bytes / sizeof(::std::size_t))
            { return gc_trace_metadata_status::size_overflow; }
            ::std::size_t references{};
#if defined(UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA == 1
            ::std::uint64_t inline_fields{};
#endif
            for(::std::size_t index{}; index != field_count; ++index)
            {
                // [fields, fields + field_count) is the complete store-owned
                // immutable field array; index < field_count selects one field.
                // [safe                                      ] no pointer moves.
                auto const storage{fields[index].storage};
                if(storage.packed > gc_trace_type::packed_kind::i16 ||
                   (storage.packed == gc_trace_type::packed_kind::none &&
                    storage.value.kind > gc_trace_type::value_kind::reference))
                { return gc_trace_metadata_status::invalid_layout; }
                references += storage.packed == gc_trace_type::packed_kind::none &&
                              storage.value.kind == gc_trace_type::value_kind::reference;
#if defined(UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA == 1
                if(kind == gc_trace_type::composite_kind::struct_ && field_count <= 64uz &&
                   storage.packed == gc_trace_type::packed_kind::none &&
                   storage.value.kind == gc_trace_type::value_kind::reference)
                {
                    // [0,field_count) and field_count <= 64 prove index < 64.
                    // [safe         ] the unsigned shift cannot reach the word
                    // width; this is a native field index, never a guest value.
                    inline_fields |= ::std::uint64_t{1u} << index;
                }
#endif
            }
            ::std::unique_ptr<::std::size_t[]> indices{};
            if(kind == gc_trace_type::composite_kind::struct_ && references != 0uz
#if defined(UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA == 1
               && field_count > 64uz
#endif
              )
            {
                // [references <= field_count <= max_bytes / sizeof(size_t)]
                // [safe                  ] complete native index-array storage.
                // ^^ acquire only a real array-new result; never a guest token.
#if defined(UWVM2TEST_GC_TRACE_METADATA_ALLOCATION_PROBE) && UWVM2TEST_GC_TRACE_METADATA_ALLOCATION_PROBE == 1
                gc_trace_metadata_allocation_probe(references);
#endif
                indices.reset(new(::std::nothrow) ::std::size_t[references]);
                if(!indices) { return gc_trace_metadata_status::out_of_memory; }
                ::std::size_t position{};
                for(::std::size_t index{}; index != field_count; ++index)
                {
                    // [fields,fields+field_count) complete immutable owned fields.
                    // [safe                     ] index is below its checked end.
                    auto const storage{fields[index].storage};
                    if(storage.packed == gc_trace_type::packed_kind::none &&
                       storage.value.kind == gc_trace_type::value_kind::reference)
                    {
                        if(position == references) { return gc_trace_metadata_status::invalid_layout; }
                        // [indices,indices+references) real allocated array.
                        // [safe                    ] position < references.
                        // ^^ assign the bounded field index, never a payload pointer.
                        indices[position++] = index;
                    }
                }
                if(position != references) { return gc_trace_metadata_status::invalid_layout; }
            }
            // [fully initialized private index array] [empty owned plan]
            // ^^ transfer unique native ownership only after the complete scan.
            reference_fields_ = ::std::move(indices);
            field_count_ = field_count;
            reference_count_ = kind == gc_trace_type::composite_kind::struct_ ? references : 0uz;
            array_reference_ = kind == gc_trace_type::composite_kind::array && references != 0uz;
            kind_ = kind;
#if defined(UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA == 1
            inline_reference_fields_ = inline_fields;
#endif
            ready_ = true;
            return gc_trace_metadata_status::ok;
        }

        [[nodiscard]] inline bool matches_shape(gc_trace_type::composite_kind kind,
                                                ::std::size_t field_count) const noexcept
        { return ready_ && kind_ == kind && field_count_ == field_count; }
        [[nodiscard]] inline bool numeric_leaf() const noexcept
        {
            return ready_ && kind_ != gc_trace_type::composite_kind::function &&
                   reference_count_ == 0uz && !array_reference_;
        }
        [[nodiscard]] inline bool array_elements_are_references() const noexcept
        { return ready_ && kind_ == gc_trace_type::composite_kind::array && array_reference_; }
        [[nodiscard]] inline ::std::size_t struct_reference_count() const noexcept
        { return ready_ && kind_ == gc_trace_type::composite_kind::struct_ ? reference_count_ : 0uz; }
        [[nodiscard]] inline bool reference_field_at(::std::size_t position,
            ::std::size_t actual_field_count, ::std::size_t& result) const noexcept
        {
#if defined(UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA == 1
            if(!ready_ || kind_ != gc_trace_type::composite_kind::struct_ ||
               actual_field_count != field_count_ || position >= reference_count_)
            { return false; }
            if(field_count_ <= 64uz)
            {
                // Compatibility indexed access only. Collection uses the
                // linear set-bit visitor below instead of repeating this select.
                auto remaining{inline_reference_fields_};
                for(::std::size_t skipped{}; skipped != position; ++skipped)
                {
                    if(remaining == 0u) { return false; }
                    remaining &= remaining - 1u;
                }
                if(remaining == 0u) { return false; }
                auto const field{static_cast<::std::size_t>(::std::countr_zero(remaining))};
                if(field >= actual_field_count) { return false; }
                result = field;
                return true;
            }
            if(!reference_fields_) { return false; }
#else
            if(!ready_ || kind_ != gc_trace_type::composite_kind::struct_ ||
               actual_field_count != field_count_ || position >= reference_count_ ||
               !reference_fields_) { return false; }
#endif
            // [reference_fields_,reference_fields_+reference_count_) initialized
            // native indices; position < reference_count_ proves this access.
            // [safe                                        ] no pointer escapes.
            auto const field{reference_fields_[position]};
            if(field >= actual_field_count) { return false; }
            result = field;
            return true;
        }
        [[nodiscard]] inline ::std::size_t allocated_index_bytes() const noexcept
        {
            // Build proved count <= max_bytes/sizeof(size_t); arrays/numeric
            // leaves need no index allocation. Include sizeof(metadata) in
            // total memory accounting separately for EVERY type layout.
#if defined(UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA == 1
            if(field_count_ <= 64uz) { return 0uz; }
#endif
            return reference_count_ * sizeof(::std::size_t);
        }
#if defined(UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA) && UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA == 1
        template<typename Visitor>
        [[nodiscard]] inline bool visit_struct_reference_fields(
            ::std::size_t actual_field_count, Visitor&& visitor) const noexcept
        {
            static_assert(::std::is_nothrow_invocable_r_v<bool, Visitor&, ::std::size_t>);
            if(!matches_shape(gc_trace_type::composite_kind::struct_, actual_field_count))
            { return false; }
            if(field_count_ <= 64uz)
            {
                auto remaining{inline_reference_fields_};
                ::std::size_t visited{};
                while(remaining != 0u)
                {
                    auto const field{static_cast<::std::size_t>(::std::countr_zero(remaining))};
                    if(field >= actual_field_count || visited == reference_count_) { return false; }
                    // [0,actual_field_count) is the caller's authenticated
                    // complete carrier extent, not arbitrary native memory.
                    // [safe                  ] this bounded index classifies
                    // one unpacked reference; the visitor proves membership.
                    if(!visitor(field)) { return false; }
                    ++visited;
                    remaining &= remaining - 1u;
                }
                return visited == reference_count_;
            }
            for(::std::size_t position{}; position != reference_count_; ++position)
            {
                ::std::size_t field{};
                if(!reference_field_at(position, actual_field_count, field) || !visitor(field))
                { return false; }
            }
            return true;
        }
#endif
    };
}
