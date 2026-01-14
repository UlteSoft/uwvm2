#include <uwvm2/validation/standard/wasm1/impl.h>

void test()
{
    using Feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using module_storage_t = ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Feature>;

    [[maybe_unused]] module_storage_t module_storage{};
    ::uwvm2::validation::error::code_validation_error_impl err{};

    ::std::byte const code_buf[1]{};
    ::std::byte const* const code_begin{code_buf};
    ::std::byte const* const code_end{code_buf};

    // Compile-only instantiation test for the validator template entry point.
    ::uwvm2::validation::standard::wasm1::validate_code<Feature>(
        ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_code_version{}, module_storage, 0uz, code_begin, code_end, err);
}

int main()
{
    // NOTE: do not call `test()`; this binary is only used to force template instantiation at compile time.
    return 0;
}
