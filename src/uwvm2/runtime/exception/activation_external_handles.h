// Actual guest activation owns PUBLIC alias, whose one external block owns
// the real internal-record lease. No duplicated domain/record/member owner.
// Unregistered public factory values remain UNKNOWN; only a genuine materialized
// alias shares this registration. No runtime producer/cohort authority is minted.
UWVM_MODULE_EXPORT namespace uwvm2::runtime::exception
{
    class guest_exception final
    {
        value_ref public_value_{};
        bool registered_{};
        class prepared_key
        {
            friend class guest_exception;
            prepared_key() noexcept = default;
        public:
            prepared_key(prepared_key const&) noexcept = default;
        };
    public:
        explicit guest_exception(value_ref instance) noexcept
            : public_value_{::std::move(instance)}
        {
            if(!public_value_) { ::std::terminate(); }
            // Genuine factory-owned internal value is the UNKNOWN native
            // fallback, recognized O(1) by its real weak origin/control block.
            // External/unknown native inputs still require actual registry proof.
            registered_ = !value::record_from_native(public_value_) &&
                external_exception_handle::is_registered(public_value_);
        }
        explicit guest_exception(prepared_key, value_ref prepared) noexcept
            : public_value_{::std::move(prepared)}, registered_{true}
        {
            // prepared_key is private to this class. Its SINGLE publisher is
            // attach_registered below, ONLY after materialize has attached and
            // linked the actual external block. No repeated global scan; a bool
            // or nonempty public owner is not used to manufacture this key.
            if(!public_value_) { ::std::terminate(); }
        }
        [[nodiscard]] static ::std::optional<guest_exception> attach_registered(
            ::std::shared_ptr<native_exception_root_domain> domain, value_ref&& instance) noexcept
        {
            // Before noexcept instance() can expose any public copy, allocate
            // and attach the true external block. Input remains intact on ANY
            // failure. Actual production domain/source mint stays separate.
#ifdef UWVM_CPP_EXCEPTIONS
            try
            {
#endif
                auto prepared{external_exception_handle::materialize(::std::move(domain), instance)};
                if(!prepared) { return {}; }
                ::std::optional<guest_exception> result{::std::in_place, prepared_key{}, ::std::move(prepared)};
                instance.reset(); // Outside all registry/root locks, after root exists.
                return result;
#ifdef UWVM_CPP_EXCEPTIONS
            }
            catch(...) { return {}; } // Real allocation failure: no partial alias.
#endif
        }
        guest_exception(guest_exception const& other) noexcept
            : public_value_{other.public_value_}, registered_{other.registered_} {}
        // Old contract: move leaves source valid and shares immutable identity;
        // every throw expression still receives its own C++ EH ABI header.
        guest_exception(guest_exception&& other) noexcept
            : guest_exception{static_cast<guest_exception const&>(other)} {}
        guest_exception& operator=(guest_exception const& other) noexcept
        {
            if(this != ::std::addressof(other))
            {
                value_ref incoming{other.public_value_};
                public_value_.swap(incoming);
                registered_ = other.registered_;
                // Last old owner detaches AFTER coherent assignment and outside
                // root/registry locks; old domain/source lives in old block.
            }
            return *this;
        }
        guest_exception& operator=(guest_exception&& other) noexcept
        { return *this = static_cast<guest_exception const&>(other); }
        ~guest_exception() noexcept = default;
        [[nodiscard]] value_ref const& instance() const noexcept { return public_value_; }
        [[nodiscard]] bool root_registered() const noexcept { return registered_; }
    };
#ifdef UWVM_CPP_EXCEPTIONS
    [[noreturn]] inline void throw_value(value_ref instance) UWVM_THROWS
    { throw guest_exception{::std::move(instance)}; }
#endif
}
