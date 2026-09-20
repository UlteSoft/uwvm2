#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace uwvm2::runtime::lib::details
{
    struct runtime_imported_function_location
    {
        ::std::size_t module_id{};
        ::std::size_t function_index{};
        bool found{};
    };

    // A funcref table can contain a reference created by any runtime module, including one written through an imported table alias.
    // Locate the storage object by integer address ranges instead of comparing or subtracting pointers from unrelated allocations.
    template <typename RuntimeModuleRecords, typename ImportedFunction>
    [[nodiscard]] inline constexpr runtime_imported_function_location
        find_runtime_imported_function_location(RuntimeModuleRecords const& records, ImportedFunction const* function) noexcept
    {
        if(function == nullptr) [[unlikely]] { return {}; }

        constexpr ::std::size_t element_size{sizeof(ImportedFunction)};
        static_assert(element_size != 0uz);
        constexpr auto address_max{::std::numeric_limits<::std::uintptr_t>::max()};
        auto const function_address{reinterpret_cast<::std::uintptr_t>(function)};

        ::std::size_t module_id{};
        for(auto const& record: records)
        {
            auto const runtime_module{record.runtime_module};
            if(runtime_module != nullptr)
            {
                auto const& functions{runtime_module->imported_function_vec_storage};
                auto const count{functions.size()};
                if(count != 0uz)
                {
                    auto const begin{functions.data()};
                    if(begin == nullptr || count > (address_max / element_size)) [[unlikely]] { return {}; }

                    auto const begin_address{reinterpret_cast<::std::uintptr_t>(begin)};
                    auto const byte_size{static_cast<::std::uintptr_t>(count * element_size)};
                    if(begin_address > (address_max - byte_size)) [[unlikely]] { return {}; }

                    auto const end_address{begin_address + byte_size};
                    if(function_address >= begin_address && function_address < end_address)
                    {
                        auto const offset{function_address - begin_address};
                        if((offset % element_size) != 0u) [[unlikely]] { return {}; }

                        auto const function_index{static_cast<::std::size_t>(offset / element_size)};
                        if(function_index >= count) [[unlikely]] { return {}; }
                        return {module_id, function_index, true};
                    }
                }
            }
            ++module_id;
        }

        return {};
    }
}
