#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1
#pragma once
#ifndef UWVM_MODULE
# include "immutable_value.h"
# include "activation.h"
#endif
#else
/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once

#ifndef UWVM_MODULE
# include <algorithm>
# include <fast_io_dsal/array.h>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <exception>
# include <memory>
# include <mutex>
# include <optional>
# include <span>
# include <string>
# include <string_view>
# include <utility>
# include <vector>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/object/global/ref.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::exception
{
    // Core 3 execution/runtime: an exception INSTANCE is a tag address and immutable argument values.
    // Its lifetime is independent of a native throw activation. In particular, throw_ref must not reuse
    // an _Unwind_Exception header saved by an earlier throw or held in a thread-local singleton.
    // https://webassembly.github.io/spec/core/exec/runtime.html#exception-instances
    enum class payload_kind : unsigned char { i32, i64, f32, f64, v128, reference, wasm_reference };

    [[nodiscard]] inline constexpr ::std::size_t payload_width(payload_kind kind) noexcept
    {
        switch(kind)
        {
            case payload_kind::i32: case payload_kind::f32: return 4uz;
            case payload_kind::i64: case payload_kind::f64: return 8uz;
            case payload_kind::v128: return 16uz;
            case payload_kind::reference: return sizeof(void*);
            case payload_kind::wasm_reference: return sizeof(::uwvm2::object::global::wasm_global_ref_t);
            default: return 0uz;
        }
    }

    // A root token owns (or registers a collector root for) the referenced instance. Aliasing handles
    // are permitted, but their owner must keep the aliased instance alive. Empty-owner aliases are
    // rejected. Tokens must not point into an interpreter frame, a JIT alloca, or a temporary catch.
    // Collector integration must use root registrations, not shared ownership of a mutable cyclic heap.
    using instance_root = ::std::shared_ptr<void const>;

    class payload_field
    {
        payload_kind kind_{payload_kind::i32};
        ::fast_io::array<::std::byte, 16uz> bits_{};
        instance_root root_{};

        inline void clear_moved_from_reference() noexcept
        {
            if(kind_ == payload_kind::wasm_reference)
            {
                // The moved-to field owns the heap root; leave a complete null
                // Wasm carrier in the still accessible moved-from field.
                for(auto& bit: bits_) { bit = ::std::byte{}; }
                return;
            }
            if(kind_ != payload_kind::reference) { return; }
            // root_ has moved to the destination. Leave a valid native null in
            // the source rather than unrooted bits borrowing the old referent.
            void const* pointer{};
            ::std::memcpy(bits_.data(), ::std::addressof(pointer), sizeof(pointer));
        }

    public:
        // A checked update of a caller-owned mutable numeric field. Reject
        // reference owners and malformed shapes before changing any byte.
        [[nodiscard]] inline bool assign_numeric_unowned(payload_kind kind,
            ::std::span<::std::byte const> bits) noexcept
        {
            auto const width{payload_width(kind)};
            if(kind == payload_kind::reference || kind == payload_kind::wasm_reference ||
               width == 0uz || bits.size() != width) { return false; }
            instance_root const empty{};
            if(root_.owner_before(empty) || empty.owner_before(root_)) { return false; }
            kind_ = kind;
            if(width == 4uz) { ::std::memcpy(bits_.data(), bits.data(), 4uz); }
            else if(width == 8uz) { ::std::memcpy(bits_.data(), bits.data(), 8uz); }
            else { ::std::memcpy(bits_.data(), bits.data(), 16uz); }
            // Only bits()'s exact current width is observable. Keep the live
            // field and its EMPTY owner; unused numeric tail bytes are private.
            return true;
        }

        payload_field() noexcept = default;
        payload_field(payload_field const&) noexcept = default;
        payload_field& operator=(payload_field const&) noexcept = default;

        inline payload_field(payload_field&& other) noexcept
            : kind_{other.kind_}, bits_{other.bits_}, root_{::std::move(other.root_)}
        {
            other.clear_moved_from_reference();
        }

        inline payload_field& operator=(payload_field&& other) noexcept
        {
            if(this != ::std::addressof(other))
            {
                kind_ = other.kind_;
                bits_ = other.bits_;
                root_ = ::std::move(other.root_);
                other.clear_moved_from_reference();
            }
            return *this;
        }

        [[nodiscard]] static inline ::std::optional<payload_field> numeric(
            payload_kind kind, ::std::span<::std::byte const> bits) noexcept
        {
            auto const width{payload_width(kind)};
            if(kind == payload_kind::reference || kind == payload_kind::wasm_reference ||
               width == 0uz || bits.size() != width) { return {}; }
            payload_field result{};
            result.kind_ = kind;
            // [source: width initialized bytes] [destination: 16 bytes]
            // [safe                           ] width is exactly 4, 8, or 16; memcpy avoids alignment,
            // aliasing and floating-point evaluation (including signaling-NaN payload changes).
            // Literal widths avoid a per-field generic memcpy dispatch.
            if(width == 4uz) { ::std::memcpy(result.bits_.data(), bits.data(), 4uz); }
            else if(width == 8uz) { ::std::memcpy(result.bits_.data(), bits.data(), 8uz); }
            else { ::std::memcpy(result.bits_.data(), bits.data(), 16uz); }
            return result;
        }

        [[nodiscard]] static inline payload_field null_reference() noexcept
        {
            payload_field result{};
            result.kind_ = payload_kind::reference;
            // Store the native null pointer representation, not an assumed all-zero object representation.
            void const* pointer{};
            ::std::memcpy(result.bits_.data(), ::std::addressof(pointer), sizeof(pointer));
            return result;
        }

        // Canonical reference identity/root representation only, not a raw packed Wasm ABI struct.
        // Backends must reconstruct their carrier with the reference-kind/type codec; never memcpy
        // this pointer-width field as a complete wasm_global_ref_t (which also carries its kind).
        [[nodiscard]] static inline ::std::optional<payload_field> rooted_reference(instance_root root) noexcept
        {
            if(!root || root.use_count() == 0) { return {}; }
            payload_field result{};
            result.kind_ = payload_kind::reference;
            result.root_ = ::std::move(root);
            // [root-owned live instance]
            //  ^^ pointer is borrowed only while root_ retains this instance/root registration.
            auto const pointer{result.root_.get()};
            ::std::memcpy(result.bits_.data(), ::std::addressof(pointer), sizeof(pointer));
            return result;
        }

        // An i31 value is an unboxed reference. Its actual pointer-width carrier is supplied by the
        // runtime's reference encoder; unlike a heap reference it has no instance requiring a root.
        // This HOST-ONLY entry must never accept arbitrary bytes supplied by a guest or debug transport.
        [[nodiscard]] static inline payload_field unboxed_i31(::std::uintptr_t encoded) noexcept
        {
            static_assert(sizeof(encoded) == sizeof(void*));
            payload_field result{};
            result.kind_ = payload_kind::reference;
            ::std::memcpy(result.bits_.data(), ::std::addressof(encoded), sizeof(encoded));
            return result;
        }

        // A Wasm reference is the complete kind+payload carrier (16 bytes on 64-bit
        // hosts, potentially smaller on 32-bit hosts). The caller
        // validates the carrier with gc_object_store and supplies its independent
        // native root for VM-managed kinds before constructing this immutable field.
        [[nodiscard]] static inline ::std::optional<payload_field> wasm_reference(
            ::std::span<::std::byte const> carrier, instance_root root = {}) noexcept
        {
            static_assert(sizeof(::uwvm2::object::global::wasm_global_ref_t) <= 16uz);
            if(carrier.size() != payload_width(payload_kind::wasm_reference)) { return {}; }
            payload_field result{};
            result.kind_ = payload_kind::wasm_reference;
            result.root_ = ::std::move(root);
            // [complete validated Wasm reference carrier] [16-byte owned field]
            // [safe                                 ] memcpy avoids aliasing/alignment and
            // copies the out-of-band kind together with its opaque payload bits.
            ::std::memcpy(result.bits_.data(), carrier.data(), carrier.size());
            return result;
        }

        [[nodiscard]] inline payload_kind kind() const noexcept { return kind_; }
        // Reference bits are canonical identities or host-encoded i31 primitives. They require the
        // backend reference-kind/type codec before materializing a Wasm stack/register ABI carrier.
        [[nodiscard]] inline ::std::span<::std::byte const> bits() const noexcept
        {
            // [bits_: 16 initialized bytes] end; all constructors preserve a valid kind and width <= 16.
            // [safe                       ] returned view borrows this field and cannot mutate it.
            return {bits_.data(), payload_width(kind_)};
        }
        [[nodiscard]] inline instance_root const& root() const noexcept { return root_; }
    };

    // Optional host-owned diagnostic metadata. Names and indices describe the throwing activation,
    // not a reconstructed handler stack. Source/instruction positions are deliberately absent until
    // a backend can capture them accurately. Module teardown cannot invalidate these copied names.
    struct diagnostic_frame
    {
        ::std::size_t module_id{};
        ::std::size_t function_index{};
        ::std::u8string module_name{};
        ::std::u8string function_name{};
    };

    // Compact throw-site identity. The immutable symbol owner below is built
    // before runtime execution admission. It owns copies of names and never
    // borrows parser strings, module records, bytecode or native stack memory.
    struct diagnostic_frame_index
    {
        ::std::size_t module_id{};
        ::std::size_t function_index{};
    };
    struct diagnostic_function_symbol
    {
        ::std::size_t function_index{};
        ::std::u8string name{};
    };
    struct diagnostic_module_symbols
    {
        ::std::u8string name{};
        ::std::size_t function_count{};
        // Only named functions occupy entries, sorted by their public index.
        // Huge unnamed modules do not allocate one native string per function.
        ::std::vector<diagnostic_function_symbol> functions{};
    };
    struct diagnostic_frame_names
    {
        ::std::u8string_view module_name{};
        ::std::u8string_view function_name{};
    };
    class diagnostic_symbols;
    using diagnostic_symbols_ref = ::std::shared_ptr<diagnostic_symbols const>;

    class diagnostic_symbols final
    {
        ::std::vector<diagnostic_module_symbols> modules_{};
        // A nonempty shared_ptr can alias this table while owning an unrelated
        // object. Keep the factory-issued control-block identity without an
        // extra strong reference, allocation or per-throw weak promotion.
        ::std::weak_ptr<diagnostic_symbols const> canonical_owner_{};
        explicit diagnostic_symbols(::std::vector<diagnostic_module_symbols>&& modules) noexcept
            : modules_{::std::move(modules)} {}
    public:
        diagnostic_symbols(diagnostic_symbols const&) = delete;
        diagnostic_symbols& operator=(diagnostic_symbols const&) = delete;
        diagnostic_symbols(diagnostic_symbols&&) = delete;
        diagnostic_symbols& operator=(diagnostic_symbols&&) = delete;

        // Native-only publisher. An lvalue is copied; after moving a private
        // builder, the caller retires every mutable alias before publication.
        [[nodiscard]] static inline diagnostic_symbols_ref make(
            ::std::vector<diagnostic_module_symbols> modules) UWVM_THROWS
        {
            for(auto const& module: modules)
            {
                ::std::size_t previous{};
                bool first{true};
                for(auto const& function: module.functions)
                {
                    if(function.function_index >= module.function_count ||
                       (!first && function.function_index <= previous)) { return {}; }
                    previous = function.function_index;
                    first = false;
                }
            }
            auto owner{::std::shared_ptr<diagnostic_symbols>{new diagnostic_symbols{::std::move(modules)}}};
            // [complete table][its genuine owning control block] publication
            // [safe                                           ] initialize
            // the weak identity before any immutable owner can escape. Never
            // publish a weak/shared alias that owns another native object.
            owner->canonical_owner_ = owner;
            return owner;
        }
        [[nodiscard]] static inline bool has_canonical_owner(diagnostic_symbols_ref const& owner) noexcept
        {
            return owner && !owner->canonical_owner_.owner_before(owner) &&
                   !owner.owner_before(owner->canonical_owner_);
        }
        [[nodiscard]] inline bool contains(diagnostic_frame_index index) const noexcept
        {
            return index.module_id < modules_.size() &&
                   index.function_index < modules_[index.module_id].function_count;
        }
        [[nodiscard]] inline diagnostic_frame_names names(diagnostic_frame_index index) const noexcept
        {
            if(!contains(index)) { ::std::terminate(); }
            auto const& module{modules_[index.module_id]};
            diagnostic_frame_names result{.module_name = module.name};
            if(module.functions.empty()) { return result; }
            // [immutable sorted native symbols] end
            // [safe                           ] library iterators stay inside
            // the complete vector; no guest pointer or module alias is read.
            auto const found{::std::lower_bound(module.functions.begin(), module.functions.end(), index.function_index,
                [](diagnostic_function_symbol const& symbol, ::std::size_t wanted) noexcept
                { return symbol.function_index < wanted; })};
            if(found != module.functions.end() && found->function_index == index.function_index)
            { result.function_name = found->name; }
            return result;
        }
    };

    class diagnostic_trace;
    using diagnostic_trace_ref = ::std::shared_ptr<diagnostic_trace const>;

    class diagnostic_trace final
    {
        // The names cache is published once and then remains immutable. The
        // compact identities and their independent symbol owner never change.
        // A caught exception that nobody displays does not copy any names.
        mutable ::std::vector<diagnostic_frame> frames_;
        ::std::vector<diagnostic_frame_index> indices_{};
        diagnostic_symbols_ref symbols_{};
        mutable ::std::mutex materialization_{};
        mutable bool names_materialized_{};
        bool truncated_{};
        class construction_key
        {
            friend class diagnostic_trace;
            construction_key() noexcept = default;
        public:
            construction_key(construction_key const&) noexcept = default;
        };

    public:
        diagnostic_trace(diagnostic_trace const&) = delete;
        diagnostic_trace& operator=(diagnostic_trace const&) = delete;

        // The inaccessible key allows make_shared's one-allocation ownership
        // while keeping the public factories as the only native publishers.
        explicit inline diagnostic_trace(construction_key, ::std::vector<diagnostic_frame>&& frames, bool truncated) noexcept
            : frames_{::std::move(frames)}, truncated_{truncated} {}
        explicit inline diagnostic_trace(construction_key, diagnostic_symbols_ref symbols,
            ::std::vector<diagnostic_frame_index>&& indices, bool truncated) noexcept
            : indices_{::std::move(indices)}, symbols_{::std::move(symbols)}, truncated_{truncated} {}

        // A host-only builder supplies innermost-to-outermost frames. Passing an lvalue copies it;
        // moving an exclusive builder avoids duplicating every name on the actual throw cold path.
        // The publisher must retire mutable aliases before handing off a moved builder.
        [[nodiscard]] static inline diagnostic_trace_ref make(::std::vector<diagnostic_frame> frames, bool truncated = false) UWVM_THROWS
        { return ::std::make_shared<diagnostic_trace>(construction_key{}, ::std::move(frames), truncated); }

        [[nodiscard]] static inline diagnostic_trace_ref make_compact(diagnostic_symbols_ref symbols,
            ::std::vector<diagnostic_frame_index> indices, bool truncated = false) UWVM_THROWS
        {
            // Reject even nonempty aliases with the wrong control block
            // before a compact trace claims independent symbol ownership.
            if(!diagnostic_symbols::has_canonical_owner(symbols)) { return {}; }
            for(auto const index: indices) { if(!symbols->contains(index)) { return {}; } }
            return ::std::make_shared<diagnostic_trace>(construction_key{}, ::std::move(symbols), ::std::move(indices), truncated);
        }

        [[nodiscard]] inline bool truncated() const noexcept { return truncated_; }
        [[nodiscard]] inline ::std::size_t frame_count() const noexcept
        { return symbols_ ? indices_.size() : frames_.size(); }

        // Cold indexed diagnostic DATA. Borrowed names remain owned by this
        // immutable trace; reading compact frames never materializes/copies all
        // names or changes ordinary throw, catch, rethrow or printing behavior.
        [[nodiscard]] inline bool frame_at(::std::size_t ordinal,
            diagnostic_frame_index& identity, diagnostic_frame_names& names) const noexcept
        {
            identity = {}; names = {};
            if(symbols_)
            {
                if(ordinal >= indices_.size() || !diagnostic_symbols::has_canonical_owner(symbols_)) { return false; }
                // [owned compact indices ... ordinal<size] end
                // [safe] ordinal bound BEFORE indexing; immutable symbols are retained.
                identity = indices_[ordinal];
                if(!symbols_->contains(identity)) { return false; }
                names = symbols_->names(identity); return true;
            }
            if(ordinal >= frames_.size()) { return false; }
            // [owned immutable expanded frames ... ordinal<size] end
            // [safe] complete vector bound BEFORE borrowing frame/name fields.
            auto const& frame{frames_[ordinal]};
            identity = {frame.module_id, frame.function_index};
            names = {::std::u8string_view{frame.module_name.data(), frame.module_name.size()},
                ::std::u8string_view{frame.function_name.data(), frame.function_name.size()}};
            return true;
        }

        [[nodiscard]] inline ::std::span<diagnostic_frame const> frames() const noexcept
        {
            if(symbols_)
            {
                ::std::lock_guard const lock{materialization_};
                if(!names_materialized_)
                {
                    ::std::vector<diagnostic_frame> snapshot{};
                    snapshot.reserve(indices_.size());
                    for(auto const index: indices_)
                    {
                        auto const names{symbols_->names(index)};
                        diagnostic_frame frame{.module_id = index.module_id, .function_index = index.function_index};
                        // [strong independently owned immutable symbol names] end
                        // [safe                                             ] copy complete
                        // nonempty views; no pointer outlives its symbol owner.
                        if(!names.module_name.empty()) { frame.module_name.assign(names.module_name.data(), names.module_name.size()); }
                        if(!names.function_name.empty()) { frame.function_name.assign(names.function_name.data(), names.function_name.size()); }
                        snapshot.push_back(::std::move(frame));
                    }
                    // Nothing is exposed until every name is owned. The instance
                    // mutex supplies publication to concurrent readers without
                    // libstdc++ call_once's initial-exec TLS GOT dependency. Allocation
                    // failure follows the existing noexcept diagnostic policy.
                    frames_.swap(snapshot);
                    names_materialized_ = true;
                }
            }
            // [owned immutable frame/name storage ...] end; publication permanently ends mutation.
            // [safe                                  ] this borrowed view lives only while its trace root does.
            return frames_;
        }
    };

    class value;
    using value_ref = ::std::shared_ptr<value const>;

    class value final
    {
        instance_root tag_;
        ::std::vector<payload_field> fields_;
        diagnostic_trace_ref diagnostic_{};
        class construction_key
        {
            friend class value;
            construction_key() noexcept = default;
        public:
            construction_key(construction_key const&) noexcept = default;
        };
    public:
        explicit inline value(construction_key, instance_root tag, ::std::span<payload_field const> fields, diagnostic_trace_ref diagnostic)
            : tag_{::std::move(tag)}, diagnostic_{::std::move(diagnostic)}
        {
            // Avoid passing an empty/null range to an implementation that computes iterator distance.
            if(!fields.empty()) { fields_.assign(fields.begin(), fields.end()); }
        }
        explicit inline value(construction_key, instance_root tag, ::std::vector<payload_field>&& fields,
                              diagnostic_trace_ref diagnostic) noexcept
            : tag_{::std::move(tag)}, fields_{::std::move(fields)}, diagnostic_{::std::move(diagnostic)} {}

    public:
        value(value const&) = delete;
        value& operator=(value const&) = delete;

        // Only the validated runtime can build an exception. Signature/subtype validation remains at
        // the throw instruction; this owner copies all argument bits and retains every reference root.
        // A tag token aliases its INSTANCE, never a module-local index or a shared function signature.
        // It must keep that tag alive even after the throwing frame/module execution has returned.
        [[nodiscard]] static inline value_ref make(instance_root tag, ::std::span<payload_field const> fields,
                                                    diagnostic_trace_ref diagnostic = {})
        {
            if(!tag || tag.use_count() == 0) { return {}; }
            // Construct privately before publication; allocation failure cannot expose a partial value.
            return ::std::make_shared<value>(construction_key{}, ::std::move(tag), fields, ::std::move(diagnostic));
        }
        // A validated throw builder transfers its exclusive field allocation. Before this call,
        // the caller must retire every mutable alias, element reference, pointer, iterator and span
        // into fields; std::move does not itself invalidate such aliases. The moved-from vector may
        // be reused, but the published fields and their reference roots must never be mutated.
        // [exclusive owned fields ...] end
        // [safe                      ] the private constructor moves storage and roots together;
        // no field pointer escapes before immutable publication. Invalid tags leave fields intact.
        [[nodiscard]] static inline value_ref make_owned(instance_root tag, ::std::vector<payload_field>&& fields,
                                                         diagnostic_trace_ref diagnostic = {})
        {
            if(!tag || tag.use_count() == 0) { return {}; }
            return ::std::make_shared<value>(construction_key{}, ::std::move(tag), ::std::move(fields), ::std::move(diagnostic));
        }

        [[nodiscard]] inline void const* tag_identity() const noexcept { return tag_.get(); }
        // Rethrow/throw_ref keeps the original immutable throw-site trace. Concurrent activations
        // can share this value without sharing native unwinder headers or mutable diagnostic state.
        [[nodiscard]] inline diagnostic_trace_ref const& diagnostic() const noexcept { return diagnostic_; }
        [[nodiscard]] inline ::std::span<payload_field const> fields() const noexcept
        {
            // [owned immutable fields ...] end; no resizes occur after construction/publication.
            // [safe                      ] view remains valid while this value_ref is retained.
            return fields_;
        }
    };

    // A typed C++ activation contains only an owning value reference. Each throw expression creates
    // its own ABI exception header; copies/rethrows cannot overwrite another activation's unwinder
    // state. Guest catch_all catches this exact type, not host exceptions, traps, or allocation failures.
    class guest_exception final
    {
        value_ref value_;

    public:
        explicit inline guest_exception(value_ref instance) noexcept : value_{::std::move(instance)}
        {
            // A guest null throw_ref is a trap checked by the runtime before reaching this invariant.
            if(!value_) [[unlikely]] { ::std::terminate(); }
        }
        guest_exception(guest_exception const&) noexcept = default;
        guest_exception& operator=(guest_exception const&) noexcept = default;

        // An activation remains a valid throwable object after a move. Copy the
        // immutable value root rather than making the still-accessible source
        // activation empty; native ABI exception headers remain independent.
        inline guest_exception(guest_exception&& other) noexcept : value_{other.value_} {}
        inline guest_exception& operator=(guest_exception&& other) noexcept
        {
            value_ = other.value_;
            return *this;
        }

        [[nodiscard]] inline value_ref const& instance() const noexcept { return value_; }
    };

#ifdef UWVM_CPP_EXCEPTIONS
    [[noreturn]] inline void throw_value(value_ref instance) UWVM_THROWS
    {
        throw guest_exception{::std::move(instance)};
    }
#endif
}

#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
#endif
