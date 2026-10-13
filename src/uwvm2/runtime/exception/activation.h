/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)
 * Copyright (c) 2025-present UlteSoft. All rights reserved.
 * Licensed under the APL-2.0 License (see LICENSE file).
 *************************************************************/
#pragma once
#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1
#ifndef UWVM_MODULE
# include <exception>
# include <memory>
# include <optional>
# include <utility>
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/runtime/exception/immutable_value.h>
# include <uwvm2/runtime/exception/native_roots.h>
#if defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) && UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES == 1
# include <uwvm2/runtime/exception/external_handle.h>
#endif
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) && UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES == 1
#include "activation_external_handles.h"
#else
UWVM_MODULE_EXPORT namespace uwvm2::runtime::exception
{
    // One actual C++ throw activation, one embedded owned value. C++ supplies a
    // separate native ABI exception header per throw expression. A real domain
    // pin owns ONLY the census lifetime, not source/generation/cohort authority.
    // Every unknown shared copy/public raw escape still requires the existing
    // native_roots_unknown/defined-tag gates. No runtime TLS domain is inferred.
    class guest_exception final
    {
        // Declaration order is intentional: destroy lease (detach/release)
        // before releasing its domain lifetime pin on all normal/EH paths.
        ::std::shared_ptr<native_exception_root_domain> domain_pin_{};
        immutable_exception_root_lease lease_{};
        class prepared_key
        {
            friend class guest_exception;
            prepared_key() noexcept = default;
        public:
            prepared_key(prepared_key const&) noexcept = default;
        };
        void clone_into(immutable_exception_root_lease& target) const noexcept
        {
            if(lease_)
            {
                if(!domain_pin_ || lease_.domain_identity() != domain_pin_.get() ||
                   target.clone_registered_from(lease_) != native_exception_root_status::registered)
                { ::std::terminate(); }
            }
            else
            {
                if(domain_pin_ || !lease_.instance()) { ::std::terminate(); }
                target.adopt_unregistered(lease_.instance());
            }
        }

    public:
        // Existing native fallback: strongly own the immutable value but DO NOT
        // register it or turn a complete native-root proof from false to true.
        explicit guest_exception(value_ref instance) noexcept
        { lease_.adopt_unregistered(::std::move(instance)); }

        // An inaccessible key allows std::optional in-place construction with
        // no extra heap node. Only attach_registered can prepare this input.
        explicit guest_exception(prepared_key, ::std::shared_ptr<native_exception_root_domain> domain,
                                 immutable_exception_root_lease& prepared) noexcept
            : domain_pin_{::std::move(domain)}
        {
            if(!domain_pin_ || !prepared || prepared.domain_identity() != domain_pin_.get()) { ::std::terminate(); }
            lease_.take_prepared_snapshot(prepared);
        }
        [[nodiscard]] static ::std::optional<guest_exception> attach_registered(
            ::std::shared_ptr<native_exception_root_domain> domain, value_ref&& instance) noexcept
        {
            // Real typed domain lifetime pin ONLY. Canonical actual value/native
            // provenance is a privileged host precondition, not shape/use_count
            // authority. Public NEW attachments still refuse closed domains.
            if(!native_exception_root_domain::has_canonical_owner(domain)) { return {}; }
            immutable_exception_root_lease prepared{*domain, ::std::move(instance)};
            if(!prepared) { return {}; } // Every rejection leaves instance intact.
            return ::std::optional<guest_exception>{::std::in_place, prepared_key{}, ::std::move(domain), prepared};
        }
        guest_exception(guest_exception const& other) noexcept : domain_pin_{other.domain_pin_}
        { other.clone_into(lease_); }
        // Preserve the old moved-from-valid behavior: a FRESH lease copies the
        // actual registered source under its census lock, including after close.
        guest_exception(guest_exception&& other) noexcept : guest_exception{static_cast<guest_exception const&>(other)} {}
        guest_exception& operator=(guest_exception const& other) noexcept
        {
            if(this != ::std::addressof(other))
            {
                auto incoming_pin{other.domain_pin_};
                immutable_exception_root_lease prepared{};
                other.clone_into(prepared); // Establish incoming precise root FIRST.
                lease_.reset(); // Old owner releases outside locks with old pin alive.
                domain_pin_ = ::std::move(incoming_pin);
                // Rebind the true snapshot under only its own domain mutex.
                // No two domain locks, root gap, heap node or duplicate member.
                lease_.take_prepared_snapshot(prepared);
            }
            return *this;
        }
        guest_exception& operator=(guest_exception&& other) noexcept
        { return *this = static_cast<guest_exception const&>(other); }
        ~guest_exception() noexcept = default;

        [[nodiscard]] value_ref const& instance() const noexcept { return lease_.instance(); }
        [[nodiscard]] bool root_registered() const noexcept { return static_cast<bool>(lease_); }
    };

#ifdef UWVM_CPP_EXCEPTIONS
    [[noreturn]] inline void throw_value(value_ref instance) UWVM_THROWS
    {
        // Legacy/default native path remains unregistered; actual generation
        // root-domain binding is a later privileged runtime integration.
        throw guest_exception{::std::move(instance)};
    }
#endif
}
#endif
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
#endif
