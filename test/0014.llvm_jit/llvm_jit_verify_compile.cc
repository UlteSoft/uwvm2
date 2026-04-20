#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace
{

    // Generated from test/0014.llvm_jit/nontrivial_start.wat to keep this test
    // self-contained and independent from a host-installed wat2wasm.
    // Regenerate with wat2wasm + xxd -i if the .wat fixture changes.
    inline constexpr ::std::array<unsigned char, 98uz> nontrivial_start_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u,
        0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x00u, 0x05u, 0x03u, 0x01u, 0x00u, 0x01u, 0x07u,
        0x0au, 0x01u, 0x06u, 0x5fu, 0x73u, 0x74u, 0x61u, 0x72u, 0x74u, 0x00u, 0x00u, 0x0au,
        0x3du, 0x01u, 0x3bu, 0x01u, 0x01u, 0x7fu, 0x41u, 0x00u, 0x21u, 0x00u, 0x02u, 0x40u,
        0x03u, 0x40u, 0x20u, 0x00u, 0x41u, 0x05u, 0x48u, 0x45u, 0x0du, 0x01u, 0x20u, 0x00u,
        0x41u, 0x01u, 0x6au, 0x21u, 0x00u, 0x0cu, 0x00u, 0x0bu, 0x0bu, 0x20u, 0x00u, 0x41u,
        0x05u, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x41u, 0x00u, 0x41u, 0x2au, 0x36u, 0x02u,
        0x00u, 0x41u, 0x00u, 0x28u, 0x02u, 0x00u, 0x41u, 0x2au, 0x47u, 0x04u, 0x40u, 0x00u,
        0x0bu, 0x0bu};

    [[maybe_unused]] void test_runtime_entry()
    {
        ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t module{};
        ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_option opt{};
        ::uwvm2::validation::error::code_validation_error_impl err{};

        [[maybe_unused]] auto storage{
            ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_all_from_uwvm(module, opt, err, 0uz)};
    }

    [[maybe_unused]] void test_parser_entry()
    {
        using Feature = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
        using module_storage_t =
            ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Feature>;

        [[maybe_unused]] module_storage_t module_storage{};
        [[maybe_unused]] bool ok{
            ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::validate_all_wasm_code_for_module(
                module_storage, {}, {})};
    }

    [[nodiscard]] ::std::string quote_argument(::std::filesystem::path const& path)
    {
        return ::std::string{"\""} + path.string() + "\"";
    }

    [[nodiscard]] ::std::filesystem::path find_uwvm_binary(::std::filesystem::path dir)
    {
        for(;;)
        {
            auto const candidate{dir / "uwvm"};
            if(::std::filesystem::exists(candidate)) [[likely]] { return candidate; }
            if(dir == dir.root_path()) [[unlikely]] { return {}; }
            dir = dir.parent_path();
        }
    }

    [[nodiscard]] bool write_fixture(::std::filesystem::path const& wasm_path)
    {
        ::std::error_code ec{};
        ::std::filesystem::create_directories(wasm_path.parent_path(), ec);
        if(ec) [[unlikely]]
        {
            ::std::cerr << "failed to create fixture directory: " << wasm_path.parent_path() << '\n';
            return false;
        }

        ::std::ofstream output(wasm_path, ::std::ios::binary | ::std::ios::trunc);
        if(!output) [[unlikely]]
        {
            ::std::cerr << "failed to open fixture output: " << wasm_path << '\n';
            return false;
        }

        output.write(reinterpret_cast<char const*>(nontrivial_start_wasm.data()),
                     static_cast<::std::streamsize>(nontrivial_start_wasm.size()));

        if(!output) [[unlikely]]
        {
            ::std::cerr << "failed to write fixture output: " << wasm_path << '\n';
            return false;
        }

        return true;
    }

    [[nodiscard]] bool run_mode(::std::filesystem::path const& uwvm_path,
                                ::std::filesystem::path const& wasm_path,
                                ::std::string_view compiler_mode)
    {
        auto const command{
            quote_argument(uwvm_path) + " -Rcm full -Rcc " + ::std::string{compiler_mode} + " --run " + quote_argument(wasm_path)};

        ::std::cout << "[llvm_jit] " << command << '\n';

        auto const status{::std::system(command.c_str())};
        if(status == 0) [[likely]] { return true; }

        ::std::cerr << "uwvm returned non-zero status for mode " << compiler_mode << ": " << status << '\n';
        return false;
    }

}  // namespace

int main(int argc, char** argv)
{
    if(argc <= 0 || argv == nullptr || argv[0] == nullptr) [[unlikely]]
    {
        ::std::cerr << "missing argv[0]\n";
        return 1;
    }

    auto const executable{::std::filesystem::absolute(argv[0])};
    auto const executable_dir{executable.parent_path()};
    auto const uwvm_path{find_uwvm_binary(executable_dir)};
    if(uwvm_path.empty()) [[unlikely]]
    {
        ::std::cerr << "failed to locate uwvm next to test executable: " << executable << '\n';
        return 1;
    }

    auto const wasm_path{executable_dir / "test-artifacts" / "0014.llvm_jit" / "nontrivial_start.wasm"};
    if(!write_fixture(wasm_path)) [[unlikely]] { return 1; }

    if(!run_mode(uwvm_path, wasm_path, "jit")) [[unlikely]] { return 1; }
    if(!run_mode(uwvm_path, wasm_path, "tiered")) [[unlikely]] { return 1; }

    return 0;
}
