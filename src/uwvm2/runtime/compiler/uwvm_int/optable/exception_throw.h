/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <atomic>
# include <cstddef>
# include <cstring>
# include <limits>
# include <memory>
# include <optional>
# include <span>
# include <utility>
# include <vector>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# include <uwvm2/runtime/exception/impl.h>
# include <uwvm2/uwvm/runtime/storage/gc_object.h>
# include "define.h"
# include "storage.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    // Core 3 throw_ref of a statically proven null exn is a trap, not a guest
    // exception. The compiler emits this terminal opcode only for the adjacent
    // ref.null exn/noexn + throw_ref sequence; no unrooted exnref is materialized.
    template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires (Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_throw_ref_null(Type... /*type*/) UWVM_THROWS
    {
        if(trap_null_reference_func != nullptr) { trap_null_reference_func(); }
        ::fast_io::fast_terminate();
    }

    template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires (!Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_throw_ref_null(Type&... /*type*/) UWVM_THROWS
    {
        if(trap_null_reference_func != nullptr) { trap_null_reference_func(); }
        ::fast_io::fast_terminate();
    }

    namespace translate
    {
        template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        [[nodiscard]] inline constexpr auto get_uwvmint_throw_ref_null_fptr_from_tuple(
            ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            if constexpr(Option.is_tail_call)
            { return static_cast<uwvm_interpreter_opfunc_t<Type...>>(uwvmint_throw_ref_null<Option, Type...>); }
            else
            { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_throw_ref_null<Option, Type...>); }
        }
    }

    // Immutable after construction. The compiled function owns this object until its bytecode is
    // retired, and patches its stable address only after finalizing the instruction stream. Both the
    // linked tag INSTANCE root and the copied parameter kinds outlive the parser/compiler temporaries.
    class exception_throw_site final
    {
        ::uwvm2::runtime::exception::instance_root tag_identity_;
        ::std::vector<::uwvm2::runtime::exception::payload_kind> parameter_kinds_;
        ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_object_store const> source_store_{};
        ::std::size_t argument_bytes_{};

        inline exception_throw_site(::uwvm2::runtime::exception::instance_root tag,
                                    ::std::vector<::uwvm2::runtime::exception::payload_kind> kinds,
                                    ::std::size_t bytes,
                                    ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_object_store const> store) noexcept
            : tag_identity_{::std::move(tag)}, parameter_kinds_{::std::move(kinds)},
              source_store_{::std::move(store)}, argument_bytes_{bytes} {}

    public:
        exception_throw_site(exception_throw_site const&) = delete;
        exception_throw_site& operator=(exception_throw_site const&) = delete;

        // The translator must handle an empty result as unsupported before emitting an opcode.
        // Wasm reference carriers require a source store to validate and root their complete kind+bits;
        // a native pointer-width reference alone cannot represent a Core 3 payload.
        [[nodiscard]] static inline ::std::shared_ptr<exception_throw_site const> make_numeric(
            ::uwvm2::runtime::exception::instance_root tag,
            ::std::span<::uwvm2::runtime::exception::payload_kind const> kinds,
            ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_object_store const> source_store = {}) UWVM_THROWS
        {
            if(!tag || tag.use_count() == 0) { return {}; }
            ::std::size_t bytes{};
            for(auto kind: kinds)
            {
                auto const width{::uwvm2::runtime::exception::payload_width(kind)};
                if(kind == ::uwvm2::runtime::exception::payload_kind::reference ||
                   (kind == ::uwvm2::runtime::exception::payload_kind::wasm_reference && !source_store) ||
                   width == 0uz ||
                   width > static_cast<::std::size_t>(::std::numeric_limits<::std::ptrdiff_t>::max()) - bytes)
                { return {}; }
                bytes += width;
            }
            ::std::vector<::uwvm2::runtime::exception::payload_kind> copied{};
            // [kinds: validated complete signature] end; do not subtract a null empty-span endpoint.
            // [safe                              ] nonempty assignment owns a copy of every kind.
            if(!kinds.empty()) { copied.assign(kinds.begin(), kinds.end()); }
            return ::std::shared_ptr<exception_throw_site const>{
                new exception_throw_site{::std::move(tag), ::std::move(copied), bytes, ::std::move(source_store)}};
        }

        [[nodiscard]] inline ::uwvm2::runtime::exception::instance_root const& tag_identity() const noexcept
        { return tag_identity_; }
        [[nodiscard]] inline ::std::span<::uwvm2::runtime::exception::payload_kind const> parameter_kinds() const noexcept
        { return {parameter_kinds_.data(), parameter_kinds_.size()}; }
        [[nodiscard]] inline ::std::size_t argument_bytes() const noexcept { return argument_bytes_; }
        [[nodiscard]] inline ::uwvm2::uwvm::runtime::storage::gc_object_store const* source_store() const noexcept
        { return source_store_.get(); }
    };

# if defined(UWVM_CPP_EXCEPTIONS)
    // Host-only cold diagnostic hook. Execution admission publishes a capture routine before Wasm
    // runs; the guest has no address/registration API. Nothing reads this atomic on ordinary calls,
    // instructions or memory accesses. The callback may allocate and must not be noexcept.
    using exception_diagnostic_capture_func_t = ::uwvm2::runtime::exception::diagnostic_trace_ref (*)() UWVM_THROWS;
    inline ::std::atomic<exception_diagnostic_capture_func_t> exception_diagnostic_capture_func{};

    // Cold synchronous boundary. arguments borrows a fully spilled, validation-bounded operand tuple.
    // Numeric and full Wasm-reference payload_field construction use memcpy, preserving v128 lanes,
    // signaling-NaN bit patterns and reference kind. Every throw creates a new native activation.
    [[noreturn]] inline void raise_numeric_tuple(exception_throw_site const& site,
                                                ::std::span<::std::byte const> arguments) UWVM_THROWS
    {
        if(arguments.size() != site.argument_bytes()) [[unlikely]] { ::fast_io::fast_terminate(); }
        ::std::vector<::uwvm2::runtime::exception::payload_field> fields{};
        fields.reserve(site.parameter_kinds().size());
        ::std::size_t offset{};
        for(auto kind: site.parameter_kinds())
        {
            auto const width{::uwvm2::runtime::exception::payload_width(kind)};
            // [already copied prefix][complete numeric field][remaining tuple] end
            // [safe                  ^^^^^^^^^^^^^^^^^^^^^^^] constructor checked all widths and their
            // sum; arguments.size() equals that sum. No cursor advances beyond the tuple's endpoint.
            ::std::optional<::uwvm2::runtime::exception::payload_field> field{};
            if(kind == ::uwvm2::runtime::exception::payload_kind::wasm_reference)
            {
                ::uwvm2::uwvm::runtime::storage::gc_reference reference{};
                // [copied prefix][complete 16-byte ref][remaining tuple] end
                // [safe         ^^^^^^^^^^^^^^^^^^^^] site.argument_bytes equals the validated
                // payload sum; `offset + width` is inside that span before forming data()+offset.
                //                 ^^ source starts at arguments.data() + offset.
                ::std::memcpy(::std::addressof(reference), arguments.data() + offset, sizeof(reference));
                auto const* store{site.source_store()};
                if(store == nullptr || store->retain_gc_reference(reference) !=
                   ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
                { ::fast_io::fast_terminate(); }
                // A local managed reference is rooted by source_store_ itself, so taking an
                // additional shared_ptr back to that store would form a store -> exn -> value
                // -> store cycle. Foreign references get an independent root from this helper;
                // retain_gc_reference above has already validated either representation.
                auto root{store->root_reference(reference)};
                field = ::uwvm2::runtime::exception::payload_field::wasm_reference(
                    arguments.subspan(offset, width), ::std::move(root));
            }
            else { field = ::uwvm2::runtime::exception::payload_field::numeric(kind, arguments.subspan(offset, width)); }
            if(!field) [[unlikely]] { ::fast_io::fast_terminate(); }
            fields.push_back(*field);
            offset += width;
        }
        // Copy the live throwing stack BEFORE C++ propagation destroys callee frame guards. Capture
        // returns an immutable owner, never TLS/frame/name-section views. Allocation failure remains
        // a host exception; it is neither a guest catch_all match nor an unreachable trap.
        auto const capture{exception_diagnostic_capture_func.load(::std::memory_order_acquire)};
        auto diagnostic{capture == nullptr ? ::uwvm2::runtime::exception::diagnostic_trace_ref{} : capture()};
        auto instance{::uwvm2::runtime::exception::value::make_owned(site.tag_identity(), ::std::move(fields), ::std::move(diagnostic))};
        if(!instance) [[unlikely]] { ::fast_io::fast_terminate(); }
        ::uwvm2::runtime::exception::throw_value(::std::move(instance));
    }

    namespace details
    {
        [[noreturn]] inline void raise_exception_reference_from_stack(::std::byte const* top) UWVM_THROWS
        {
            if(top == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            ::uwvm2::uwvm::runtime::storage::gc_reference reference{};
            // [live caller operand prefix][complete 16-byte exnref] top
            // [safe                    ^^^^^^^^^^^^^^^^^^^^^^^^] the translator proved
            // the typed operand width and spilled its carrier before this cold opcode.
            //                           ^^ top - sizeof(reference) stays in the live frame.
            ::std::memcpy(::std::addressof(reference), top - sizeof(reference), sizeof(reference));
            using ref_kind = ::uwvm2::object::global::wasm_ref_kind;
            if(reference.kind == ref_kind::wasm_null ||
               (reference.kind == ref_kind::wasm_exn && reference.storage.ptr == nullptr))
            {
                if(trap_null_reference_func != nullptr) { trap_null_reference_func(); }
                ::fast_io::fast_terminate();
            }
            auto value{::uwvm2::uwvm::runtime::storage::gc_object_store::lookup_exn_reference(reference)};
            if(!value) [[unlikely]] { ::fast_io::fast_terminate(); }
            ::uwvm2::runtime::exception::throw_value(::std::move(value));
        }

        [[noreturn]] inline void raise_exception_from_stack(
            ::std::byte const* site_slot, ::std::byte const* top) UWVM_THROWS
        {
            exception_throw_site const* site;
            // [throw opfunc][complete compiler-owned site pointer]
            // [safe        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^] memcpy accepts an unaligned slot.
            ::std::memcpy(::std::addressof(site), site_slot, sizeof(site));
            if(site == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            // [caller prefix][fully spilled arguments] top
            // [safe                                  ]; translation proves argument_bytes <= the
            // live operand extent before emitting this opcode. Even an empty tuple keeps its top.
            auto const arguments{top - site->argument_bytes()};
            //                ^^ arguments begins the complete borrowed tuple; no pointer escapes.
            raise_numeric_tuple(*site, {arguments, site->argument_bytes()});
        }
    }

    template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires (Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_throw_numeric(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 3uz);
        // [throw opfunc][complete site pointer]
        // [safe        ^^^^^^^^^^^^^^^^^^^^^^] dispatch already checked the opcode slot. This opcode
        // has no successor: its real throwing native call, rather than dispatch, ends the activation.
        auto const site_slot{type...[0] + sizeof(uwvm_interpreter_opfunc_t<Type...>)};
        details::raise_exception_from_stack(site_slot, type...[1]);
    }

    template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires (!Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline constexpr void uwvmint_throw_numeric(Type&... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 3uz);
        static_assert(Option.i32_stack_top_begin_pos == SIZE_MAX && Option.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(Option.i64_stack_top_begin_pos == SIZE_MAX && Option.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(Option.f32_stack_top_begin_pos == SIZE_MAX && Option.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(Option.f64_stack_top_begin_pos == SIZE_MAX && Option.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(Option.v128_stack_top_begin_pos == SIZE_MAX && Option.v128_stack_top_end_pos == SIZE_MAX);
        // [throw opfunc][complete site pointer]
        // [safe        ^^^^^^^^^^^^^^^^^^^^^^] advancing skips exactly the validated opcode slot;
        // byref IP/top remain untouched because this native activation always exits exceptionally.
        auto const site_slot{type...[0] + sizeof(uwvm_interpreter_opfunc_byref_t<Type...>)};
        details::raise_exception_from_stack(site_slot, type...[1]);
    }

    namespace translate
    {
        template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
            requires (Option.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline void uwvmint_throw_ref(Type... type) UWVM_THROWS
        { details::raise_exception_reference_from_stack(type...[1]); }

        template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
            requires (!Option.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline void uwvmint_throw_ref(Type&... type) UWVM_THROWS
        { details::raise_exception_reference_from_stack(type...[1]); }

        template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        [[nodiscard]] inline constexpr auto get_uwvmint_throw_ref_fptr_from_tuple(
            ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            if constexpr(Option.is_tail_call)
            { return static_cast<uwvm_interpreter_opfunc_t<Type...>>(uwvmint_throw_ref<Option, Type...>); }
            else
            { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_throw_ref<Option, Type...>); }
        }

        template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        [[nodiscard]] inline constexpr auto get_uwvmint_throw_numeric_fptr_from_tuple(
            ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            if constexpr(Option.is_tail_call)
            { return static_cast<uwvm_interpreter_opfunc_t<Type...>>(uwvmint_throw_numeric<Option, Type...>); }
            else
            { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_throw_numeric<Option, Type...>); }
        }
    }
# endif
}
#endif
#ifndef UWVM_MODULE
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
