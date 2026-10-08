/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstring>
# include <limits>
# include <memory>
# include <span>
# include <utility>
# include <vector>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/storage/gc_object.h>
# include "exception.h"
# include "exception_throw.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
#if defined(UWVM_RUNTIME_UWVM_INTERPRETER) && defined(UWVM_CPP_EXCEPTIONS)
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::uwvm_int::optable
{
    // Final compiler input, in lexical selection order: innermost try first, catches in source order.
    // The tag names an INSTANCE, not its signature or module-local index. catch_all has no tag/payload.
    // target_ip_offset names an emitted recovery thunk whose logical register caches are empty or
    // explicitly reloaded. prefix_bytes retains the caller's live operand prefix beneath that payload.
    struct exception_numeric_handler_spec
    {
        ::uwvm2::runtime::exception::instance_root tag{};
        bool catch_all{};
        bool with_reference{};
        ::std::shared_ptr<::uwvm2::uwvm::runtime::storage::gc_object_store const> receiving_store{};
        ::std::size_t target_ip_offset{};
        ::std::size_t prefix_bytes{};
        ::std::vector<::uwvm2::runtime::exception::payload_kind> parameter_kinds{};
    };

    struct exception_numeric_call_spec
    {
        // Fully spilled operand bytes at protected-opcode entry, INCLUDING parameters and any
        // indirect-call index. invoke_with_exception_handler retains this original top across the call.
        ::std::size_t stack_bytes_at_call{};
        ::std::vector<exception_numeric_handler_spec> handlers{};
    };

    // One immutable owner travels with local_func_storage_t. It pins descriptors, handler signatures,
    // tag instances and throw sites independently of parser/compiler temporaries. Its bytecode view
    // borrows the SAME function's finalized op.operands allocation, which must remain pinned while
    // executing or borrowing a call_site. Moving local_func_storage_t transfers that allocation intact.
    // Keeping this metadata alone does not extend the lifetime of retired executable bytecode.
    class exception_function_metadata final
    {
        struct handler
        {
            exception_numeric_handler_spec spec{};
            ::std::size_t payload_bytes{};
        };
        struct call_context
        {
            exception_function_metadata const* owner{};
            ::std::size_t stack_bytes_at_call{};
            ::std::vector<handler> handlers{};
            exception_call_site site{};
        };
        ::std::span<::std::byte const> code_{};
        ::std::size_t operand_capacity_{};
        ::std::vector<call_context> calls_{};
        ::std::vector<::std::shared_ptr<exception_throw_site const>> throws_{};

        inline exception_function_metadata(::std::span<::std::byte const> code, ::std::size_t operand_capacity) noexcept
            : code_{code}, operand_capacity_{operand_capacity} {}

        [[nodiscard]] static inline exception_continuation dispatch_numeric(
            void const* opaque, ::uwvm2::runtime::exception::value_ref const& instance,
            [[maybe_unused]] ::std::byte const* locals, ::std::byte* top) noexcept
        {
            if(opaque == nullptr || !instance) [[unlikely]] { ::fast_io::fast_terminate(); }
            // [immutable owner.calls_ array ... complete call_context ...] end
            // [safe                                                     ] factory installs this borrow
            // only AFTER final array allocation; no subsequent resize or move is permitted.
            auto const& context{*static_cast<call_context const*>(opaque)};
            if(context.owner == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
            for(auto const& selected: context.handlers)
            {
                auto const& spec{selected.spec};
                if(!spec.catch_all && spec.tag.get() != instance->tag_identity()) { continue; }
                auto const fields{instance->fields()};
                if(!spec.catch_all)
                {
                    // Prove the WHOLE tuple before writing any caller byte. A matching tag with a
                    // different signature is a runtime invariant violation, never a reason to choose
                    // a later catch. Complete Wasm reference carriers are copied only after
                    // their receiving store has accepted an independent root.
                    if(fields.size() != spec.parameter_kinds.size()) [[unlikely]] { ::fast_io::fast_terminate(); }
                    for(::std::size_t index{}; index != fields.size(); ++index)
                    {
                        auto const kind{spec.parameter_kinds[index]};
                        if(fields[index].kind() != kind || fields[index].bits().size() !=
                            ::uwvm2::runtime::exception::payload_width(kind)) [[unlikely]]
                        { ::fast_io::fast_terminate(); }
                    }
                }
                if(top == nullptr && (context.stack_bytes_at_call != 0uz || selected.payload_bytes != 0uz)) [[unlikely]]
                { ::fast_io::fast_terminate(); }
                // [caller operand base ... fully spilled live prefix/arguments] top [spare capacity]
                // [safe                                                       ] factory checked depth
                // <= capacity; the protected opcode lends its original live caller top synchronously.
                // ^^ base is derived within that frame. Zero depth preserves a possible empty sentinel.
                auto const base{context.stack_bytes_at_call == 0uz ? top : top - context.stack_bytes_at_call};
                // [retained caller prefix][destination payload ...] frame end
                // [safe                  ^^^^^^^^^^^^^^^^^^^^^^^^^] factory proved prefix <= old depth
                // and prefix + payload_bytes <= operand capacity, using checked integer extents.
                auto cursor{spec.prefix_bytes == 0uz ? base : base + spec.prefix_bytes};
                if(!spec.catch_all)
                {
                    for(auto const& field: fields)
                    {
                        auto const bits{field.bits()};
                        if(field.kind() == ::uwvm2::runtime::exception::payload_kind::wasm_reference)
                        {
                            ::uwvm2::uwvm::runtime::storage::gc_reference reference{};
                            ::std::memcpy(::std::addressof(reference), bits.data(), sizeof(reference));
                            if(!spec.receiving_store || spec.receiving_store->retain_gc_reference(reference) !=
                               ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
                            { ::fast_io::fast_terminate(); }
                        }
                        // [immutable numeric field: 4/8/16 bytes] [checked destination remainder]
                        // [safe                                ] memcpy preserves unaligned packed
                        // values, v128 lanes and all NaN bits without floating-point evaluation.
                        ::std::memcpy(cursor, bits.data(), bits.size());
                        // [copied fields][complete next field][remaining payload ...] operand end
                        // [safe        ][safe              ] unsafe (possibly one-past)
                        //                ^^ cursor: factory proved the next field fits before this advance.
                        cursor += bits.size();
                        // [copied fields including this field] next ... operand end
                        // [safe                             ] unsafe (possibly one-past)
                        //                                     ^^ cursor: never dereferenced after the final field.
                    }
                }
                if(spec.with_reference)
                {
                    ::uwvm2::uwvm::runtime::storage::gc_reference reference{};
                    if(spec.receiving_store->make_exn_reference(instance, reference) !=
                       ::uwvm2::uwvm::runtime::storage::gc_object_status::ok) [[unlikely]]
                    { ::fast_io::fast_terminate(); }
                    // [validated payload][full 16-byte exnref][spare operand capacity]
                    // [safe           ^^^^^^^^^^^^^^^^^^^^^] factory reserved the complete carrier.
                    //                  ^^ cursor advances only after memcpy, possibly to one-past.
                    ::std::memcpy(cursor, ::std::addressof(reference), sizeof(reference));
                    cursor += sizeof(reference);
                    // [validated payload][complete exnref] next ... operand end
                    // [safe                        ] unsafe (possibly one-past)
                    //                                ^^ cursor: the factory reserved this exact carrier.
                }
                // [final function bytecode ...][complete recovery opfunc][...] end
                // [safe                       ^^^^^^^^^^^^^^^^^^^^^^^^^^] factory checked the full
                // opcode width at this offset. No guest/parser address is used as an instruction IP.
                auto const target{context.owner->code_.data() + spec.target_ip_offset};
                return {target, cursor};
            }
            // Typed native catch rethrows this unchanged value_ref when no lexical handler matches.
            return {};
        }

    public:
        exception_function_metadata(exception_function_metadata const&) = delete;
        exception_function_metadata& operator=(exception_function_metadata const&) = delete;

        // Validate/copy before publishing addresses into bytecode. nullptr means invalid/unsupported
        // descriptors; the compiler must reject them before execution. Complete Wasm reference
        // carriers and catch_ref use the receiving store for validated root transfer.
        [[nodiscard]] static inline ::std::shared_ptr<exception_function_metadata const> make_numeric(
            ::std::span<::std::byte const> final_code, ::std::size_t opfunc_width, ::std::size_t operand_capacity,
            ::std::span<exception_numeric_call_spec const> call_specs,
            ::std::span<::std::shared_ptr<exception_throw_site const> const> throw_sites = {}) UWVM_THROWS
        {
            constexpr auto limit{static_cast<::std::size_t>(::std::numeric_limits<::std::ptrdiff_t>::max())};
            if(final_code.data() == nullptr || final_code.size() > limit || opfunc_width == 0uz ||
               opfunc_width > final_code.size() || operand_capacity > limit) { return {}; }
            auto owner{::std::shared_ptr<exception_function_metadata>{new exception_function_metadata{final_code, operand_capacity}}};
            owner->calls_.reserve(call_specs.size());
            for(auto const& source: call_specs)
            {
                if(source.stack_bytes_at_call > operand_capacity) { return {}; }
                call_context context{};
                context.stack_bytes_at_call = source.stack_bytes_at_call;
                context.handlers.reserve(source.handlers.size());
                for(auto const& spec: source.handlers)
                {
                    if(spec.target_ip_offset > final_code.size() - opfunc_width ||
                       spec.prefix_bytes > source.stack_bytes_at_call) { return {}; }
                    if(spec.catch_all)
                    {
                        if(spec.tag || spec.tag.use_count() != 0 || !spec.parameter_kinds.empty()) { return {}; }
                    }
                    else if(!spec.tag || spec.tag.use_count() == 0) { return {}; }
                    if(spec.with_reference && !spec.receiving_store) { return {}; }
                    ::std::size_t bytes{};
                    for(auto kind: spec.parameter_kinds)
                    {
                        auto const width{::uwvm2::runtime::exception::payload_width(kind)};
                        if(kind == ::uwvm2::runtime::exception::payload_kind::reference ||
                           (kind == ::uwvm2::runtime::exception::payload_kind::wasm_reference && !spec.receiving_store) ||
                           width == 0uz ||
                           width > limit - bytes) { return {}; }
                        bytes += width;
                    }
                    if(spec.with_reference)
                    {
                        constexpr auto ref_width{sizeof(::uwvm2::uwvm::runtime::storage::gc_reference)};
                        if(ref_width > limit - bytes) { return {}; }
                        bytes += ref_width;
                    }
                    if(bytes > operand_capacity - spec.prefix_bytes) { return {}; }
                    context.handlers.push_back(handler{spec, bytes});
                }
                owner->calls_.push_back(::std::move(context));
            }
            owner->throws_.reserve(throw_sites.size());
            for(auto const& site: throw_sites)
            {
                if(!site) { return {}; }
                owner->throws_.push_back(site);
            }
            // All owning vectors have their FINAL sizes. Publish borrows only now: exceptions during
            // allocation/copy above cannot leave an executable descriptor or dangling bytecode patch.
            for(auto& context: owner->calls_)
            {
                // [stable metadata allocation] owns calls_/handlers_/tags_/throws_ for the function.
                // [safe                      ] owner.get() stays stable across shared_ptr/local_func moves.
                context.owner = owner.get();
                // [final calls_ element][site] no vector reallocation follows this publication.
                // [safe                 ] context and dispatch have identical metadata-owner lifetime.
                context.site = {dispatch_numeric, ::std::addressof(context)};
            }
            return owner;
        }

        [[nodiscard]] inline ::std::size_t call_count() const noexcept { return calls_.size(); }
        [[nodiscard]] inline ::std::size_t operand_capacity() const noexcept { return operand_capacity_; }
        [[nodiscard]] inline exception_call_site const* call_site(::std::size_t index) const noexcept
        {
            if(index >= calls_.size()) { return nullptr; }
            // [fixed calls_ array][element index].site is a complete descriptor, borrowed from *this.
            return ::std::addressof(calls_[index].site);
        }
    };

    // A lexical catch_ref needs the SAME owning native exception that a protected
    // call produces. This cold throw opcode catches only the guest exception it
    // just raised, then resumes through the immutable validated handler table.
    namespace details
    {
        [[nodiscard]] inline exception_continuation raise_with_exception_handler(
            ::std::byte const* site_slot, ::std::byte* top, ::std::byte const* locals) UWVM_THROWS
        {
            auto const dispatch_slot{site_slot + sizeof(exception_throw_site const*)};
            // [throw-site pointer][complete call-site pointer] bytecode end
            // [safe             ^^^^^^^^^^^^^^^^^^^^^^^^^^^] translator emits both complete
            // slots before final metadata publication; no guest address is dereferenced.
            try { raise_exception_from_stack(site_slot, top); }
            catch(::uwvm2::runtime::exception::guest_exception const& caught)
            {
                exception_call_site const* site{};
                ::std::memcpy(::std::addressof(site), dispatch_slot, sizeof(site));
                if(site == nullptr || site->dispatch == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto const resumed{site->dispatch(site->context, caught.instance(), locals, top)};
                if(resumed.ip == nullptr) { throw; }
                return resumed;
            }
        }

        [[nodiscard]] inline exception_continuation raise_reference_with_exception_handler(
            ::std::byte const* dispatch_slot, ::std::byte* top, ::std::byte const* locals) UWVM_THROWS
        {
            // [complete call-site pointer] bytecode end
            // [safe                     ] translator reserves the whole slot before final publication.
            // ^^ dispatch_slot is read only after a real guest exception is raised.
            try { raise_exception_reference_from_stack(top); }
            catch(::uwvm2::runtime::exception::guest_exception const& caught)
            {
                exception_call_site const* site{};
                ::std::memcpy(::std::addressof(site), dispatch_slot, sizeof(site));
                if(site == nullptr || site->dispatch == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
                auto const resumed{site->dispatch(site->context, caught.instance(), locals, top)};
                if(resumed.ip == nullptr) { throw; }
                return resumed;
            }
        }
    }

    template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires (Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline void uwvmint_throw_catching(Type... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 3uz);
        auto const slot{type...[0] + sizeof(uwvm_interpreter_opfunc_t<Type...>)};
        // [opfunc][site pointer][call-site pointer] bytecode end
        // [safe ]^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ translator reserved all slots.
        //        ^^ slot borrows the checked immediate tuple.
        auto const resumed{details::raise_with_exception_handler(slot, type...[1], type...[2])};
        // recovery bytecode: [owned finalized code ...][complete handler opfunc] | code_end
        //                    [factory-checked target and opfunc width       ] safe for dispatch
        // ^^ type...[0] takes resumed.ip; the executing function pins code borrowed by immutable exception metadata.
        type...[0] = resumed.ip;
        // caller frame: operand_base ... [retained prefix][checked payload] | frame_end
        //               [factory-checked prefix + payload capacity       ] unsafe past frame_end
        // ^^ type...[1] takes resumed.top within the still-live caller operand frame.
        type...[1] = resumed.top;
        uwvm_interpreter_opfunc_t<Type...> next{};
        ::std::memcpy(::std::addressof(next), type...[0], sizeof(next));
        UWVM_MUSTTAIL return next(type...);
    }

    template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        requires (!Option.is_tail_call)
    UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline void uwvmint_throw_catching(Type&... type) UWVM_THROWS
    {
        static_assert(sizeof...(Type) >= 3uz);
        static_assert(Option.i32_stack_top_begin_pos == SIZE_MAX && Option.i32_stack_top_end_pos == SIZE_MAX);
        static_assert(Option.i64_stack_top_begin_pos == SIZE_MAX && Option.i64_stack_top_end_pos == SIZE_MAX);
        static_assert(Option.f32_stack_top_begin_pos == SIZE_MAX && Option.f32_stack_top_end_pos == SIZE_MAX);
        static_assert(Option.f64_stack_top_begin_pos == SIZE_MAX && Option.f64_stack_top_end_pos == SIZE_MAX);
        static_assert(Option.v128_stack_top_begin_pos == SIZE_MAX && Option.v128_stack_top_end_pos == SIZE_MAX);
        auto const slot{type...[0] + sizeof(uwvm_interpreter_opfunc_byref_t<Type...>)};
        // [opfunc][site pointer][call-site pointer] bytecode end
        // [safe ]^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ translator reserved all slots.
        //        ^^ slot borrows the checked immediate tuple.
        auto const resumed{details::raise_with_exception_handler(slot, type...[1], type...[2])};
        // recovery bytecode: [owned finalized code ...][complete handler opfunc] | code_end
        //                    [factory-checked target and opfunc width       ] safe for dispatch
        // ^^ type...[0] takes resumed.ip; the executing function pins code borrowed by immutable exception metadata.
        type...[0] = resumed.ip;
        // caller frame: operand_base ... [retained prefix][checked payload] | frame_end
        //               [factory-checked prefix + payload capacity       ] unsafe past frame_end
        // ^^ type...[1] takes resumed.top within the still-live caller operand frame.
        type...[1] = resumed.top;
    }

    namespace translate
    {
        template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
            requires (Option.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline void uwvmint_throw_ref_catching(Type... type) UWVM_THROWS
        {
            static_assert(sizeof...(Type) >= 3uz);
            auto const slot{type...[0] + sizeof(uwvm_interpreter_opfunc_t<Type...>)};
            // [opfunc][complete call-site pointer] bytecode end
            // [safe ]^^^^^^^^^^^^^^^^^^^^^^^^^^^^ translator reserved both slots.
            //        ^^ slot
            auto const resumed{details::raise_reference_with_exception_handler(slot, type...[1], type...[2])};
            // recovery bytecode: [owned finalized code ...][complete handler opfunc] | code_end
            //                    [factory-checked target and opfunc width       ] safe for dispatch
            // ^^ type...[0] takes resumed.ip; the executing function pins code borrowed by immutable exception metadata.
            type...[0] = resumed.ip;
            // caller frame: operand_base ... [retained prefix][checked payload] | frame_end
            //               [factory-checked prefix + payload capacity       ] unsafe past frame_end
            // ^^ type...[1] takes resumed.top within the still-live caller operand frame.
            type...[1] = resumed.top;
            uwvm_interpreter_opfunc_t<Type...> next{};
            ::std::memcpy(::std::addressof(next), type...[0], sizeof(next));
            UWVM_MUSTTAIL return next(type...);
        }

        template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
            requires (!Option.is_tail_call)
        UWVM_INTERPRETER_OPFUNC_HOT_MACRO inline void uwvmint_throw_ref_catching(Type&... type) UWVM_THROWS
        {
            static_assert(sizeof...(Type) >= 3uz);
            static_assert(Option.i32_stack_top_begin_pos == SIZE_MAX && Option.i32_stack_top_end_pos == SIZE_MAX);
            static_assert(Option.i64_stack_top_begin_pos == SIZE_MAX && Option.i64_stack_top_end_pos == SIZE_MAX);
            static_assert(Option.f32_stack_top_begin_pos == SIZE_MAX && Option.f32_stack_top_end_pos == SIZE_MAX);
            static_assert(Option.f64_stack_top_begin_pos == SIZE_MAX && Option.f64_stack_top_end_pos == SIZE_MAX);
            static_assert(Option.v128_stack_top_begin_pos == SIZE_MAX && Option.v128_stack_top_end_pos == SIZE_MAX);
            auto const slot{type...[0] + sizeof(uwvm_interpreter_opfunc_byref_t<Type...>)};
            // [opfunc][complete call-site pointer] bytecode end
            // [safe ]^^^^^^^^^^^^^^^^^^^^^^^^^^^^ translator reserved both slots.
            //        ^^ slot
            auto const resumed{details::raise_reference_with_exception_handler(slot, type...[1], type...[2])};
            // recovery bytecode: [owned finalized code ...][complete handler opfunc] | code_end
            //                    [factory-checked target and opfunc width       ] safe for dispatch
            // ^^ type...[0] takes resumed.ip; the executing function pins code borrowed by immutable exception metadata.
            type...[0] = resumed.ip;
            // caller frame: operand_base ... [retained prefix][checked payload] | frame_end
            //               [factory-checked prefix + payload capacity       ] unsafe past frame_end
            // ^^ type...[1] takes resumed.top within the still-live caller operand frame.
            type...[1] = resumed.top;
        }

        template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        [[nodiscard]] inline constexpr auto get_uwvmint_throw_ref_catching_fptr_from_tuple(
            ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            if constexpr(Option.is_tail_call)
            { return static_cast<uwvm_interpreter_opfunc_t<Type...>>(uwvmint_throw_ref_catching<Option, Type...>); }
            else
            { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_throw_ref_catching<Option, Type...>); }
        }

        template<uwvm_interpreter_translate_option_t Option, uwvm_int_stack_top_type... Type>
        [[nodiscard]] inline constexpr auto get_uwvmint_throw_catching_fptr_from_tuple(
            ::uwvm2::utils::container::tuple<Type...> const&) noexcept
        {
            if constexpr(Option.is_tail_call)
            { return static_cast<uwvm_interpreter_opfunc_t<Type...>>(uwvmint_throw_catching<Option, Type...>); }
            else
            { return static_cast<uwvm_interpreter_opfunc_byref_t<Type...>>(uwvmint_throw_catching<Option, Type...>); }
        }
    }
}
#endif
#ifndef UWVM_MODULE
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/runtime/compiler/uwvm_int/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
