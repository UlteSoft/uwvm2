// Private, owned mount provenance. This identity never enters portable wire
// data and grants no authority beyond an actual target FD under the host gate.
#pragma once
#ifndef UWVM_MODULE
#include <uwvm2/imported/wasi/wasip1/fd_manager/impl.h>
#include <cstdint>
#include <map>
#include <optional>
#include <tuple>
#include <vector>
#endif
namespace uwvm2::runtime::lib::wasip1_mount_identity
{
    namespace fm=::uwvm2::imported::wasi::wasip1::fd_manager;
    // One immutable target table under the genuine closed host gate. Cached
    // observations expire with this index; no pointer or cache is authority.
    struct descriptor_index
    {
        using fd=fm::wasi_fd_t;
        fd* cell{};bool ambiguous{};
        ::std::vector<fd*> aliases{};
        ::std::map<::std::pair<::std::uint64_t,::std::uint64_t>,fd*> choices{};
        ::std::map<::std::tuple<::std::uint64_t,::std::uint64_t,::std::uint16_t>,fd*> flag_choices{};
        ::std::map<fd const*,::std::optional<::std::uint16_t>> observed_flags{};
        [[nodiscard]] static bool sufficient(fd const* candidate,::std::uint64_t base,::std::uint64_t inheriting) noexcept
        {
            return candidate!=nullptr && (base & ~static_cast<::std::uint64_t>(candidate->rights_base))==0u &&
                (inheriting & ~static_cast<::std::uint64_t>(candidate->rights_inherit))==0u;
        }
        [[nodiscard]] fd* select(::std::uint64_t base,::std::uint64_t inheriting)
        {
            if(sufficient(cell,base,inheriting)) { return cell; }
            auto key=::std::pair{base,inheriting};auto found=choices.find(key);
            if(found!=choices.end()) { return found->second; }
            fd* chosen{};for(auto candidate:aliases)
            { if(sufficient(candidate,base,inheriting)) { chosen=candidate;break; } }
            choices.emplace(key,chosen);return chosen;
        }
        template<typename Observe>
        [[nodiscard]] fd* select_flags(::std::uint64_t base,::std::uint64_t inheriting,::std::uint16_t wanted,Observe observe)
        {
            auto key=::std::tuple{base,inheriting,wanted};auto found=flag_choices.find(key);
            if(found!=flag_choices.end()) { return found->second; }
            auto matches=[&](fd* candidate)
            {
                if(!sufficient(candidate,base,inheriting)) { return false; }
                auto [state,inserted]=observed_flags.try_emplace(candidate);
                if(inserted) { state->second=observe(candidate); }
                if(!state->second) { return false; }
                auto actual=*state->second;
                using rights=::uwvm2::imported::wasi::wasip1::abi::rights_t;
                return actual==wanted || (((actual^wanted)&26u)==0u &&
                    (candidate->rights_base&rights::right_fd_fdstat_set_flags)==rights::right_fd_fdstat_set_flags);
            };
            fd* chosen{};
            if(matches(cell)) { chosen=cell; }
            else { for(auto candidate:aliases) { if(matches(candidate)) { chosen=candidate;break; } } }
            flag_choices.emplace(key,chosen);return chosen;
        }
    };
    [[nodiscard]] inline fm::dir_stack_entry_ref_t const* root(fm::wasi_fd_ref_t const& value) noexcept
    {
        if(value.ptr==nullptr || value.ptr->wasi_fd_storage.type!=fm::wasi_fd_type_e::dir) { return nullptr; }
        auto const& chain=value.ptr->wasi_fd_storage.storage.dir_stack.dir_stack;
        if(chain.empty() || chain.front_unchecked().ptr==nullptr) { return nullptr; }
        if(value.ptr->wasi_fd_storage.storage.dir_stack.checkpoint_mount_origin) { return ::std::addressof(*value.ptr->wasi_fd_storage.storage.dir_stack.checkpoint_mount_origin); }
        return ::std::addressof(chain.front_unchecked());
    }
    [[nodiscard]] inline bool same(fm::wasi_fd_ref_t const& first,fm::wasi_fd_ref_t const& second) noexcept
    {
        auto a=root(first),b=root(second);
        return a!=nullptr && b!=nullptr && a->ptr!=nullptr && a->ptr==b->ptr;
    }
    inline void provenance(fm::wasi_fd_ref_t& replacement,fm::wasi_fd_ref_t const& target,bool follow)
    {
        auto owner=root(target);
        if(owner==nullptr) { ::fast_io::fast_terminate(); }
        replacement.ptr->wasi_fd_storage.storage.dir_stack.checkpoint_mount_origin=*owner;
        replacement.ptr->checkpoint_follow=follow;
    }
    [[nodiscard]] inline fm::wasi_fd_ref_t copy_root(fm::wasi_fd_ref_t const& target,bool follow)
    {
        fm::wasi_fd_ref_t replacement{};
        replacement.ptr->wasi_fd_storage=fm::wasi_fd_storage_t{fm::wasi_fd_type_e::dir};
        replacement.ptr->wasi_fd_storage.storage.dir_stack=target.ptr->wasi_fd_storage.storage.dir_stack;
        provenance(replacement,target,follow);
        return replacement;
    }
}
