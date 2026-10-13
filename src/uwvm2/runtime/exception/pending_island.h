// Private native experiment. Not imported by production modules or JIT.
#pragma once
#ifndef UWVM_MODULE
#include <uwvm2/runtime/exception/roots.h>
#include <array>
#include <cstddef>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <span>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::runtime::exception::pending_experiment
{
#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1
    inline constexpr ::std::size_t max_payload_fields{256uz};
#else
    inline constexpr ::std::size_t max_payload_fields{64uz};
#endif
    inline constexpr ::std::size_t max_retired_frames{64uz};
    inline constexpr ::std::size_t max_tags{1024uz};
    inline constexpr ::std::size_t max_native_pins{1024uz};
    using reference = ::uwvm2::object::global::wasm_global_ref_t;
    enum class status : unsigned char
    { ok, invalid_registry, invalid_tag, invalid_signature, invalid_reference, inactive,
      wrong_thread, busy, no_pending, no_match, target_not_registered, target_busy,
      invalid_root_chain, host_codec_required, rejected_reference, size_overflow };
    enum class phase : unsigned char { empty, fresh, existing };
    struct retired_frame { ::std::size_t module_id{}, function_index{}; };

    // HOST-ONLY input. The real validator/initializer must already have proved
    // this tag instance, its signature and the native generation/store pins.
    // prepare() checks carrier shape; it is not a Wasm binary/tag validator.
    struct admitted_tag
    { instance_root identity; ::std::span<payload_kind const> signature; };

    class prepared_registry final
    {
        struct owned_tag
        {
            instance_root identity;
            ::std::array<payload_kind, max_payload_fields> signature{};
            ::std::size_t count{};
            // Cold classification of the immutable copied schema.
            bool numeric_only{true};
            ::std::size_t numeric_bytes{};
        };
        ::std::vector<owned_tag> tags_;
        ::std::vector<instance_root> generation_and_store_pins_;
        ::std::weak_ptr<prepared_registry const> canonical_owner_;
        prepared_registry() = default;
        friend class pending_context;
        // Native admission retains every field-root owner until all contexts
        // and catch scopes retire. Then leaf clear cannot run an arbitrary
        // last-owner deleter/reentry. Local GC fields have an EMPTY root;
        // their liveness still requires precise enumeration, not this pin.
        [[nodiscard]] bool owns_field_root(instance_root const& root) const noexcept
        {
            if(!root)
            {
                instance_root const empty{};
                return !root.owner_before(empty) && !empty.owner_before(root);
            }
            for(auto const& pin: generation_and_store_pins_)
            { if(!root.owner_before(pin) && !pin.owner_before(root)) { return true; } }
            for(auto const& tag: tags_)
            { if(!root.owner_before(tag.identity) && !tag.identity.owner_before(root)) { return true; } }
            return false;
        }
    public:
        using owner = ::std::shared_ptr<prepared_registry const>;
        prepared_registry(prepared_registry const&) = delete;
        prepared_registry& operator=(prepared_registry const&) = delete;
        prepared_registry(prepared_registry&&) = delete;
        prepared_registry& operator=(prepared_registry&&) = delete;

        // Cold admission allocates the table/control block/vectors and copies
        // strong pins. Mutable input aliases cannot alter the owned schema.
        [[nodiscard]] static owner prepare(::std::span<admitted_tag const> input,
            ::std::span<instance_root const> pins)
        {
            if(input.empty() || input.size() > max_tags || pins.size() > max_native_pins) { return {}; }
            for(auto const& tag: input)
            {
                if(!tag.identity || tag.identity.use_count() == 0 || tag.signature.size() > max_payload_fields) { return {}; }
                for(auto kind: tag.signature)
                {
                    // Opaque native reference/i31-pointer fields need another
                    // codec and are deliberately outside fresh publication.
                    if(kind == payload_kind::reference || payload_width(kind) == 0uz) { return {}; }
                }
            }
            for(auto const& pin: pins) { if(!pin || pin.use_count() == 0) { return {}; } }
            auto result{::std::shared_ptr<prepared_registry>{new prepared_registry{}}};
            result->tags_.reserve(input.size());
            if(!pins.empty()) { result->generation_and_store_pins_.assign(pins.begin(), pins.end()); }
            for(auto const& tag: input)
            {
                owned_tag owned{.identity = tag.identity, .count = tag.signature.size()};
                for(::std::size_t index{}; index != owned.count; ++index)
                {
                    auto const kind{tag.signature[index]};
                    owned.signature[index] = kind;
                    owned.numeric_only = owned.numeric_only &&
                        (kind == payload_kind::i32 || kind == payload_kind::i64 ||
                         kind == payload_kind::f32 || kind == payload_kind::f64 || kind == payload_kind::v128);
                    owned.numeric_bytes += payload_width(kind);
                }
                for(auto const& earlier: result->tags_)
                {
                    if(earlier.identity.get() != owned.identity.get()) { continue; }
                    if(earlier.count != owned.count) { return {}; }
                    for(::std::size_t index{}; index != owned.count; ++index)
                    { if(earlier.signature[index] != owned.signature[index]) { return {}; } }
                }
                result->tags_.push_back(::std::move(owned));
            }
            result->canonical_owner_ = result;
            return result;
        }
        [[nodiscard]] static bool canonical(owner const& candidate) noexcept
        { return candidate && !candidate->canonical_owner_.owner_before(candidate) && !candidate.owner_before(candidate->canonical_owner_); }
    };

    class execution_island;
    class native_island_chain final
    {
        execution_island* top_{};
        ::std::thread::id owner_{};
        friend class execution_island;
    public:
        native_island_chain() noexcept = default;
        native_island_chain(native_island_chain const&) = delete;
        native_island_chain& operator=(native_island_chain const&) = delete;
        ~native_island_chain() { if(top_ != nullptr) { ::std::terminate(); } }
        // Borrow only under the same owner/quiescence contract as root visit.
        // All nested native entries must use this SAME explicit thread chain.
        // The embedding VM must admit/register every such chain; not done here.
        [[nodiscard]] execution_island const* quiescent_head() const noexcept { return top_; }
    };
    class caught_root_scope;
    class caught_payload final
    {
        prepared_registry::owner registry_;
        ::std::array<payload_field, max_payload_fields> slots_{};
        ::std::size_t count_{};
        bool full_{};
        value_ref observable_{};
        execution_island* registered_island_{};
        caught_payload* previous_{};
        friend class pending_context;
        friend class caught_root_scope;
        friend class execution_island;
        void clear_private() noexcept
        {
            for(::std::size_t index{}; index != count_; ++index) { slots_[index] = payload_field{}; }
            observable_.reset();
            count_ = 0uz;
            full_ = false;
        }
    public:
        explicit caught_payload(prepared_registry::owner registry) noexcept : registry_{::std::move(registry)} {}
        caught_payload(caught_payload const&) = delete;
        caught_payload& operator=(caught_payload const&) = delete;
        ~caught_payload() { if(registered_island_ != nullptr) { ::std::terminate(); } }
        [[nodiscard]] ::std::span<payload_field const> fields() const noexcept
        { return observable_ ? observable_->fields() : ::std::span<payload_field const>{slots_.data(), count_}; }
        // Borrow only while the registered scope and this payload remain alive.
        // A native/host owner taking a copy must register its own explicit root
        // before allowing collection after this scope retires.
        [[nodiscard]] value_ref const& observable() const noexcept { return observable_; }
    };

    namespace details
    {
        // Same two-stage carrier rule as the actual immutable payload visitor.
        // This span is owned private storage and must be stable through the
        // complete quiescent visit. A known kind never proves token liveness.
        [[nodiscard]] inline status preflight(::std::span<payload_field const> fields) noexcept
        {
            for(auto const& field: fields)
            {
                auto const bits{field.bits()};
                if(payload_width(field.kind()) == 0uz || bits.size() != payload_width(field.kind())) { return status::invalid_signature; }
                if(field.kind() == payload_kind::reference)
                {
                    void const* null{};
                    if(field.root() || ::std::memcmp(bits.data(), ::std::addressof(null), sizeof(null)) != 0)
                    { return status::host_codec_required; }
                }
                else if(field.kind() == payload_kind::wasm_reference)
                {
                    reference carrier{};
                    // [complete owned carrier bytes][aligned local carrier]
                    // [safe                        ] memcpy copies the kind
                    // and payload together, never dereferences opaque tokens.
                    ::std::memcpy(::std::addressof(carrier), bits.data(), sizeof(carrier));
                    if(!payload_root_details::runtime_reference_kind(carrier)) { return status::invalid_reference; }
                }
            }
            return status::ok;
        }
        template<class Visitor>
        [[nodiscard]] inline status deliver(::std::span<payload_field const> fields, Visitor& visitor,
            ::std::size_t& visited) noexcept
        {
            for(auto const& field: fields)
            {
                if(field.kind() != payload_kind::wasm_reference) { continue; }
                if(visited == (::std::numeric_limits<::std::size_t>::max)()) { return status::size_overflow; }
                reference carrier{};
                // [preflighted, stopped native field][aligned local carrier]
                // [safe                            ] no alias to the owned
                // field escapes to the collector visitor; it gets a VALUE.
                ::std::memcpy(::std::addressof(carrier), field.bits().data(), sizeof(carrier));
                if(!visitor(carrier)) { return status::rejected_reference; }
                ++visited;
            }
            return status::ok;
        }
    }

    // PRIVATE child-only read view. No field/owner copy, mutation or root
    // registration occurs here. A borrow is valid only inside one admitted
    // owner-thread leaf, with no collection, callback, park, clear or escape.
    struct numeric_leaf_state
    {
        ::std::span<payload_kind const> signature{};
        ::std::span<payload_field const> fields{};
        ::std::span<retired_frame const> retired_frames{};
        bool truncated{};
#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1
        // Derived only by the real context publication/borrow, never an input
        // authorization. The owning leaf may not retain this span/count.
        ::std::size_t exact_numeric_bytes{};
        // A complete initialized byte prefix, issued only by the live context.
        // Numeric bits are opaque; this never carries references or owners.
        ::std::span<::std::byte const> packed_tuple{};
        bool packed{};
#endif
    };

    class pending_context final
    {
        prepared_registry::owner registry_;
        mutable ::std::array<payload_field, max_payload_fields> slots_{};
        ::std::array<retired_frame, max_retired_frames> retired_{};
        ::std::size_t count_{}, tag_{}, retired_count_{};
        bool truncated_{};
        phase phase_{phase::empty};
        value_ref existing_{};
        execution_island* active_{};
#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1
        // PRIVATE proof: only successful publish_fresh may admit this prefix.
        // The copied const registry plus every actual field kind/width and EMPTY
        // control block were checked BEFORE any move. No native bool/alias is
        // accepted as proof. Existing/reference publication cannot mint it.
        bool numeric_admitted_{};
        ::std::size_t numeric_exact_bytes_{};
        // Raw byte lifetimes exist at construction; only a successful complete
        // memcpy publishes their exact prefix. No default zeroing is required.
        ::std::array<::std::byte, max_payload_fields * 16uz> numeric_tuple_;
        mutable bool numeric_packed_{};
        void invalidate_numeric_admission() noexcept
        { numeric_admitted_ = false; numeric_exact_bytes_ = 0uz; numeric_packed_ = false; }
        // Cold legacy observation/materialization. The packed representation
        // has no roots; all destination owners are proved empty before writes.
        [[nodiscard]] status materialize_numeric_fields() const noexcept
        {
            if(!numeric_packed_) { return status::ok; }
            auto const access{leaf_access()};
            if(access != status::ok) { return access; }
            if(!numeric_admitted_ || phase_ != phase::fresh || tag_ >= registry_->tags_.size())
            { return status::invalid_signature; }
            auto const& tag{registry_->tags_[tag_]};
            if(!tag.numeric_only || count_ != tag.count || count_ > max_payload_fields ||
                numeric_exact_bytes_ != tag.numeric_bytes || numeric_exact_bytes_ > numeric_tuple_.size())
            { return status::invalid_signature; }
            instance_root const empty{};
            for(::std::size_t index{}; index != count_; ++index)
            {
                auto const& root{slots_[index].root()};
                if(root.owner_before(empty) || empty.owner_before(root))
                { return status::host_codec_required; }
            }
            auto const* input{numeric_tuple_.data()};
            for(::std::size_t index{}; index != count_; ++index)
            {
                auto const kind{tag.signature[index]};
                auto const width{payload_width(kind)};
                if(!slots_[index].assign_numeric_unowned(kind,{input,width})) { ::std::terminate(); }
                input += width;
            }
            numeric_packed_ = false;
            return status::ok;
        }
#endif
        friend class execution_island;
        friend class caught_root_scope;
        [[nodiscard]] status leaf_access() const noexcept;
        void clear_private() noexcept
        {
#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1
            if(numeric_admitted_ && phase_ == phase::fresh)
            {
                // ALL active fields have EMPTY root control blocks; existing_
                // is empty. No callback, ref decrement, GC or reentry is needed.
                // Keep each payload_field lifetime started; stale numeric bits
                // are private and invisible after count/phase reset. Future
                // publication overwrites its complete prefix before committing.
                invalidate_numeric_admission();
                count_ = retired_count_ = tag_ = 0uz;
                truncated_ = false;
                phase_ = phase::empty;
                return;
            }
            invalidate_numeric_admission();
#endif
            for(::std::size_t index{}; index != count_; ++index) { slots_[index] = payload_field{}; }
            existing_.reset();
            count_ = retired_count_ = tag_ = 0uz;
            truncated_ = false;
            phase_ = phase::empty;
        }
        [[nodiscard]] ::std::span<payload_field const> fields_private() const noexcept
        {
            if(phase_ == phase::existing) { return existing_->fields(); }
#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1
            // Root enumeration only: a committed packed numeric tuple has zero
            // references. Cold value consumers materialize explicitly first.
            if(numeric_packed_) { return {}; }
#endif
            return {slots_.data(), count_};
        }
    public:
        explicit pending_context(prepared_registry::owner registry) noexcept : registry_{::std::move(registry)} {}
        pending_context(pending_context const&) = delete;
        pending_context& operator=(pending_context const&) = delete;
        ~pending_context() { if(active_ != nullptr) { ::std::terminate(); } }
        // The generated owner-thread success path needs only this plain state
        // load: no allocation, lock, TLS lookup, shared/weak promotion or stack.
        // This is not a concurrent management-thread query.
        [[nodiscard]] bool has_pending() const noexcept { return phase_ != phase::empty; }
        [[nodiscard]] phase state() const noexcept { return phase_; }

        // PRIVATE child-only readonly admission. The original 16a has no
        // public registry signature accessor. This shares its copied schema,
        // never the caller's mutable admitted_tag input array.
        [[nodiscard]] status borrow_numeric_signature(::std::size_t tag_index,
            ::std::span<payload_kind const>& output, ::std::size_t* numeric_bytes = nullptr) const noexcept
        {
            output = {};
            auto const access{leaf_access()};
            if(access != status::ok) { return access; }
            if(tag_index >= registry_->tags_.size()) { return status::invalid_tag; }
            auto const& tag{registry_->tags_[tag_index]};
            if(tag.count > max_payload_fields) { return status::invalid_signature; }
            if(tag.numeric_only)
            {
                output = {tag.signature.data(), tag.count};
                if(numeric_bytes != nullptr) { *numeric_bytes = tag.numeric_bytes; }
                return status::ok;
            }
            for(::std::size_t index{}; index != tag.count; ++index)
            {
                auto const kind{tag.signature[index]};
                if(payload_width(kind) == 0uz) { return status::invalid_signature; }
                if(kind != payload_kind::i32 && kind != payload_kind::i64 && kind != payload_kind::f32 &&
                   kind != payload_kind::f64 && kind != payload_kind::v128)
                { return status::host_codec_required; }
            }
            // [registry-owned complete signature array, max 64][end]
            // [safe                                             ] tag.count
            // was checked and the admitted context retains the real registry.
            output = {tag.signature.data(), tag.count};
            return status::ok;
        }

        // PRIVATE child-only readonly pending borrow. Observable/throw_ref and
        // all reference carriers are deliberately rejected for this first ABI.
        // Caller must not retain a span through clear, retirement or reentry.
        [[nodiscard]] status borrow_numeric_state(numeric_leaf_state& output) const noexcept
        {
#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1
            output = {};
            auto const materialized{materialize_numeric_fields()};
            if(materialized != status::ok) { return materialized; }
#endif
            return borrow_numeric_state_for_bridge(output);
        }

        [[nodiscard]] status borrow_numeric_state_for_bridge(numeric_leaf_state& output) const noexcept
        {
            output = {};
            auto const access{leaf_access()};
            if(access != status::ok) { return access; }
            if(phase_ == phase::empty) { return status::no_pending; }
            if(phase_ != phase::fresh) { return status::host_codec_required; }
#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1
            if(numeric_admitted_)
            {
                // leaf_access still runs FIRST on every borrow. Only immutable
                // successful publication minted this proof. No child/reentry,
                // clear, writable borrow or registry mutation may intervene.
                if(tag_ >= registry_->tags_.size()) { return status::invalid_tag; }
                auto const& tag{registry_->tags_[tag_]};
                if(count_ != tag.count || count_ > max_payload_fields || retired_count_ > max_retired_frames)
                { return status::invalid_signature; }
                // [actual immutable registry signature][count <= 64]
                // [safe ] the real registry remains pinned by this context.
                output.signature = {tag.signature.data(), count_};
                // [constructed private slots][fully admitted numeric prefix]
                // [safe ] count bounds + the private proof cover every field;
                // no pointer/root copy or mutable alias is returned.
                if(numeric_packed_)
                {
                    if(numeric_exact_bytes_ != tag.numeric_bytes || numeric_exact_bytes_ > numeric_tuple_.size())
                    { return status::invalid_signature; }
                    output.packed = true;
                    output.packed_tuple = {numeric_tuple_.data(), numeric_exact_bytes_};
                }
                else { output.fields = {slots_.data(), count_}; }
                // [constructed diagnostic array][retired_count <= 64]
                // [safe ] IDs only; appending trace does not change payload proof.
                output.retired_frames = {retired_.data(), retired_count_};
                output.truncated = truncated_;
                output.exact_numeric_bytes = numeric_exact_bytes_;
                return status::ok;
            }
#endif
            ::std::span<payload_kind const> signature{};
            auto const admitted{borrow_numeric_signature(tag_, signature)};
            if(admitted != status::ok) { return admitted; }
            if(count_ != signature.size() || count_ > max_payload_fields || retired_count_ > max_retired_frames)
            { return status::invalid_signature; }
            instance_root const empty{};
#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1
            ::std::size_t exact_numeric_bytes{};
#endif
            for(::std::size_t index{}; index != count_; ++index)
            {
                auto const& field{slots_[index]};
                if(field.kind() != signature[index] || field.bits().size() != payload_width(signature[index]))
                { return status::invalid_signature; }
                if(field.root().owner_before(empty) || empty.owner_before(field.root()))
                { return status::host_codec_required; }
#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1
                auto const width{payload_width(signature[index])};
                if(width > (::std::numeric_limits<::std::size_t>::max)() - exact_numeric_bytes)
                { return status::size_overflow; }
                exact_numeric_bytes += width;
#endif
            }
#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1
            output.exact_numeric_bytes = exact_numeric_bytes;
#endif
            output.signature = signature;
            // [live 64-slot context array][0,count_) preflighted][end]
            // [safe                                           ] no field/root
            // copy and no writable alias is returned. Context remains active.
            output.fields = {slots_.data(), count_};
            // [live 64-entry diagnostic array][0,retired_count_)[end]
            // [safe                                            ] count bound
            // was checked; these are logical IDs, never native stack pointers.
            output.retired_frames = {retired_.data(), retired_count_};
            output.truncated = truncated_;
            return status::ok;
        }

#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1
        // Only the sealed native bridge calls this after checking the complete
        // tuple extent and its disjointness from the real context/header.
        [[nodiscard]] status publish_numeric_tuple(::std::size_t tag_index,
            ::std::span<::std::byte const> tuple) noexcept
        {
            auto const access{leaf_access()};
            if(access != status::ok) { return access; }
            if(has_pending()) { return status::busy; }
            if(tag_index >= registry_->tags_.size()) { return status::invalid_tag; }
            auto const& tag{registry_->tags_[tag_index]};
            if(!tag.numeric_only) { return status::host_codec_required; }
            if(tag.count > max_payload_fields || tuple.size() != tag.numeric_bytes)
            { return status::invalid_signature; }
            instance_root const empty{};
            if(existing_.owner_before(empty) || empty.owner_before(existing_))
            { return status::host_codec_required; }
            // The destination is a private byte array, never payload_field
            // or a shared owner. Do not walk untouched legacy slots. The sealed
            // bridge checked source extent/disjointness before entering.
            if(tag.numeric_bytes > numeric_tuple_.size()) { return status::invalid_signature; }
            if(!tuple.empty()) { ::std::memcpy(numeric_tuple_.data(),tuple.data(),tuple.size()); }
            numeric_packed_ = true;
            count_ = tag.count;
            tag_ = tag_index;
            phase_ = phase::fresh;
            numeric_admitted_ = true;
            numeric_exact_bytes_ = tag.numeric_bytes;
            return status::ok;
        }
#endif

        // Leaf publication: first preflight EVERYTHING, then move the exclusive
        // fields into existing constructed slots. No failure after the first
        // move. The validator is the real tag/subtype/token codec and may not
        // collect, park, allocate, invoke guest/host code or mutate these fields.
        template<class Validator>
        [[nodiscard]] status publish_fresh(::std::size_t tag_index, ::std::span<payload_field> fields,
            Validator&& validate) noexcept
        {
            static_assert(::std::is_nothrow_invocable_r_v<bool, Validator&, ::std::size_t, ::std::size_t, reference>);
            auto const access{leaf_access()};
            if(access != status::ok) { return access; }
            if(has_pending()) { return status::busy; }
            if(tag_index >= registry_->tags_.size()) { return status::invalid_tag; }
            auto const& tag{registry_->tags_[tag_index]};
            if(fields.size() != tag.count) { return status::invalid_signature; }
#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1
            instance_root const empty{};
            // Empty phase is a private invariant; also prove EMPTY ownership,
            // not merely existing_.get()==nullptr (which permits null aliases).
            bool numeric_admitted{!existing_.owner_before(empty) && !empty.owner_before(existing_)};
            ::std::size_t exact_numeric_bytes{};
#endif
            for(::std::size_t index{}; index != fields.size(); ++index)
            {
                auto const& field{fields[index]};
                if(field.kind() != tag.signature[index] || field.bits().size() != payload_width(field.kind()))
                { return status::invalid_signature; }
                if(!registry_->owns_field_root(field.root())) { return status::invalid_reference; }
#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1
                auto const kind{field.kind()};
                auto const width{payload_width(kind)};
                bool const numeric_kind{kind == payload_kind::i32 || kind == payload_kind::i64 ||
                    kind == payload_kind::f32 || kind == payload_kind::f64 || kind == payload_kind::v128};
                // Only admit a true EMPTY control block, never a null alias or
                // a numeric-looking root. Original generic validation/status
                // order remains first and is never skipped on publication.
                if(!numeric_kind || field.root().owner_before(empty) || empty.owner_before(field.root()) ||
                   width > (::std::numeric_limits<::std::size_t>::max)() - exact_numeric_bytes)
                { numeric_admitted = false; }
                else { exact_numeric_bytes += width; }
#endif
                if(field.kind() == payload_kind::wasm_reference)
                {
                    reference carrier{};
                    // [complete native field carrier][aligned local carrier]
                    // [safe                         ] no guest pointer follows;
                    // the actual codec proves liveness/subtype before any move.
                    ::std::memcpy(::std::addressof(carrier), field.bits().data(), sizeof(carrier));
                    if(!payload_root_details::runtime_reference_kind(carrier) || !validate(tag_index, index, carrier))
                    { return status::invalid_reference; }
                }
            }
            for(::std::size_t index{}; index != fields.size(); ++index) { slots_[index] = ::std::move(fields[index]); }
            count_ = fields.size();
            tag_ = tag_index;
            phase_ = phase::fresh;
#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1
            // Commit only AFTER the complete original schema/kind/width/root
            // validation and all noexcept slot moves. Failed publication has
            // changed neither pending state, input ownership nor this proof.
            numeric_admitted_ = numeric_admitted;
            numeric_exact_bytes_ = numeric_admitted ? exact_numeric_bytes : 0uz;
#endif
            return status::ok;
        }
        [[nodiscard]] status publish_existing(value_ref&& instance) noexcept
        {
            auto const access{leaf_access()};
            if(access != status::ok) { return access; }
            if(has_pending()) { return status::busy; }
            if(!instance) { return status::invalid_reference; }
            if(instance->fields().size() > max_payload_fields) { return status::size_overflow; }
            // HOST-ONLY value_ref must genuinely own the actual immutable
            // value (production throw_ref supplies that value). shared_ptr
            // alone cannot prove an arbitrary native alias pointer's origin.
            for(auto const& field: instance->fields())
            { if(!registry_->owns_field_root(field.root())) { return status::invalid_reference; } }
#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1
            // Observable/throw_ref ownership is never a numeric cache proof.
            // Invalidate only after every original success precondition passed.
            invalidate_numeric_admission();
#endif
            existing_ = ::std::move(instance);
            phase_ = phase::existing;
            return status::ok;
        }
        [[nodiscard]] status matches(::std::size_t tag_index) const noexcept
        {
            auto const access{leaf_access()};
            if(access != status::ok) { return access; }
            if(!has_pending()) { return status::no_pending; }
            if(tag_index >= registry_->tags_.size()) { return status::invalid_tag; }
            auto const identity{phase_ == phase::fresh ? registry_->tags_[tag_].identity.get() : existing_->tag_identity()};
            return identity == registry_->tags_[tag_index].identity.get() ? status::ok : status::no_match;
        }
        [[nodiscard]] status append_exceptional_exit(retired_frame frame) noexcept
        {
            auto const access{leaf_access()};
            if(access != status::ok) { return access; }
            if(!has_pending()) { return status::no_pending; }
            // throw_ref preserves its original immutable diagnostic identity.
            if(phase_ == phase::existing) { return status::ok; }
            if(retired_count_ == retired_.size()) { truncated_ = true; return status::ok; }
            retired_[retired_count_++] = frame;
            return status::ok;
        }
        [[nodiscard]] status transfer_matching(::std::size_t tag_index, caught_payload& target) noexcept;
        [[nodiscard]] status clear() noexcept
        {
            auto const access{leaf_access()};
            if(access != status::ok) { return access; }
            clear_private();
            return status::ok;
        }
        // Cold observable-ref/native boundary. Builder may allocate/throw but
        // must not reenter, park/collect or retain this borrowed prefix. Fresh
        // value creation copies owned fields; failure leaves pending roots
        // intact until normal C++ unwinding clears the island. The target was
        // registered BEFORE entry, and receives the actual immutable owner
        // before pending is cleared. Existing moves that SAME instance without
        // invoking builder or rewriting trace. There is no unregistered return
        // value gap and no catch_ref/exn-token allocation in this prototype.
        template<class TraceBuilder>
        [[nodiscard]] status materialize_in_registered(caught_payload& target, TraceBuilder&& builder);
    };

    struct root_result { status result{status::ok}; ::std::size_t islands{}, catches{}, visited{}; };
    class execution_island final
    {
        native_island_chain& chain_;
        pending_context& context_;
        execution_island* parent_{};
        caught_payload* catches_{};
        ::std::thread::id owner_{};
        ::std::size_t children_{};
        int entered_uncaught_{};
        bool entered_{};
        status admission_{status::inactive};
        friend class pending_context;
        friend class caught_root_scope;
    public:
        // A real native execution scope, in the SAME explicit nested-entry chain.
        // NO TLS singleton. Registry and fixed slots were prepared before use.
        explicit execution_island(native_island_chain& chain, pending_context& context) noexcept
            : chain_{chain}, context_{context}, parent_{chain.top_}, owner_{::std::this_thread::get_id()}, entered_uncaught_{::std::uncaught_exceptions()}
        {
            if(!prepared_registry::canonical(context.registry_)) { admission_ = status::invalid_registry; return; }
            if(context.active_ != nullptr || context.has_pending()) { admission_ = status::busy; return; }
            if(parent_ && (!parent_->entered_ || chain.owner_ != owner_ || parent_->owner_ != owner_))
            { admission_ = status::wrong_thread; return; }
            if(parent_ && parent_->children_ != 0uz) { admission_ = status::busy; return; }
            // [inactive owner-thread context] -> [this live native scope]
            // [safe                        ] published only in this leaf scope;
            // the embedding pause protocol must publish it before any visit.
            context.active_ = this;
            if(parent_) { ++parent_->children_; }
            chain.owner_ = owner_;
            // [previous live native top] -> [new scope]->[previous top]
            // [safe                    ] real LIFO, never a guest address.
            chain.top_ = this;
            entered_ = true;
            admission_ = status::ok;
        }
        execution_island(execution_island const&) = delete;
        execution_island& operator=(execution_island const&) = delete;
        ~execution_island() noexcept
        {
            if(!entered_) { return; }
            if(owner_ != ::std::this_thread::get_id() || context_.active_ != this || chain_.top_ != this ||
               children_ != 0uz || catches_ != nullptr)
            { ::std::terminate(); }
            // Normal return may not silently erase an unhandled guest error.
            // Genuine foreign/C++ unwinding still runs this destructor normally.
            if(context_.has_pending() && ::std::uncaught_exceptions() <= entered_uncaught_) { ::std::terminate(); }
            context_.clear_private();
            // [this live scope] -> inactive; release fields before root unlink.
            // [safe          ] owner thread and no child/caught scope remain.
            context_.active_ = nullptr;
            // [this native top]->[parent live native scope or null]
            // [safe                                               ] restore LIFO.
            chain_.top_ = parent_;
            if(parent_) { --parent_->children_; }
            else { chain_.owner_ = {}; }
        }
        [[nodiscard]] status admission() const noexcept { return admission_; }

        // Explicit quiescent root contract: all native mutators/host readers
        // stopped, each supplied island/scope/field alive and frozen until
        // return, release/acquire pause publication already complete. This
        // does not discover/register VM threads or establish stop-the-world.
        // Visitor is the actual GC codec, receives BY VALUE, no allocation,
        // sweeping, retaining borrows, reentry or throws; discard failed roots.
        template<class Visitor>
        [[nodiscard]] root_result visit_quiescent_roots(Visitor&& visitor) const noexcept
        {
            static_assert(::std::is_nothrow_invocable_r_v<bool, Visitor&, reference>);
            root_result result{};
            if(chain_.top_ != this) { result.result = status::invalid_root_chain; return result; }
            for(auto const* island{this}; island != nullptr;)
            {
                if(!island->entered_ || island->context_.active_ != island || ::std::addressof(island->chain_) != ::std::addressof(chain_) ||
                   result.islands == 64uz)
                { result.result = status::invalid_root_chain; return result; }
                auto check{details::preflight(island->context_.fields_private())};
                if(check != status::ok) { result.result = check; return result; }
                for(auto const* caught{island->catches_}; caught != nullptr;)
                {
                    if(caught->registered_island_ != island || caught->fields().size() > max_payload_fields || result.catches == 128uz)
                    { result.result = status::invalid_root_chain; return result; }
                    check = details::preflight(caught->fields());
                    if(check != status::ok) { result.result = check; return result; }
                    ++result.catches;
                    // [registered stopped caught chain] all nodes are owned
                    // [safe                           ] by live native scopes.
                    // ^^ advance only to its stable previous registration.
                    caught = caught->previous_;
                }
                ++result.islands;
                // [explicit stopped native island chain] no scope can leave.
                // ^^ follow its native parent; never guess FP/SP or a guest ptr.
                island = island->parent_;
            }
            for(auto const* island{this}; island != nullptr;)
            {
                auto delivered{details::deliver(island->context_.fields_private(), visitor, result.visited)};
                if(delivered != status::ok) { result.result = delivered; return result; }
                for(auto const* caught{island->catches_}; caught != nullptr;)
                {
                    delivered = details::deliver(caught->fields(), visitor, result.visited);
                    if(delivered != status::ok) { result.result = delivered; return result; }
                    // [same preflighted native caught chain] all links frozen.
                    // ^^ repeat the proved link advance; retain no borrow.
                    caught = caught->previous_;
                }
                // [same preflighted parent chain] mutators remain stopped.
                // ^^ advance through the same proved native parent link.
                island = island->parent_;
            }
            return result;
        }
    };

    inline status pending_context::leaf_access() const noexcept
    {
        if(active_ == nullptr) { return status::inactive; }
        if(active_->owner_ != ::std::this_thread::get_id()) { return status::wrong_thread; }
        if(active_->children_ != 0uz) { return status::busy; }
        return status::ok;
    }
    inline status pending_context::transfer_matching(::std::size_t tag_index, caught_payload& target) noexcept
    {
        auto const matched{matches(tag_index)};
        if(matched != status::ok) { return matched; }
        if(target.registered_island_ != active_ || target.registry_.get() != registry_.get()) { return status::target_not_registered; }
        if(target.full_) { return status::target_busy; }
#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1
        auto const materialized{materialize_numeric_fields()};
        if(materialized != status::ok) { return materialized; }
#endif
        auto const fields{fields_private()};
        auto const& tag{registry_->tags_[tag_index]};
        if(fields.size() != tag.count || fields.size() > max_payload_fields) { return status::invalid_signature; }
        for(::std::size_t index{}; index != fields.size(); ++index)
        { if(fields[index].kind() != tag.signature[index]) { return status::invalid_signature; } }
        // Already-registered destination owns every field before pending clear.
        // No safepoint/callback/park occurs during this leaf handoff. Collection
        // may inspect only after the whole operation returns and owner parks.
        for(::std::size_t index{}; index != fields.size(); ++index)
        {
            if(phase_ == phase::fresh) { target.slots_[index] = ::std::move(slots_[index]); }
            else { target.slots_[index] = fields[index]; }
        }
        target.count_ = fields.size();
        target.full_ = true;
        clear_private();
        return status::ok;
    }

    template<class TraceBuilder>
    inline status pending_context::materialize_in_registered(caught_payload& target, TraceBuilder&& builder)
    {
        auto const access{leaf_access()};
        if(access != status::ok) { return access; }
        if(!has_pending()) { return status::no_pending; }
        if(target.registered_island_ != active_ || target.registry_.get() != registry_.get())
        { return status::target_not_registered; }
        if(target.full_) { return status::target_busy; }
        if(phase_ == phase::existing) { target.observable_ = ::std::move(existing_); }
        else
        {
#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1
            auto const materialized{materialize_numeric_fields()};
            if(materialized != status::ok) { return materialized; }
#endif
            auto trace{builder(::std::span<retired_frame const>{retired_.data(), retired_count_}, truncated_)};
            auto instance{value::make(registry_->tags_[tag_].identity, fields_private(), ::std::move(trace))};
            if(!instance) { return status::invalid_reference; }
            target.observable_ = ::std::move(instance);
        }
        target.full_ = true;
        clear_private();
        return status::ok;
    }

    class caught_root_scope final
    {
        execution_island& island_;
        caught_payload& payload_;
        status admission_{status::inactive};
        bool entered_{};
    public:
        explicit caught_root_scope(execution_island& island, caught_payload& payload) noexcept : island_{island}, payload_{payload}
        {
            if(!island.entered_ || island.context_.active_ != ::std::addressof(island))
            { admission_ = status::inactive; return; }
            auto const access{island.context_.leaf_access()};
            if(access != status::ok) { admission_ = access; return; }
            if(payload.registered_island_ || payload.full_) { admission_ = status::target_busy; return; }
            if(!prepared_registry::canonical(payload.registry_) || payload.registry_.get() != island.context_.registry_.get())
            { admission_ = status::invalid_registry; return; }
            // [this inactive payload] -> [previous live caught root]
            // [safe                 ] both records are native, owner-thread.
            payload.previous_ = island.catches_;
            // [native payload] -> [this admitted native execution scope]
            // [safe          ] scope will remain alive until LIFO retirement.
            payload.registered_island_ = ::std::addressof(island);
            // [previous caught head] -> [this payload]->[previous caught head]
            // [safe               ] registration precedes any field handoff.
            island.catches_ = ::std::addressof(payload);
            entered_ = true;
            admission_ = status::ok;
        }
        caught_root_scope(caught_root_scope const&) = delete;
        caught_root_scope& operator=(caught_root_scope const&) = delete;
        ~caught_root_scope() noexcept
        {
            if(!entered_) { return; }
            if(island_.owner_ != ::std::this_thread::get_id() || island_.children_ != 0uz || island_.catches_ != ::std::addressof(payload_))
            { ::std::terminate(); }
            payload_.clear_private();
            // [head]->[this registered payload]->[previous live payload]
            // [safe                                            ] native LIFO
            // owner retires its root only after releasing all owned fields.
            // ^^ restore the saved prior root registration, not a guest link.
            island_.catches_ = payload_.previous_;
            // [retired record] -> null; no visitor can run while unlinking.
            // [safe         ] owner thread is not parked in this leaf.
            payload_.previous_ = nullptr;
            payload_.registered_island_ = nullptr;
        }
        [[nodiscard]] status admission() const noexcept { return admission_; }
    };
}
