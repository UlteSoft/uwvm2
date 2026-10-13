/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <array>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
# include <span>
# include <utility>
# include <fast_io.h>
# include <uwvm2/utils/macro/push_macros.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
// Only the real runtime management implementation may define this bridge.
// No producer exists yet: this foundation cannot publish/restore checkpoints.
// A linkage specification attaches this forward declaration to the global
// module, rather than this utility partition. Its future real runtime definition
// must use the same extern "C++" linkage specification (C++ [module.unit]/7).
extern "C++" { namespace uwvm2::runtime::lib { class runtime_protected_asset_bridge; } }
UWVM_MODULE_EXPORT namespace uwvm2::utils::control
{
    enum class protected_asset_status : unsigned char
    {
        ok, manager_guard_unavailable, unsupported_provider, invalid_owner,
        identity_unavailable, invalid_role, private_mode_required, aliased_file,
        budget_exceeded, allocation_failure, stale_transaction, stale_asset, busy
    };
    enum class protected_asset_role : unsigned char { private_directory, unpublished_file };
    enum class protected_asset_decision : unsigned char { allow, denied, guest_directory, identity_unavailable, unsupported_provider };
    enum class protected_asset_import_kind : unsigned char { managed_wasi, no_host_access, opaque_native };
    enum class protected_asset_census_status : unsigned char
    { candidate_clean, invalid_input, budget_exceeded, opaque_native_import, guest_directory_or_preopen, asset_alias, identity_unavailable, unsupported_provider };
    struct protected_asset_identity
    {
        ::std::uintmax_t device{}, inode{};
        ::fast_io::file_type type{};
        friend constexpr bool operator==(protected_asset_identity const&, protected_asset_identity const&) noexcept = default;
    };
    // Actual retained guest FD-table owners/observers and every directory-stack
    // entry + configured preopen must supply these synchronous borrows. The
    // prospective runtime producer enumerates ALL default/group environments,
    // opens, renumber_map and inherited handles under its real manager guard.
    // This public input/report is diagnostic data, NEVER a census certificate.
    struct protected_asset_census_input
    {
        ::std::span<::fast_io::native_io_observer const> files{}, directories{};
        ::std::span<protected_asset_import_kind const> imports{};
    };
    struct protected_asset_census_report
    {
        protected_asset_census_status observation{protected_asset_census_status::invalid_input};
        protected_asset_status management{protected_asset_status::manager_guard_unavailable};
        ::std::size_t files_inspected{};
    };
    class protected_asset_registry;
    class protected_asset_token;
    namespace protected_asset_details
    {
        class entry final
        {
            friend class ::uwvm2::utils::control::protected_asset_registry;
            friend class ::uwvm2::utils::control::protected_asset_token;
            // Exactly one native_file owns this object. Issued tokens retain
            // this same entry control block through pending writes/mappings.
            ::fast_io::native_file retained{};
            protected_asset_identity identity{};
            protected_asset_role role{};
            ::std::uint64_t incarnation{};
            entry(::fast_io::native_file&& file, protected_asset_identity value,
                protected_asset_role kind, ::std::uint64_t generation) noexcept
                : retained{::std::move(file)}, identity{value}, role{kind}, incarnation{generation} {}
        public:
            // shared_ptr may destroy an already fully owned entry on allocation
            // failure. It cannot construct one or expose the retained native FD.
            ~entry() = default;
        };
    }
    class protected_asset_token final
    {
        friend class protected_asset_registry;
        friend class ::uwvm2::runtime::lib::runtime_protected_asset_bridge;
        ::std::shared_ptr<protected_asset_details::entry const> owner_{};
        ::std::uint64_t incarnation_{};
        protected_asset_token(::std::shared_ptr<protected_asset_details::entry> const& owner) noexcept
            : owner_{owner}, incarnation_{owner->incarnation} {}
    public:
        protected_asset_token() noexcept = default; // Empty never grants a file capability.
        protected_asset_token(protected_asset_token const&) noexcept = default;
        protected_asset_token& operator=(protected_asset_token const&) noexcept = default;
        protected_asset_token(protected_asset_token&&) noexcept = default;
        protected_asset_token& operator=(protected_asset_token&&) noexcept = default;
        // No FD, address, identity/UUID constructor or public native handle.
        explicit operator bool() const noexcept { return owner_ != nullptr && incarnation_ != 0u; }
    };
    class protected_asset_registry final
    {
        friend class ::uwvm2::runtime::lib::runtime_protected_asset_bridge;
        static constexpr ::std::size_t asset_limit{256u}, directory_limit{64u}, census_limit{65536u};
        ::std::array<::std::shared_ptr<protected_asset_details::entry>, asset_limit> entries_{};
        ::std::size_t count_{}, directories_{};
        ::std::uint64_t next_incarnation_{1u};
        // Comparison-only borrow of ONE actual runtime management callback.
        // Never a guest pointer, supplied boolean, public stop-ID or numeric FD.
        void const* current_transaction_{};
        protected_asset_registry() noexcept = default;
        class transaction final
        {
            friend class protected_asset_registry;
            friend class ::uwvm2::runtime::lib::runtime_protected_asset_bridge;
            protected_asset_registry* owner_{};
            // Only the actual closed-admission/host-operation-drain producer
            // can mark its complete real census. Public inspect_census cannot.
            protected_asset_census_status census_{protected_asset_census_status::invalid_input};
            explicit transaction(protected_asset_registry& owner) noexcept
            {
                // Nested/overlapping callbacks never overwrite the actual live
                // comparison-only borrow or resurrect a stale transaction.
                if(owner.current_transaction_ != nullptr) { return; }
                // owner is the genuine immovable manager-owned registry. Both
                // borrows last only through this synchronous maintenance callback.
                owner_ = __builtin_addressof(owner);
                owner.current_transaction_ = this;
            }
            transaction(transaction const&) = delete;
            transaction& operator=(transaction const&) = delete;
            transaction(transaction&&) = delete;
            ~transaction()
            {
                // Retire the comparison-only callback borrow before this stack
                // object ends. It does not reopen guest admission or publish data.
                if(owner_ != nullptr && owner_->current_transaction_ == this) { owner_->current_transaction_ = nullptr; }
            }
        };
        [[nodiscard]] bool accepts(transaction const& actual) const noexcept
        {
            return actual.owner_ == this && current_transaction_ == __builtin_addressof(actual) &&
                actual.census_ == protected_asset_census_status::candidate_clean;
        }
        [[nodiscard]] protected_asset_status retain_new_owner(transaction const& actual,
            ::fast_io::native_file&& file, protected_asset_role role, protected_asset_token& out) noexcept
        {
            out = {}; // A failed registration never leaves a partial token.
            if(!accepts(actual)) { return protected_asset_status::stale_transaction; }
            if(role != protected_asset_role::private_directory && role != protected_asset_role::unpublished_file)
            { return protected_asset_status::invalid_role; }
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
            if(file.native_handle() == -1) { return protected_asset_status::invalid_owner; }
            auto const status{::fast_io::posix_status_nothrow(::fast_io::posix_io_observer{file.native_handle()})};
            if(!status) { return protected_asset_status::identity_unavailable; }
            auto const& observed{status.value}; // Complete local status borrow; never retained as native metadata.
            bool const directory{role == protected_asset_role::private_directory};
            if(observed.type != (directory ? ::fast_io::file_type::directory : ::fast_io::file_type::regular))
            { return protected_asset_status::invalid_owner; }
            if((static_cast<unsigned>(observed.perm) & 07777u) != (directory ? 0700u : 0600u))
            { return protected_asset_status::private_mode_required; }
            // The producer itself must create O_EXCL/0600 at its retained
            // directory, before any content. Metadata alone is NOT creation proof.
            if(!directory && (observed.nlink != 1u || observed.size != 0u))
            { return protected_asset_status::aliased_file; }
            if(count_ == asset_limit || (directory && directories_ == directory_limit) || next_incarnation_ == 0u)
            { return protected_asset_status::budget_exceeded; }
            protected_asset_identity const identity{observed.dev, observed.ino, observed.type};
            for(::std::size_t index{}; index != count_; ++index)
            {
                // [manager-owned bounded entry array] index<count_<=256;
                // every initialized cell pins its complete immutable entry.
                if(entries_[index]->identity == identity) { return protected_asset_status::aliased_file; }
            }
# ifdef UWVM_CPP_EXCEPTIONS
            try
            {
                // Only this private registry can invoke the entry constructor.
                // If control-block allocation fails, shared_ptr deletes its fully
                // owned entry (and closes the moved file); no partial token escapes.
                ::std::shared_ptr<protected_asset_details::entry> fresh{
                    new protected_asset_details::entry{::std::move(file), identity, role, next_incarnation_}};
                // Publishing a fully constructed cell transfers exactly one
                // native owner, before its private token is issued. No guest
                // may inspect/mutate this array during the real closed guard.
                entries_[count_] = ::std::move(fresh);
                out = protected_asset_token{entries_[count_]};
                ++count_;
                if(directory) { ++directories_; }
                next_incarnation_ = next_incarnation_ == (::std::numeric_limits<::std::uint64_t>::max)() ? 0u : next_incarnation_ + 1u;
                return protected_asset_status::ok;
            }
            catch(...) { return protected_asset_status::allocation_failure; }
# else
            // No-EH allocation/provider qualification is deliberately absent.
            return protected_asset_status::unsupported_provider;
# endif
#else
            static_cast<void>(file); return protected_asset_status::unsupported_provider;
#endif
        }
        [[nodiscard]] protected_asset_status retire(transaction const& actual, protected_asset_token& token) noexcept
        {
            if(!accepts(actual)) { return protected_asset_status::stale_transaction; }
            for(::std::size_t index{}; index != count_; ++index)
            {
                // Compare registered address + control block before borrowing
                // any supplied owner contents; a foreign alias is never read.
                auto& entry{entries_[index]};
                if(entry.get() != token.owner_.get() || entry.owner_before(token.owner_) || token.owner_.owner_before(entry)) { continue; }
                if(entry->incarnation != token.incarnation_) { return protected_asset_status::stale_asset; }
                // The argument token itself is one pin. Any additional token,
                // writer or mapping must release before its identity retires.
                if(entry.use_count() != 2) { return protected_asset_status::busy; }
                bool const directory{entry->role == protected_asset_role::private_directory};
                // First slice keeps every private directory until all retained
                // unpublished file entries retire; no pathname/parent guess.
                if(directory && count_ != directories_) { return protected_asset_status::busy; }
                if(directory) { --directories_; }
                // Consume the FINAL token before dropping its registered identity.
                // No unregistered live FD or pending writer/mapping pin may survive
                // the closed manager guard. The last entry reset closes its owner.
                token.owner_.reset();
                token.incarnation_ = 0u;
                entry.reset();
                --count_;
                if(index != count_) { entry = ::std::move(entries_[count_]); }
                return protected_asset_status::ok;
            }
            return protected_asset_status::stale_asset;
        }
    public:
        protected_asset_registry(protected_asset_registry const&) = delete;
        protected_asset_registry& operator=(protected_asset_registry const&) = delete;
        protected_asset_registry(protected_asset_registry&&) = delete;
        ~protected_asset_registry() = default; // Runtime producer must drain guests before dropping the registry.
        [[nodiscard]] protected_asset_decision inspect_guest_file(::fast_io::native_io_observer file) const noexcept
        {
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
            auto const status{::fast_io::posix_status_nothrow(::fast_io::posix_io_observer{file.native_handle()})};
            if(!status) { return protected_asset_decision::identity_unavailable; }
            // Observe the actual provider type even when the caller classified
            // this FD as a file: an omitted/misclassified directory cannot become
            // a clean census. First slice has no proven ancestor/preopen policy.
            if(status.value.type == ::fast_io::file_type::directory)
            { return protected_asset_decision::guest_directory; }
            protected_asset_identity const actual{status.value.dev, status.value.ino, status.value.type};
            for(::std::size_t index{}; index != count_; ++index)
            {
                // Registry construction/mutation/retirement all require a real
                // guest-drain boundary. The live immutable native owner pins this
                // identity; the bounded borrow cannot survive the synchronous call.
                if(entries_[index]->identity == actual) { return protected_asset_decision::denied; }
            }
            return protected_asset_decision::allow;
#else
            static_cast<void>(file); return protected_asset_decision::unsupported_provider;
#endif
        }
        [[nodiscard]] protected_asset_census_report inspect_census(protected_asset_census_input input) const noexcept
        {
            // This report ALWAYS retains manager_guard_unavailable: read-only
            // public observations do not mint a transaction or publish permission.
            if(input.files.size() > census_limit || input.directories.size() > census_limit - input.files.size() ||
               input.imports.size() > census_limit) { return {protected_asset_census_status::budget_exceeded}; }
            for(auto const provider : input.imports)
            {
                if(provider != protected_asset_import_kind::managed_wasi && provider != protected_asset_import_kind::no_host_access)
                { return {protected_asset_census_status::opaque_native_import}; }
            }
            // First slice declines EVERY guest directory/preopen, including
            // ancestor/broad preopens. It does not guess path prefix/ancestry or
            // permit later hardlink/rename access through an unproven directory.
            if(!input.directories.empty()) { return {protected_asset_census_status::guest_directory_or_preopen}; }
            ::std::size_t visited{};
            for(auto const file : input.files)
            {
                auto const decision{inspect_guest_file(file)};
                if(decision != protected_asset_decision::allow)
                {
                    auto const status{decision == protected_asset_decision::denied ? protected_asset_census_status::asset_alias :
                        decision == protected_asset_decision::guest_directory ? protected_asset_census_status::guest_directory_or_preopen :
                        decision == protected_asset_decision::unsupported_provider ? protected_asset_census_status::unsupported_provider :
                        protected_asset_census_status::identity_unavailable};
                    return {status, protected_asset_status::manager_guard_unavailable, visited};
                }
                ++visited;
            }
            return {protected_asset_census_status::candidate_clean, protected_asset_status::manager_guard_unavailable, visited};
        }
    };
}
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
