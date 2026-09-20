#ifndef UWVM2TEST_RUNNER_USE_LLVM_JIT
# define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#endif
#define UWVM2TEST_STRICT_NO_INTERPRETER 1

#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"

#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

namespace
{
    namespace strict = ::uwvm2test::uwvm_int_strict;
    namespace llvm_jit = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    namespace wasm_type = ::uwvm2::uwvm::wasm::type;

    inline constexpr ::std::size_t wasm_page_bytes{65536uz};

    struct host_memory
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view memory_name{u8"mem"};
        inline static constexpr ::std::uint_least64_t page_size{wasm_page_bytes};

        ::std::array<::std::byte, wasm_page_bytes> bytes{};

        friend bool memory_grow(host_memory&, ::std::uint_least64_t delta_pages) noexcept
        { return delta_pages == 0u; }

        friend ::std::byte* memory_begin(host_memory& memory) noexcept
        { return memory.bytes.data(); }

        friend ::std::uint_least64_t memory_size(host_memory&) noexcept
        { return 1u; }
    };

    struct host_memory_module
    {
        ::uwvm2::utils::container::u8string_view module_name{u8"llvm-bulk-host"};
        using local_memory_tuple = ::uwvm2::utils::container::tuple<host_memory>;
        local_memory_tuple local_memory{};
    };

    static_assert(wasm_type::is_local_imported_memory<host_memory>);
    static_assert(wasm_type::is_local_imported_module<host_memory_module>);

    [[nodiscard]] int fail(int line, char const* message) noexcept
    {
        ::std::fprintf(stderr, "llvm_jit_imported_bulk_memory:%d: %s\n", line, message);
        return 1;
    }

#define LLVM_IMPORTED_BULK_REQUIRE(condition, message) \
    do                                                   \
    {                                                    \
        if(!(condition)) [[unlikely]]                    \
        {                                                \
            return fail(__LINE__, message);              \
        }                                                \
    } while(false)

    [[nodiscard]] strict::byte_vec build_imported_bulk_memory_module()
    {
        strict::module_builder module{};
        module.add_import_memory("llvm-bulk-host", "mem", 1u, 1u, true);

        auto op = [&](strict::byte_vec& code, strict::wasm_op opcode) { strict::append_u8(code, strict::u8(opcode)); };
        auto ext = [&](strict::byte_vec& code, strict::wasm1p1_numeric_op opcode)
        {
            strict::append_u8(code, strict::u8(strict::wasm1p1_op::numeric_prefix));
            strict::append_u32_leb(code, strict::u32(opcode));
        };
        auto i32 = [&](strict::byte_vec& code, ::std::int32_t value) { strict::append_i32_leb(code, value); };

        strict::func_type type{{}, {}};
        strict::func_body body{};
        auto& code{body.code};

        // Backward overlapping copy crossing the bridge's 4 KiB staging boundary.
        op(code, strict::wasm_op::i32_const); i32(code, 1001);
        op(code, strict::wasm_op::i32_const); i32(code, 1000);
        op(code, strict::wasm_op::i32_const); i32(code, 5000);
        ext(code, strict::wasm1p1_numeric_op::memory_copy);
        strict::append_u8(code, 0u);
        strict::append_u8(code, 0u);

        // Forward overlapping copy crossing the same boundary.
        op(code, strict::wasm_op::i32_const); i32(code, 15000);
        op(code, strict::wasm_op::i32_const); i32(code, 15001);
        op(code, strict::wasm_op::i32_const); i32(code, 5000);
        ext(code, strict::wasm1p1_numeric_op::memory_copy);
        strict::append_u8(code, 0u);
        strict::append_u8(code, 0u);

        op(code, strict::wasm_op::i32_const); i32(code, 22000);
        op(code, strict::wasm_op::i32_const); i32(code, 0xa5);
        op(code, strict::wasm_op::i32_const); i32(code, 5000);
        ext(code, strict::wasm1p1_numeric_op::memory_fill);
        strict::append_u8(code, 0u);
        op(code, strict::wasm_op::end);

        static_cast<void>(module.add_func(::std::move(type), ::std::move(body)));
        return module.build();
    }

    [[nodiscard]] ::std::string_view as_string_view(::uwvm2::utils::container::u8string const& text) noexcept
    { return {reinterpret_cast<char const*>(text.data()), text.size()}; }

    [[nodiscard]] bool starts_with(::std::string_view text, ::std::string_view prefix) noexcept
    { return text.size() >= prefix.size() && text.substr(0uz, prefix.size()) == prefix; }

    template <auto Bridge>
    [[nodiscard]] bool module_references_bridge(::llvm::Module const& module) noexcept
    {
#if defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
        // RISC-V64 materializes process-local bridge pointers directly because RuntimeDyld cannot reliably relocate them.
        static_cast<void>(module);
        return true;
#else
        auto const prefix_storage{llvm_jit::details::get_llvm_runtime_bridge_function_symbol_name<Bridge>()};
        auto const prefix{as_string_view(prefix_storage)};
        for(auto const& function: module)
        {
            auto const name{function.getName()};
            if(starts_with({name.data(), name.size()}, prefix)) { return true; }
        }
        return false;
#endif
    }

    [[nodiscard]] wasm_type::local_imported_t* resolve_host_memory(storage::wasm_module_storage_t const& module) noexcept
    {
        if(module.imported_memory_vec_storage.size() != 1uz) { return nullptr; }
        using imported_memory_t = storage::imported_memory_storage_t;
        using link_kind = imported_memory_t::imported_memory_link_kind;
        auto current{::std::addressof(module.imported_memory_vec_storage.index_unchecked(0uz))};
        for(;;)
        {
            if(current == nullptr) { return nullptr; }
            switch(current->link_kind)
            {
                case link_kind::imported:
                    current = current->target.imported_ptr;
                    continue;
                case link_kind::local_imported:
                    return current->target.local_imported.index == 0uz ? current->target.local_imported.module_ptr : nullptr;
                default:
                    return nullptr;
            }
        }
    }

    [[nodiscard]] int test_imported_bulk_memory()
    {
        auto wasm{build_imported_bulk_memory_module()};
        auto features{strict::make_wasm1p1_feature_parameter()};
        wasm_type::local_imported_t host_module{host_memory_module{}};
        auto prepared{strict::prepare_runtime_from_wasm(wasm, u8"llvm_jit_imported_bulk_memory", {}, features, {host_module})};
        LLVM_IMPORTED_BULK_REQUIRE(prepared.mod != nullptr, "runtime module preparation failed");

        ::uwvm2::validation::error::code_validation_error_impl error{};
        llvm_jit::compile_option options{};
        options.validator_feature_parameter = ::std::addressof(features);
        options.verify_llvm_jit_ir = true;
        options.emit_call_stack_frames = false;
        auto compiled{llvm_jit::compile_all_from_uwvm(*prepared.mod, options, error, 0uz)};
        LLVM_IMPORTED_BULK_REQUIRE(error.err_code == ::uwvm2::validation::error::code_validation_error_code::ok,
                                   "LLVM validator rejected imported bulk memory");
        LLVM_IMPORTED_BULK_REQUIRE(compiled.local_funcs.size() == 1uz, "unexpected local function count");
        LLVM_IMPORTED_BULK_REQUIRE(compiled.llvm_jit_module.emitted && compiled.llvm_jit_module.llvm_module != nullptr,
                                   "LLVM module emission failed");
        LLVM_IMPORTED_BULK_REQUIRE(
            module_references_bridge<llvm_jit::details::llvm_jit_local_imported_memory_copy_bridge>(*compiled.llvm_jit_module.llvm_module),
            "emitted IR does not reference the local-imported memory.copy bridge");
        LLVM_IMPORTED_BULK_REQUIRE(
            module_references_bridge<llvm_jit::details::llvm_jit_local_imported_memory_fill_bridge>(*compiled.llvm_jit_module.llvm_module),
            "emitted IR does not reference the local-imported memory.fill bridge");

        auto provider{resolve_host_memory(*prepared.mod)};
        LLVM_IMPORTED_BULK_REQUIRE(provider != nullptr, "imported memory did not resolve to the host provider");

        ::std::vector<::std::byte> expected(wasm_page_bytes);
        for(::std::size_t index{}; index != expected.size(); ++index)
        {
            expected[index] = static_cast<::std::byte>(static_cast<unsigned char>((index * 37uz + 11uz) & 0xffuz));
        }
        LLVM_IMPORTED_BULK_REQUIRE(provider->memory_write_to_index(0uz, 0u, expected.data(), expected.size()), "failed to seed host memory");

        ::std::memmove(expected.data() + 1001uz, expected.data() + 1000uz, 5000uz);
        ::std::memmove(expected.data() + 15000uz, expected.data() + 15001uz, 5000uz);
        ::std::memset(expected.data() + 22000uz, 0xa5, 5000uz);

        ::uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod, 0u, nullptr, 0uz, nullptr, 0uz);

        ::std::vector<::std::byte> actual(wasm_page_bytes);
        LLVM_IMPORTED_BULK_REQUIRE(provider->memory_read_from_index(0uz, 0u, actual.data(), actual.size()), "failed to read host memory");
        LLVM_IMPORTED_BULK_REQUIRE(actual == expected, "host memory copy/fill result differs from memmove/memset semantics");

#if defined(__unix__) || defined(__APPLE__)
        LLVM_IMPORTED_BULK_REQUIRE(
            strict::run_in_child_expect_trap_message("memory access out of bounds",
                                                     [&]
                                                     {
                                                         llvm_jit::details::llvm_jit_local_imported_memory_copy_bridge(
                                                             reinterpret_cast<::std::uintptr_t>(provider), 0uz, 65534, 0, 4);
                                                     }) == 0,
            "out-of-bounds local-imported memory.copy did not trap");
        LLVM_IMPORTED_BULK_REQUIRE(
            strict::run_in_child_expect_trap_message("memory access out of bounds",
                                                     [&]
                                                     {
                                                         llvm_jit::details::llvm_jit_local_imported_memory_fill_bridge(
                                                             reinterpret_cast<::std::uintptr_t>(provider), 0uz, 65535, 0xa5, 2);
                                                     }) == 0,
            "out-of-bounds local-imported memory.fill did not trap");
#endif
        return 0;
    }
}  // namespace

int main()
{
    return test_imported_bulk_memory();
}
