#include <uwvm2/runtime/lib/uwvm_runtime_imported_function_lookup.h>

#include <array>
#include <cstddef>
#include <memory>
#include <vector>

namespace
{
    struct mock_imported_function
    {
        ::std::size_t value{};
    };

    struct mock_runtime_module
    {
        ::std::vector<mock_imported_function> imported_function_vec_storage{};
    };

    struct mock_module_record
    {
        mock_runtime_module const* runtime_module{};
    };
}

int main()
{
    mock_runtime_module table_owner{{{10uz}}};
    mock_runtime_module element_owner{{{20uz}, {21uz}, {22uz}}};
    ::std::array<mock_module_record, 3> records{{{&table_owner}, {nullptr}, {&element_owner}}};

    // The element was created by another module and then written through an imported alias of table_owner's table.
    auto const cross_module_location{::uwvm2::runtime::lib::details::find_runtime_imported_function_location(
        records, ::std::addressof(element_owner.imported_function_vec_storage[1]))};
    if(!cross_module_location.found || cross_module_location.module_id != 2uz || cross_module_location.function_index != 1uz) { return 1; }

    auto const owner_location{::uwvm2::runtime::lib::details::find_runtime_imported_function_location(
        records, ::std::addressof(table_owner.imported_function_vec_storage[0]))};
    if(!owner_location.found || owner_location.module_id != 0uz || owner_location.function_index != 0uz) { return 2; }

    mock_imported_function foreign{};
    if(::uwvm2::runtime::lib::details::find_runtime_imported_function_location(records, ::std::addressof(foreign)).found) { return 3; }

    auto const misaligned{reinterpret_cast<mock_imported_function const*>(
        reinterpret_cast<::std::byte const*>(element_owner.imported_function_vec_storage.data()) + 1uz)};
    if(::uwvm2::runtime::lib::details::find_runtime_imported_function_location(records, misaligned).found) { return 4; }
}
