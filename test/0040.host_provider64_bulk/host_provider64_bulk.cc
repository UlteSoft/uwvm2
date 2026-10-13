#define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/impl.h>
#include <array>
#include <cstring>
#include <memory>
#include <string_view>
#include <type_traits>

namespace
{
    namespace strict = ::uwvm2test::uwvm_int_strict;
    namespace compiler = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
    namespace wasm_type = ::uwvm2::uwvm::wasm::type;
    namespace details = compiler::details;
    constexpr ::std::size_t page_bytes{65536uz};

    void require(bool condition, char const* message)
    {
        if(!condition) [[unlikely]]
        {
            ::fast_io::print(::fast_io::err(), "provider64 fixture failure: ", ::fast_io::mnp::os_c_str(message), "\n");
            ::fast_io::fast_terminate();
        }
    }

    struct shared_backing
    {
        // Actual native_file owns the descriptor. The mapping is shared and
        // survives a VM trap in the backing file; the external runner compares
        // every byte after the child process exits. No fork COW inference.
        ::fast_io::native_file file;
        ::fast_io::native_file_loader mapping;

        explicit shared_backing(char const* path)
            : file{::fast_io::mnp::os_c_str(path), ::fast_io::open_mode::in | ::fast_io::open_mode::out},
              mapping{::fast_io::native_mmap_options{
                  ::fast_io::mmap_prot::prot_read | ::fast_io::mmap_prot::prot_write,
                  ::fast_io::mmap_flags::map_shared}, ::fast_io::at(file)}
        {
            require(mapping.size() == page_bytes, "shared native file is exactly one Wasm page");
        }
    };

    struct host_memory
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view memory_name{u8"mem"};
        inline static constexpr ::std::uint_least64_t page_size{page_bytes};
        ::std::shared_ptr<shared_backing> backing{};
        friend bool memory_grow(host_memory&, ::std::uint_least64_t delta) noexcept { return delta == 0u; }
        friend ::std::byte* memory_begin(host_memory& memory) noexcept
        {
            // [actual shared mapping ... page_bytes] owns the complete provider.
            // [safe                               ] no pointer advancement.
            return reinterpret_cast<::std::byte*>(memory.backing->mapping.data());
        }
        friend ::std::uint_least64_t memory_size(host_memory&) noexcept { return 1u; }
    };
    struct host_module
    {
        ::uwvm2::utils::container::u8string_view module_name{u8"provider64-host"};
        using local_memory_tuple = ::uwvm2::utils::container::tuple<host_memory>;
        local_memory_tuple local_memory{};
    };
    static_assert(wasm_type::is_local_imported_memory<host_memory>);
    static_assert(wasm_type::is_local_imported_module<host_module>);
    static_assert(::std::is_same_v<decltype(&details::llvm_jit_local_imported_memory64_fill_bridge),
        void(*)(::std::uintptr_t, ::std::size_t, ::std::uint64_t, details::runtime_wasm_i32, ::std::uint64_t) noexcept>);
    static_assert(::std::is_same_v<decltype(&details::llvm_jit_local_imported_memory64_init_bridge),
        void(*)(::std::uintptr_t, ::std::size_t, ::std::uintptr_t, ::std::uint64_t, ::std::uint64_t,
                details::llvm_jit_atomic_address_carrier_t, details::llvm_jit_atomic_address_carrier_t) noexcept>);

    template<auto Bridge>
    bool references_bridge(::llvm::Module const& module)
    {
        auto const name{details::get_llvm_runtime_bridge_function_symbol_name<Bridge>()};
        ::std::string_view const prefix{reinterpret_cast<char const*>(name.data()), name.size()};
        for(auto const& function : module)
        {
            auto const symbol{function.getName()};
            if(::std::string_view{symbol.data(), symbol.size()}.starts_with(prefix)) { return true; }
        }
        // The native Linux qualification is symbol-backed. Other targets with
        // immediate bridge-pointer lowering need a separate actual IR witness;
        // no target silently returns true here.
        return false;
    }
}

int main(int argc, char** argv)
{
    require(argc == 4, "arguments: real assembled Wasm, shared backing file, function index");
    ::std::uint_least32_t function_index{};
    ::std::string_view const numeric{argv[3]};
    // [complete command argument] end; size is from the native argv terminator.
    // [safe                     ] the bounded fast_io scanner never advances past end.
    auto const parsed{::fast_io::parse_by_scan(numeric.data(), numeric.data() + numeric.size(),
        ::fast_io::mnp::dec(function_index))};
    require(parsed.code == ::fast_io::parse_code::ok && parsed.iter == numeric.data() + numeric.size() &&
        function_index < 11u, "exact bounded function index");

    ::fast_io::native_file_loader input{::fast_io::mnp::os_c_str(argv[1]), ::fast_io::open_mode::in};
    require(input.size() != 0uz && input.size() <= 65536uz, "bounded nonempty actual Wasm fixture");
    strict::byte_vec wasm{};
    wasm.resize(input.size());
    // [complete owned input mapping] -> [same-sized owned module bytes]
    // [safe                        ] both allocations are live for this copy.
    ::std::memcpy(wasm.data(), input.data(), wasm.size());
    auto backing{::std::make_shared<shared_backing>(argv[2])};
    host_module host{};
    ::fast_io::get<0uz>(host.local_memory).backing = backing;
    wasm_type::local_imported_t erased_host{::std::move(host)};
    auto features{strict::make_wasm1p1_feature_parameter()};
    auto& policy{::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features)};
    policy.disable_memory64 = false;
    auto prepared{strict::prepare_runtime_from_wasm(wasm, u8"actual-host-provider64-bulk", {}, features, {erased_host})};
    require(prepared.mod != nullptr, "real parser/initializer accepted the host-imported module");
    auto const access{details::resolve_runtime_memory_access_info(*prepared.mod, 0uz)};
    require(access.memory_p == nullptr && access.local_imported_module_ptr != nullptr &&
        access.local_imported_memory_index == 0uz, "actual imported memory is a host provider, not a native Wasm memory");
    require(prepared.mod->imported_memory_vec_storage.size() == 1uz &&
        prepared.mod->memory_declarations_require_memory64, "actual parsed declaration selects memory64");

    ::uwvm2::validation::error::code_validation_error_impl error{};
    compiler::compile_option options{};
    options.validator_feature_parameter = ::std::addressof(features);
    options.verify_llvm_jit_ir = true;
    options.emit_call_stack_frames = false;
    auto analyzed{compiler::compile_all_from_uwvm(*prepared.mod, options, error, 0uz)};
    require(error.err_code == ::uwvm2::validation::error::code_validation_error_code::ok &&
        analyzed.local_funcs.size() == 11uz && analyzed.llvm_jit_module.emitted &&
        analyzed.llvm_jit_module.llvm_module != nullptr, "all eleven bodies emitted valid LLVM IR");
    require(references_bridge<details::llvm_jit_local_imported_memory64_fill_bridge>(*analyzed.llvm_jit_module.llvm_module) &&
        references_bridge<details::llvm_jit_local_imported_memory64_init_bridge>(*analyzed.llvm_jit_module.llvm_module),
        "analyzed native IR references both actual wide host-provider bridges");
    ::fast_io::print(::fast_io::out(), "{\"schema\":\"uwvm.host-provider64-bulk.actual-function.v1\",\"phase\":\"pre-call\",\"function\":",
        ::fast_io::mnp::dec(function_index),
        ",\"host_provider\":true,\"memory_address_bits\":64,\"analyzed_ir_both_bridges\":true,\"runtime_mode\":\"llvm_jit_only_full\"}\n");
    // This analyzed compiler module is a separate proof from the subsequent
    // actual full runtime materialization; the evidence protocol binds both
    // to the same frozen source/SDK/runtime closure, never conflating objects.
    ::uwvm2::runtime::lib::llvm_jit_call_raw_host_api(prepared.mod, function_index, nullptr, 0uz, nullptr, 0uz);
    ::fast_io::print(::fast_io::out(), "{\"schema\":\"uwvm.host-provider64-bulk.actual-function.v1\",\"phase\":\"returned\",\"function\":",
        ::fast_io::mnp::dec(function_index),
        ",\"host_provider\":true,\"memory_address_bits\":64,\"analyzed_ir_both_bridges\":true,\"runtime_mode\":\"llvm_jit_only_full\"}\n");
    return 0;
}
