#include <uwvm2/runtime/lib/uwvm_runtime_call_indirect_table_views.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{
    struct mock_table_view
    {
        ::std::uintptr_t data_address{};
        ::std::size_t size{};
    };

    struct mock_runtime_module
    {
        ::std::vector<mock_table_view> llvm_jit_call_indirect_table_views{};
    };

    struct mock_module_record
    {
        mock_runtime_module const* runtime_module{};
    };

    void refresh_hook() noexcept {}

    [[nodiscard]] bool is_clear(mock_runtime_module const& module) noexcept
    {
        for(auto const& view: module.llvm_jit_call_indirect_table_views)
        {
            if(view.data_address != 0u || view.size != 0uz) { return false; }
        }
        return true;
    }
}

int main()
{
    mock_runtime_module first{{{0x1234u, 3uz}, {0x5678u, 7uz}}};
    mock_runtime_module second{{{0x9ABCu, 11uz}}};
    ::std::vector<mock_module_record> records{{&first}, {nullptr}, {&second}};

    void (*hook)() noexcept = &refresh_hook;
    ::uwvm2::runtime::lib::details::detach_borrowed_llvm_jit_call_indirect_table_views(records, hook);
    if(hook != nullptr || !is_clear(first) || !is_clear(second)) { return 1; }

    first.llvm_jit_call_indirect_table_views[0] = {0xDEF0u, 13uz};
    ::uwvm2::runtime::lib::details::clear_borrowed_llvm_jit_call_indirect_table_views(records);
    return is_clear(first) ? 0 : 2;
}
