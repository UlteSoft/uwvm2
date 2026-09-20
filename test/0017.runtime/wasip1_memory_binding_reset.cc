#include <array>

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
}

int main()
{
    int first_memory{};
    int second_memory{};
    fake_environment default_environment{&first_memory};
    ::std::array<fake_group_state, 2> configured_groups{{{{&first_memory}}, {{&second_memory}}}};

    ::uwvm2::runtime::lib::details::clear_wasip1_memory_bindings(default_environment, configured_groups);
    if(default_environment.wasip1_memory != nullptr) { return 1; }
    for(auto const& state: configured_groups)
    {
        if(state.env.wasip1_memory != nullptr) { return 2; }
    }
}
