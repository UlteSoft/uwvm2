/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <memory>
# include <utility>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/validation/standard/wasm3/exception_policy.h>
# include <uwvm2/validation/standard/wasm3/exception_validation.h>
# include <uwvm2/validation/standard/wasm3/reference_policy.h>
# include <uwvm2/uwvm/runtime/storage/impl.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::shared::wasm_exception_control
{
    namespace validation = ::uwvm2::validation::standard::wasm3;
    using value_type = ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_value_type_t;
    using tag_type = ::uwvm2::uwvm::runtime::storage::local_defined_tag_storage_t;
    struct type_tuple
    {
        value_type const* begin{};
        value_type const* end{};
        [[nodiscard]] inline constexpr ::std::size_t size() const noexcept
        { return begin == end ? 0uz : static_cast<::std::size_t>(end - begin); }
    };
    struct tag_info { tag_type const* identity{}; type_tuple parameters{}; ::std::size_t type_index{SIZE_MAX}; };
    struct handler
    {
        tag_type const* identity{}; // null only for catch_all
        ::std::size_t target_frame{}; // absolute OUTER control-frame index
        bool with_reference{}; // catch_ref/catch_all_ref appends one non-null exnref
    };
    using handlers = ::uwvm2::utils::container::vector<handler>;

    [[noreturn]] inline constexpr void unsupported(::std::byte const* opcode, unsigned value,
        ::uwvm2::validation::error::code_validation_error_impl& error) UWVM_THROWS
    {
        // [opcode] ... code_end; the dispatch loop already proved this address readable.
        // [safe  ] no dereference or pointer arithmetic when retaining the diagnostic address.
        // ^^ error.err_curr
        error.err_curr = opcode;
        error.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_opbase;
        error.err_selectable.u8 = value;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
    }

    // Use linked tag INSTANCE identity, never a type index or a module-local tag index. Distinct
    // imports can alias one provider tag, and two local tags with identical signatures remain distinct.
    // Preserve the caller opcode in errors: tags occur in both throw (0x08) and try_table (0x1f).
    [[nodiscard]] inline constexpr tag_info resolve_tag(
        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& module, ::std::uint_least32_t index,
        ::std::byte const* opcode, unsigned opcode_value,
        ::uwvm2::validation::error::code_validation_error_impl& error) UWVM_THROWS
    {
        auto const imports{module.imported_tag_vec_storage.size()};
        tag_type const* identity{};
        ::std::size_t type_index{SIZE_MAX};
        if(index < imports)
        {
            // [imported tag records] end; index < size proves the selected slot readable.
            // [safe               ] initializer owns the resolved target until module retirement.
            // ^^ identity borrows the immutable linked provider, without forming an out-of-range pointer.
            auto const& imported{module.imported_tag_vec_storage.index_unchecked(index)};
            identity = imported.resolved_tag;
            if(imported.import_type_ptr == nullptr) [[unlikely]] { unsupported(opcode, opcode_value, error); }
            type_index = imported.import_type_ptr->imports.storage.tag_type_index;
        }
        else
        {
            auto const local{static_cast<::std::size_t>(index) - imports};
            if(local >= module.local_defined_tag_vec_storage.size()) [[unlikely]] { unsupported(opcode, opcode_value, error); }
            // [local tag records] end; local < size, so addressof refers to a live complete record.
            // [safe             ]
            // ^^ identity
            identity = ::std::addressof(module.local_defined_tag_vec_storage.index_unchecked(local));
            type_index = identity->type_index;
        }
        if(identity == nullptr || identity->function_type_ptr == nullptr) [[unlikely]] { unsupported(opcode, opcode_value, error); }
        auto const& signature{*identity->function_type_ptr};
        if(signature.result.begin != signature.result.end) [[unlikely]] { unsupported(opcode, opcode_value, error); }
        // [validated tag parameter types ...] end; both endpoints borrow the same immutable signature.
        // [safe                            ] an empty range is permitted and is never subtracted/dereferenced.
        return {identity, {signature.parameter.begin, signature.parameter.end}, type_index};
    }

    struct carrier_matching
    {
        [[nodiscard]] inline constexpr bool matches(value_type actual, value_type expected) const noexcept { return actual == expected; }
        [[nodiscard]] inline constexpr bool matches(::uwvm2::parser::wasm::standard::wasm3::type::core_value_type, value_type) const noexcept { return false; }
    };

    // This is the allocation-free EXECUTION lowering for lexically caught throw/catch/catch_all:
    // handlers exist only during compilation. A selected throw becomes the existing tuple branch,
    // preserving ring-cache repair/LLVM PHIs. Escaping throws and retaining *_ref catches still
    // require the separate native exception path; rejecting those must not be mistaken for support.
    struct core3_matching
    {
        ::uwvm2::uwvm::runtime::storage::type_section_storage_t const* types{};
        [[nodiscard]] inline bool matches(
            ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type actual,
            ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type expected) const noexcept
        {
            auto const first{types->owned_signature_begin};
            auto const last{types->owned_signature_end};
            auto const count{first == nullptr || last == nullptr ? 0uz : static_cast<::std::size_t>(last - first)};
            // GC-disabled ordinary 0x60 sections intentionally have no recursive
            // context. Their abstract exception heaps still use Core 3 matching:
            // noexn <: exn, with the same nullability and distinct-family rules as
            // the pure validator and each fused compiler's operand stack.
            // [retained signature/context objects] synchronous owner-pinned borrow
            // [safe                              ] no byte/stack pointer is advanced.
            return validation::core3_value_type_matches_with_context(actual, expected,
                validation::core3_signature_view<::uwvm2::uwvm::runtime::storage::wasm_binfmt1_owned_signature_t>{first, count},
                types->core3_context_ptr);
        }
    };

    template<typename LabelAt, typename RichLabelAt>
    [[nodiscard]] inline constexpr handlers read_handlers(::std::byte const*& cursor, ::std::byte const* end,
        ::std::byte const* opcode, ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& module,
        ::std::size_t frame_count, LabelAt label_at, RichLabelAt rich_label_at,
        ::uwvm2::validation::error::code_validation_error_impl& error) UWVM_THROWS
    {
        // [try_table blocktype] vector ... end
        // [safe               ] unsafe (could be end)
        //                       ^^ next: private cursor; failure leaves the caller at vector start.
        auto next{cursor};
        auto const decoded{validation::scan_exception_catches(next, end)};
        // [try_table blocktype][bounded vector] next ... end
        // [safe                              ] unsafe (could be end)
        //                                      ^^ next after complete decoding, unchanged on malformed input.
        if(decoded.error != validation::exception_immediate_error::ok) [[unlikely]] { unsupported(opcode, 0x1fu, error); }
        handlers result{};
        result.reserve(decoded.clauses.size()); // decoder already bounded count against input and PTRDIFF_MAX
        for(auto const& clause: decoded.clauses)
        {
            auto const tagged{clause.kind == validation::exception_catch_kind::tagged ||
                              clause.kind == validation::exception_catch_kind::tagged_ref};
            auto const with_reference{clause.kind == validation::exception_catch_kind::tagged_ref ||
                                      clause.kind == validation::exception_catch_kind::all_ref};
            auto const tag{tagged ? resolve_tag(module, clause.tag_index, opcode, 0x1fu, error) : tag_info{}};
            if(clause.label_index >= frame_count) [[unlikely]]
            {
                // [try_table] ... end; borrow only the checked opcode address.
                // [safe    ]
                // ^^ error.err_curr
                error.err_curr = opcode;
                error.err_code = ::uwvm2::validation::error::code_validation_error_code::illegal_label_index;
                error.err_selectable.illegal_label_index = {clause.label_index, static_cast<::std::uint_least32_t>(frame_count)};
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
            auto const target{frame_count - 1uz - clause.label_index};
            auto const label{label_at(target)}; // callback receives a proven live OUTER frame, never the new try frame
            auto const label_size{label.begin == label.end ? 0uz : static_cast<::std::size_t>(label.end - label.begin)};
            auto const& types{module.type_section_storage};
            auto const rich_begin{types.owned_signature_begin};
            auto const rich_end{types.owned_signature_end};
            auto const rich_count{rich_begin == nullptr || rich_end == nullptr ? 0uz :
                static_cast<::std::size_t>(rich_end - rich_begin)};
            auto const check{validation::validate_exception_catch_signature(clause.kind, tag.parameters.size(),
                [&](::std::size_t i) constexpr noexcept
                {
                    if(tag.type_index < rich_count)
                    {
                        auto const& values{rich_begin[tag.type_index].parameters};
                        if(values.size() != tag.parameters.size()) { ::fast_io::fast_terminate(); }
                        // [rich tag tuple] end; validator bounded i by tag.parameters.size().
                        // [safe          ] unsafe (one-past)
                        //                 ^^ index_unchecked(i) borrows a live retained value.
                        return values.index_unchecked(i);
                    }
                    // [linked tag tuple] end; i < size proves begin[i] readable.
                    // [safe           ] unsafe (one-past)
                    //                  ^^ begin[i]
                    return validation::core3_legacy_carrier_type(tag.parameters.begin[i]);
                }, label_size,
                [&](::std::size_t i) constexpr noexcept
                {
                    auto const rich{rich_label_at(target, i)};
                    if(rich.has_type) { return rich.type; }
                    // [outer label tuple] end; i < label_size proves begin[i] readable.
                    // [safe            ] unsafe (one-past)
                    //                    ^^ begin[i]
                    return validation::core3_legacy_carrier_type(label.begin[i]);
                }, core3_matching{::std::addressof(types)})};
            if(check != validation::core3_exception_error::ok) [[unlikely]] { unsupported(opcode, 0x1fu, error); }
            result.push_back({tag.identity, target, with_reference}); // lexical catch order is semantic
        }
        // [try_table blocktype checked and resolved clauses] next ... end
        // [safe                                            ] unsafe (could be end)
        //                                                    ^^ cursor: sole commit after all checks, before frame publication.
        cursor = next;
        return result;
    }

    template<typename Frames>
    [[nodiscard]] inline constexpr ::std::size_t find_handler(Frames const& frames, tag_type const* identity) noexcept
    {
        for(auto depth{frames.size()}; depth != 0uz; --depth)
        {
            for(auto const& clause: frames.index_unchecked(depth - 1uz).exception_handlers)
            {
                if(clause.identity == nullptr || clause.identity == identity)
                {
                    // A *_ref catch must materialize an owning exnref; the tuple-branch
                    // shortcut applies only to catches without that extra runtime value.
                    return clause.with_reference ? SIZE_MAX : clause.target_frame;
                }
            }
        }
        return SIZE_MAX;
    }
}
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
