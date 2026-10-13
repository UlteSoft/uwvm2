// Regression for import forwarding across independently allocated module function vectors.
#include <uwvm2/runtime/compiler/uwvm_int/compile_all_from_uwvm/translate.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>

int main()
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    namespace details = ::uwvm2::runtime::compiler::uwvm_int::compile_all_from_uwvm::details;

    storage::wasm_binfmt1_final_function_type_t function_type{};
    storage::wasm_module_storage_t consumer{};
    storage::wasm_module_storage_t provider{};
    consumer.imported_function_vec_storage.resize(1uz);
    provider.local_defined_function_vec_storage.resize(1uz);
    auto& provider_function{provider.local_defined_function_vec_storage.index_unchecked(0uz)};
    provider_function.function_type_ptr = ::std::addressof(function_type);
    auto& alias{consumer.imported_function_vec_storage.index_unchecked(0uz)};
    alias.link_kind = storage::imported_function_link_kind::defined;
    alias.target.defined_ptr = ::std::addressof(provider_function);

    auto const foreign{details::resolve_runtime_import_direct_defined_call(consumer, 0uz)};
    if(foreign.direct_callable || foreign.function_type_ptr != ::std::addressof(function_type))
    {
        ::std::fputs("foreign defined-function alias was incorrectly treated as local\n", stderr);
        return 1;
    }

    consumer.local_defined_function_vec_storage.resize(2uz);
    auto& local_function{consumer.local_defined_function_vec_storage.index_unchecked(1uz)};
    local_function.function_type_ptr = ::std::addressof(function_type);
    alias.target.defined_ptr = ::std::addressof(local_function);
    auto const local{details::resolve_runtime_import_direct_defined_call(consumer, 0uz)};
    if(!local.direct_callable || local.local_defined_index != 1uz ||
       local.function_type_ptr != ::std::addressof(function_type))
    {
        ::std::fputs("local defined-function alias did not resolve to index 1\n", stderr);
        return 1;
    }

    ::std::size_t index{};
    auto const begin{consumer.local_defined_function_vec_storage.data()};
    auto const interior_address{reinterpret_cast<::std::uintptr_t>(begin) + 1u};
    auto const interior{reinterpret_cast<storage::local_defined_function_storage_t const*>(interior_address)};
    if(details::classify_runtime_defined_function_pointer(begin, 2uz, interior, index) !=
       details::runtime_defined_function_pointer_membership::invalid)
    {
        ::std::fputs("interior-byte function pointer was not rejected\n", stderr);
        return 1;
    }
    // The consumer vector's interior byte is malformed; the resolution path must reject it before dereference.
    alias.target.defined_ptr = reinterpret_cast<storage::local_defined_function_storage_t*>(interior_address);
    auto const invalid_alias{details::resolve_runtime_import_direct_defined_call(consumer, 0uz)};
    if(invalid_alias.direct_callable || invalid_alias.function_type_ptr != nullptr)
    {
        ::std::fputs("interior-byte defined-function alias was dereferenced or accepted\n", stderr);
        return 1;
    }
    ::std::puts("PASS imported-defined pointer membership and direct call classification");
}
