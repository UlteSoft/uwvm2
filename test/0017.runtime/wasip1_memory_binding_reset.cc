#include <array>
#include <fast_io.h>
#include <uwvm2/utils/container/impl.h>

#include <uwvm2/runtime/lib/uwvm_runtime_wasip1_memory_bindings.h>

namespace
{
    struct fake_environment
    {
        void* wasip1_memory{};
    };

    struct fake_group_state
    {
        fake_environment env{};
    };
    struct fake_context { fake_environment* env{}; };
    unsigned checks{};
    void require(bool value)
    {
        ++checks;
        if(!value) { ::fast_io::fast_terminate(); }
    }
}

int main()
{
    int first_memory{};
    int second_memory{};
    fake_environment default_environment{&first_memory};
    ::std::array<fake_group_state, 2> configured_groups{{{{&first_memory}}, {{&second_memory}}}};
    fake_environment candidate_environment{&second_memory};
    ::uwvm2::utils::container::vector<fake_context> cache{};
    auto const allowed{[&](::std::size_t module,bool overrides)
    { return ::uwvm2::runtime::lib::details::wasip1_default_context_fast_path(cache,module,&default_environment,overrides); }};
    require(allowed(0u,false)); // Existing unbuilt-cache fallback.
    require(!allowed(0u,true));
    cache.push_back({&default_environment});cache.push_back({&candidate_environment});
    require(allowed(0u,false));
    require(!allowed(1u,false)); // Same final pointer identity, empty old config.
    require(!allowed(0u,true));
    cache.index_unchecked(1u).env=nullptr;
    require(!allowed(1u,false));
    require(allowed(2u,false)); // Existing uncached-module fallback remains.

    ::uwvm2::runtime::lib::details::clear_wasip1_memory_bindings(default_environment, configured_groups);
    require(default_environment.wasip1_memory == nullptr);
    for(auto const& state: configured_groups)
    {
        require(state.env.wasip1_memory == nullptr);
    }
    ::fast_io::io::perrln("wasip1_memory_binding_reset PASS checks=",checks);
}
