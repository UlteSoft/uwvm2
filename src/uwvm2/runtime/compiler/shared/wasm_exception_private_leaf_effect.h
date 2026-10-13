/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <limits>
# include <memory>
# include <optional>
# include <span>
# include <utility>
# include <vector>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

// PRIVATE effect observations, not a validator or an executable certificate.
// Every event is supplied AFTER the real fused decoder validates that opcode
// and its operands. No Wasm bytes, raw guest pointers, IR or machine addresses
// are consumed here. A complete observation cannot replace actual source,
// full-validation epoch, LLVM function/ABI or loaded-range authentication.
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::shared::wasm_exception_private_leaf_effect
{
    inline constexpr auto unknown_function{(::std::numeric_limits<::std::size_t>::max)()};
    inline constexpr auto native_extent_limit{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())};

    // An observation domain scopes this one fused traversal's records. It is
    // not a generation, initialized module, phase permit or runtime source pin.
    class observation_domain final : public ::std::enable_shared_from_this<observation_domain>
    {
        ::std::size_t module_index_{}, function_count_{};
        class construction_key
        {
            friend class observation_domain;
            construction_key() noexcept = default;
        public:
            construction_key(construction_key const&) noexcept = default;
        };
    public:
        using owner = ::std::shared_ptr<observation_domain const>;
        observation_domain(observation_domain const&) = delete;
        observation_domain& operator=(observation_domain const&) = delete;
        explicit observation_domain(construction_key, ::std::size_t module, ::std::size_t functions) noexcept
            : module_index_{module}, function_count_{functions} {}
        [[nodiscard]] static owner create(::std::size_t module, ::std::size_t functions)
        {
            if(module == unknown_function || functions == 0uz || functions > native_extent_limit) { return {}; }
            return ::std::make_shared<observation_domain>(construction_key{}, module, functions);
        }
        [[nodiscard]] static bool has_actual_observation_owner(owner const& input) noexcept
        {
            // [typed live native observation object][its genuine control block]
            // [safe                                                          ]
            // The host supplies a real C++ object, never a guest integer cast.
            // Empty/nonowning aliases cannot retain it. This checks only the
            // observation's own lifetime, not VM source/validation authority.
            if(!input || input.use_count() == 0) { return false; }
            auto const canonical{input->weak_from_this()};
            return !canonical.owner_before(input) && !input.owner_before(canonical);
        }
        [[nodiscard]] ::std::size_t module_index() const noexcept { return module_index_; }
        [[nodiscard]] ::std::size_t function_count() const noexcept { return function_count_; }
    };

    class linked_tag_identity final
    {
        // This owns only the already resolved native tag INSTANCE lifetime.
        // It is not a module/source/collector permission or a type signature.
        ::std::shared_ptr<void const> instance_{};
        explicit linked_tag_identity(::std::shared_ptr<void const> instance) noexcept : instance_{::std::move(instance)} {}
    public:
        linked_tag_identity() noexcept = default;
        // HOST-ONLY: the real initialized linked tag's exception_identity is
        // supplied by the trusted fused adapter after actual record membership
        // checks. This analysis cannot authenticate an arbitrary host alias;
        // nonempty ownership is a lifetime precondition, never that proof.
        [[nodiscard]] static ::std::optional<linked_tag_identity> observe_actual_linked_instance(
            ::std::shared_ptr<void const> instance) noexcept
        {
            if(!instance || instance.use_count() == 0) { return {}; }
            return linked_tag_identity{::std::move(instance)};
        }
        [[nodiscard]] bool present() const noexcept { return static_cast<bool>(instance_); }
        [[nodiscard]] bool same_instance(linked_tag_identity const& other) const noexcept
        {
            // [two retained actual tag instances][opaque identity addresses]
            // [safe                                                       ]
            // Compare identity only; never dereference, advance or infer a
            // signature from this pointer. Live roots prevent address reuse.
            return instance_ && other.instance_ && instance_.get() == other.instance_.get();
        }
    };

    enum class catch_form : unsigned char { tagged, tagged_reference, all, all_reference, unknown };
    struct handler_observation
    {
        catch_form form{catch_form::unknown};
        linked_tag_identity tag{}; // Empty exactly for the two catch_all forms.
    };
    enum class handler_match : unsigned char { invalid, absent, consuming, retaining };

    [[nodiscard]] inline bool handlers_well_formed(::std::span<handler_observation const> handlers) noexcept
    {
        if(handlers.size() > native_extent_limit / sizeof(handler_observation)) { return false; }
        for(auto const& handler: handlers)
        {
            // [trusted owned native clause array ...] end
            // [safe                                 ] range-for borrows live
            // elements only for this call; no pointer into it is retained.
            switch(handler.form)
            {
                case catch_form::tagged: case catch_form::tagged_reference:
                    if(!handler.tag.present()) { return false; }
                    break;
                case catch_form::all: case catch_form::all_reference:
                    if(handler.tag.present()) { return false; }
                    break;
                default: return false;
            }
        }
        return true;
    }

    // Input is ALREADY ordered inner-to-outer and clause-first-to-last by the
    // validated lexical control stack. This function never sorts by tag index.
    [[nodiscard]] inline handler_match first_actual_handler(linked_tag_identity const& tag,
        ::std::span<handler_observation const> handlers) noexcept
    {
        // Validate the complete observation list first: an invalid later input
        // is not concealed by an earlier apparently matching handler.
        if(!tag.present() || !handlers_well_formed(handlers)) { return handler_match::invalid; }
        for(auto const& handler: handlers)
        {
            // [unchanged owned ordered clauses ...] end
            // [safe                               ] no callback, mutation or
            // unlock occurs between preflight and this second native borrow.
            auto const all{handler.form == catch_form::all || handler.form == catch_form::all_reference};
            if(!all && !tag.same_instance(handler.tag)) { continue; }
            return handler.form == catch_form::tagged_reference || handler.form == catch_form::all_reference ?
                handler_match::retaining : handler_match::consuming;
        }
        return handler_match::absent;
    }

    struct validated_byte_range { ::std::size_t begin{}, end{}; };
    enum class signature_class : unsigned char { numeric, reference_or_vector, unknown };
    enum class payload_class : unsigned char { numeric, reference_or_vector, unknown };
    enum class disqualifying_effect : unsigned char
    { reference, retaining_handler, throw_reference, tail_transfer, memory_or_table_or_global, host_or_unknown, unknown_opcode };
    enum class call_route : unsigned char { ordinary_local_direct, tail, indirect_or_reference, imported_or_host, unknown };

    class function_recorder;
    class function_observation;
    enum class selection : unsigned char
    {
        observed_consumed, incomplete, different_domain, wrong_target, unsupported_leaf,
        no_escaping_tag, invalid_handlers, unconsumed_tag, retaining_handler
    };
    [[nodiscard]] selection observe_consumed_direct_call(function_observation const& caller,
        ::std::size_t callsite_index, function_observation const& callee) noexcept;

    class function_observation final
    {
        friend class function_recorder;
        friend selection observe_consumed_direct_call(function_observation const&, ::std::size_t,
            function_observation const&) noexcept;
        struct direct_call
        {
            ::std::size_t target{unknown_function};
            ::std::vector<handler_observation> handlers{};
        };
        observation_domain::owner domain_{};
        ::std::size_t function_index_{unknown_function};
        bool eligible_leaf_{};
        ::std::vector<linked_tag_identity> escaping_{};
        ::std::vector<direct_call> calls_{};
        function_observation(observation_domain::owner domain, ::std::size_t function_index, bool eligible,
            ::std::vector<linked_tag_identity>&& escaping, ::std::vector<direct_call>&& calls) noexcept
            : domain_{::std::move(domain)}, function_index_{function_index}, eligible_leaf_{eligible},
              escaping_{::std::move(escaping)}, calls_{::std::move(calls)} {}
    public:
        function_observation(function_observation const&) = delete;
        function_observation& operator=(function_observation const&) = delete;
        function_observation(function_observation&&) noexcept = default;
        function_observation& operator=(function_observation&&) noexcept = default;
        // Completion means adjacent, validated-event coverage ending at the
        // observed outer function-end. It does NOT mean canonical validation.
        [[nodiscard]] bool event_stream_complete() const noexcept
        {
            // [retained genuine observation domain][immutable count]
            // [safe                                               ] the owner
            // survives the borrow; a moved-from record has an empty owner.
            return domain_ && function_index_ < domain_->function_count();
        }
        [[nodiscard]] bool numeric_leaf_observed() const noexcept
        { return event_stream_complete() && eligible_leaf_; }
        [[nodiscard]] ::std::size_t function_index() const noexcept { return function_index_; }
        [[nodiscard]] ::std::size_t escaping_tag_count() const noexcept { return escaping_.size(); }
        [[nodiscard]] ::std::size_t direct_callsite_count() const noexcept { return calls_.size(); }
    };

    class function_recorder final
    {
        observation_domain::owner domain_{};
        ::std::size_t function_index_{unknown_function}, expression_size_{}, cursor_{};
        bool stream_valid_{true}, eligible_leaf_{}, outer_end_{}, sealed_{};
        ::std::vector<linked_tag_identity> escaping_{};
        ::std::vector<function_observation::direct_call> calls_{};

        function_recorder(observation_domain::owner domain, ::std::size_t function_index,
            ::std::size_t expression_size, signature_class signature) noexcept
            : domain_{::std::move(domain)}, function_index_{function_index}, expression_size_{expression_size},
              eligible_leaf_{signature == signature_class::numeric} {}
        [[nodiscard]] bool advance(validated_byte_range range) noexcept
        {
            // [already observed: 0..cursor_][next event: begin..end][..size]
            // [safe                       ] no body bytes are dereferenced.
            //                                ^^ begin must equal cursor_;
            //                                  end must advance within size.
            if(!stream_valid_ || sealed_ || outer_end_ || !domain_ ||
               range.begin != cursor_ || range.end <= range.begin || range.end > expression_size_)
            { stream_valid_ = false; eligible_leaf_ = false; return false; }
            // [0..old cursor_][exact validated event consumed][..size]
            // [safe                                                ]
            //                 ^^ cursor_ moves only to checked range.end.
            cursor_ = range.end;
            return true;
        }
        void invalidate() noexcept { stream_valid_ = false; eligible_leaf_ = false; }
        void note_escaping(linked_tag_identity const& tag)
        {
            for(auto const& prior: escaping_)
            {
                // [owned retained tag observations ...] end
                // [safe                              ] no vector mutation
                // occurs during a borrow; equal INSTANCE aliases deduplicate.
                if(prior.same_instance(tag)) { return; }
            }
            if(escaping_.size() >= native_extent_limit / sizeof(linked_tag_identity)) { invalidate(); return; }
            // Advancing this event happened before allocation. If its owner
            // copy cannot be recorded, even a host which catches the exception
            // must never seal a summary missing this escaping instance.
            try { escaping_.push_back(tag); } // Copy an owner, not a borrowed record/span.
            catch(...) { invalidate(); throw; }
        }
    public:
        function_recorder(function_recorder const&) = delete;
        function_recorder& operator=(function_recorder const&) = delete;
        function_recorder(function_recorder&&) noexcept = default;
        function_recorder& operator=(function_recorder&&) noexcept = default;
        [[nodiscard]] static ::std::optional<function_recorder> begin(observation_domain::owner domain,
            ::std::size_t function_index, ::std::size_t expression_size, signature_class signature) noexcept
        {
            // [canonical observation lifetime][its immutable index extent]
            // [safe                                                       ]
            // Bounds are checked before borrowing fields. No VM module or
            // source admission is inferred from this analysis-only owner.
            if(!observation_domain::has_actual_observation_owner(domain) ||
               function_index >= domain->function_count() || expression_size == 0uz ||
               expression_size > native_extent_limit) { return {}; }
            return function_recorder{::std::move(domain), function_index, expression_size, signature};
        }
        [[nodiscard]] bool observe_scalar_or_control(validated_byte_range range) noexcept { return advance(range); }
        [[nodiscard]] bool observe_disqualifying(validated_byte_range range, disqualifying_effect) noexcept
        {
            if(!advance(range)) { return false; }
            eligible_leaf_ = false;
            return true;
        }
        [[nodiscard]] bool observe_handlers(validated_byte_range range,
            ::std::span<handler_observation const> handlers) noexcept
        {
            if(!advance(range) || !handlers_well_formed(handlers)) { invalidate(); return false; }
            for(auto const& handler: handlers)
            {
                // [actual validated try_table clauses ...] end
                // [safe                                  ] only this call
                // borrows them; a ref handler declines leaf specialization.
                if(handler.form == catch_form::tagged_reference || handler.form == catch_form::all_reference)
                { eligible_leaf_ = false; }
            }
            return true;
        }
        [[nodiscard]] bool observe_throw(validated_byte_range range, linked_tag_identity const& tag,
            payload_class payload, ::std::span<handler_observation const> ordered_local_handlers)
        {
            if(!advance(range)) { return false; }
            auto const matched{first_actual_handler(tag, ordered_local_handlers)};
            if(matched == handler_match::invalid) { invalidate(); return false; }
            if(payload != payload_class::numeric || matched == handler_match::retaining) { eligible_leaf_ = false; }
            // Existing same-function non-ref lowering already consumes this
            // instance. Its tag must not be counted as an escaping effect.
            if(matched != handler_match::consuming) { note_escaping(tag); }
            return stream_valid_;
        }
        [[nodiscard]] bool observe_call(validated_byte_range range, call_route route,
            ::std::size_t target, ::std::span<handler_observation const> ordered_handlers)
        {
            if(!advance(range) || !handlers_well_formed(ordered_handlers)) { invalidate(); return false; }
            // Any outgoing call disqualifies this function AS A LEAF; an
            // otherwise complete caller can still have a proved private edge.
            eligible_leaf_ = false;
            if(route != call_route::ordinary_local_direct || target == unknown_function) { return true; }
            // [recorder's retained domain][immutable function extent]
            // [safe                                                ] advance
            // has checked this owner; it remains owned across this field borrow.
            if(target >= domain_->function_count() || calls_.size() >= native_extent_limit / sizeof(function_observation::direct_call))
            { invalidate(); return false; }
            try
            {
                function_observation::direct_call site{};
                site.target = target;
                site.handlers.reserve(ordered_handlers.size());
                for(auto const& handler: ordered_handlers)
                {
                    // [validated owned clause span][new site-owned vector]
                    // [safe                                             ] copy
                    // roots/order without retaining any caller vector pointer.
                    site.handlers.push_back(handler);
                }
                calls_.push_back(::std::move(site));
            }
            catch(...) { invalidate(); throw; } // No partially copied callsite can be sealed.
            return true;
        }
        [[nodiscard]] bool observe_outer_function_end(validated_byte_range range) noexcept
        {
            if(!advance(range)) { return false; }
            if(cursor_ != expression_size_) { invalidate(); return false; }
            outer_end_ = true;
            return true;
        }
        // Called only after the real fused validator/emitter returns success.
        // This seals an EVENT observation, never an execution/publication proof.
        [[nodiscard]] ::std::optional<function_observation> seal_observation() noexcept
        {
            if(!stream_valid_ || sealed_ || !outer_end_ || cursor_ != expression_size_ || !domain_) { return {}; }
            sealed_ = true;
            // [private vectors and domain owner][new immutable observation]
            // [safe                                                        ]
            // No element references/spans escape any observer; move storage
            // only after closing this recorder. Moved-from state cannot seal.
            return function_observation{::std::move(domain_), function_index_, eligible_leaf_,
                ::std::move(escaping_), ::std::move(calls_)};
        }
    };

    [[nodiscard]] inline selection observe_consumed_direct_call(function_observation const& caller,
        ::std::size_t callsite_index, function_observation const& callee) noexcept
    {
        if(!caller.event_stream_complete() || !callee.event_stream_complete()) { return selection::incomplete; }
        // [two genuine observation owners][exact traversal identity]
        // [safe                                                 ] compare
        // control block plus object identity, not only module/index numbers.
        if(caller.domain_.get() != callee.domain_.get() || caller.domain_.owner_before(callee.domain_) ||
           callee.domain_.owner_before(caller.domain_)) { return selection::different_domain; }
        if(callsite_index >= caller.calls_.size()) { return selection::wrong_target; }
        // [owned callsites: size][callsite_index < size] end
        // [safe                                            ] no mutation,
        // callback or unlock occurs while borrowing this selected element.
        auto const& site{caller.calls_[callsite_index]};
        if(site.target != callee.function_index_) { return selection::wrong_target; }
        if(!callee.eligible_leaf_) { return selection::unsupported_leaf; }
        if(callee.escaping_.empty()) { return selection::no_escaping_tag; }
        for(auto const& tag: callee.escaping_)
        {
            // [callee-owned retained actual escaping instances ...] end
            // [safe                                                  ] test
            // EVERY instance against the site's original clause order.
            switch(first_actual_handler(tag, site.handlers))
            {
                case handler_match::consuming: break;
                case handler_match::retaining: return selection::retaining_handler;
                case handler_match::absent: return selection::unconsumed_tag;
                default: return selection::invalid_handlers;
            }
        }
        // Analysis result only. No executable address, canonical validator
        // admission, runtime/source lifetime pin or native permission is issued.
        return selection::observed_consumed;
    }
}
