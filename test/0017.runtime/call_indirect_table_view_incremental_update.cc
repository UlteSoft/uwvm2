#include <uwvm2/runtime/lib/uwvm_runtime_call_indirect_table_views.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace
{
    // The production ABI carries the same five operation kinds. This mock keeps the ownership/range policy test independent
    // of the full runtime storage graph and LLVM headers.
    enum class mutation_kind : unsigned char
    {
        set,
        init,
        copy,
        fill,
        grow
    };

    struct mock_table
    {
        ::std::vector<int> elems{};
    };

    struct mock_table_view
    {
        ::std::uintptr_t data_address{};
        ::std::size_t size{};
    };

    struct mock_runtime_module
    {
        ::std::vector<mock_table*> resolved_tables{};
        ::std::vector<mock_table_view> llvm_jit_call_indirect_table_views{};
    };

    struct mock_module_record
    {
        mock_runtime_module const* runtime_module{};
        ::std::vector<::std::vector<int>> targets{};
        int caller_bias{};
        ::std::size_t encoded_element_count{};
    };

    void publish_view(mock_module_record& rec, mock_runtime_module& module, ::std::size_t table_index)
    {
        auto& targets{rec.targets[table_index]};
        module.llvm_jit_call_indirect_table_views[table_index] = {
            reinterpret_cast<::std::uintptr_t>(targets.data()), targets.size()};
    }

    void initialize_record(mock_module_record& rec)
    {
        auto& module{*const_cast<mock_runtime_module*>(rec.runtime_module)};
        rec.targets.resize(module.resolved_tables.size());
        module.llvm_jit_call_indirect_table_views.resize(module.resolved_tables.size());
        for(::std::size_t table_index{}; table_index != module.resolved_tables.size(); ++table_index)
        {
            auto const table{module.resolved_tables[table_index]};
            auto& targets{rec.targets[table_index]};
            targets.resize(table->elems.size());
            for(::std::size_t i{}; i != targets.size(); ++i) { targets[i] = rec.caller_bias + table->elems[i]; }
            publish_view(rec, module, table_index);
        }
    }

    [[nodiscard]] ::std::size_t update_aliases(::std::vector<mock_module_record>& records,
                                                mock_table* table,
                                                mutation_kind kind,
                                                ::std::size_t begin,
                                                ::std::size_t count)
    {
        bool grow{};
        switch(kind)
        {
            case mutation_kind::set: [[fallthrough]];
            case mutation_kind::init: [[fallthrough]];
            case mutation_kind::copy: [[fallthrough]];
            case mutation_kind::fill: break;
            case mutation_kind::grow: grow = true; break;
        }

        return ::uwvm2::runtime::lib::details::for_each_borrowed_llvm_jit_call_indirect_table_view_alias(
            records,
            table,
            [](mock_runtime_module const& module, ::std::size_t table_index) noexcept
            { return module.resolved_tables[table_index]; },
            [&](mock_module_record& rec, mock_runtime_module& module, ::std::size_t table_index) noexcept
            {
                auto& targets{rec.targets[table_index]};
                if(grow) { targets.resize(table->elems.size()); }
                for(::std::size_t i{begin}; i != begin + count; ++i)
                {
                    targets[i] = rec.caller_bias + table->elems[i];
                    ++rec.encoded_element_count;
                }
                if(grow) { publish_view(rec, module, table_index); }
            });
    }

    [[nodiscard]] bool equal(::std::vector<int> const& lhs, ::std::initializer_list<int> rhs)
    { return lhs == ::std::vector<int>{rhs}; }
}

int main()
{
    mock_table shared{{1, 2, 3, 4}};
    mock_table untouched{{8, 9}};
    mock_runtime_module provider{{&shared, &untouched}, {}};
    mock_runtime_module importer{{&shared}, {}};
    ::std::vector<mock_module_record> records{{&provider, {}, 100, 0}, {&importer, {}, 200, 0}};
    for(auto& rec: records) { initialize_record(rec); }

    auto const provider_shared_address{records[0].targets[0].data()};
    auto const importer_shared_address{records[1].targets[0].data()};
    auto const untouched_address{records[0].targets[1].data()};
    auto const untouched_view{provider.llvm_jit_call_indirect_table_views[1]};

    shared.elems[1] = 12;
    if(update_aliases(records, &shared, mutation_kind::set, 1, 1) != 2 ||
       !equal(records[0].targets[0], {101, 112, 103, 104}) ||
       !equal(records[1].targets[0], {201, 212, 203, 204}) ||
       records[0].targets[0].data() != provider_shared_address || records[1].targets[0].data() != importer_shared_address)
    {
        return 1;
    }

    shared.elems[2] = 30;
    shared.elems[3] = 40;
    if(update_aliases(records, &shared, mutation_kind::fill, 2, 2) != 2 ||
       !equal(records[0].targets[0], {101, 112, 130, 140}) ||
       !equal(records[1].targets[0], {201, 212, 230, 240}))
    {
        return 2;
    }

    shared.elems[0] = untouched.elems[0];
    shared.elems[1] = untouched.elems[1];
    if(update_aliases(records, &shared, mutation_kind::copy, 0, 2) != 2 ||
       !equal(records[0].targets[0], {108, 109, 130, 140}) ||
       !equal(records[1].targets[0], {208, 209, 230, 240}))
    {
        return 3;
    }

    shared.elems[2] = 33;
    if(update_aliases(records, &shared, mutation_kind::init, 2, 1) != 2 || records[0].targets[0][2] != 133 ||
       records[1].targets[0][2] != 233)
    {
        return 4;
    }

    auto const provider_prefix{records[0].targets[0]};
    auto const importer_prefix{records[1].targets[0]};
    auto const old_size{shared.elems.size()};
    shared.elems.push_back(50);
    shared.elems.push_back(60);
    if(update_aliases(records, &shared, mutation_kind::grow, old_size, 2) != 2 ||
       records[0].targets[0].size() != 6 || records[1].targets[0].size() != 6)
    {
        return 5;
    }
    for(::std::size_t i{}; i != old_size; ++i)
    {
        if(records[0].targets[0][i] != provider_prefix[i] || records[1].targets[0][i] != importer_prefix[i]) { return 6; }
    }
    if(records[0].targets[0][4] != 150 || records[0].targets[0][5] != 160 || records[1].targets[0][4] != 250 ||
       records[1].targets[0][5] != 260)
    {
        return 7;
    }

    // The non-target table is neither re-encoded nor republished by any mutation of the shared resolved table.
    if(records[0].targets[1].data() != untouched_address || !equal(records[0].targets[1], {108, 109}) ||
       provider.llvm_jit_call_indirect_table_views[1].data_address != untouched_view.data_address ||
       provider.llvm_jit_call_indirect_table_views[1].size != untouched_view.size ||
       records[0].encoded_element_count != 8 || records[1].encoded_element_count != 8)
    {
        return 8;
    }
    return 0;
}
