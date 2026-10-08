// Real VM regression: a retired Wasm caller tail-enters a host adapter, whose
// callback enters another Wasm function through the public raw host API.
// The nested throw must retain leaf -> surviving entry, never the adapter or
// retired function. Link against the same frozen runtime object as the CLI.
#define UWVM2TEST_RUNNER_USE_LLVM_JIT 1
#define UWVM2TEST_STRICT_NO_INTERPRETER 1
#include "../0013.uwvm_int/strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/lib/uwvm_runtime.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

namespace
{
    namespace strict = uwvm2test::uwvm_int_strict;
    namespace lib = uwvm2::runtime::lib;
    namespace mode = uwvm2::uwvm::runtime::runtime_mode;
    namespace types = uwvm2::uwvm::wasm::type;
    namespace container = uwvm2::utils::container;
    using wasm1 = uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using value_type = uwvm2::parser::wasm::standard::wasm1::type::value_type;
    using host_features = types::feature_list<wasm1>;

    struct reenter
    {
        inline static constexpr container::u8string_view function_name{u8"reenter"};
        using result_tuple = types::import_function_result_tuple_t<host_features>;
        using parameter_tuple = types::import_function_parameter_tuple_t<host_features, value_type::i32>;
        using local_imported_function_type = types::local_imported_function_type_t<result_tuple, parameter_tuple>;
        inline static uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* module{};
        inline static unsigned calls{}, returns{};

        static void call(local_imported_function_type& invocation) noexcept
        {
            if(module == nullptr || ++calls != 1u) { std::abort(); }
            std::uint32_t parameter{static_cast<std::uint32_t>(container::get<0>(invocation.params))};
            std::printf("PROBE host_reentry_entered parameter=%u\n", unsigned(parameter));
            std::fflush(stdout);
            // [prepared module] [initialized local i32 carrier]
            // [safe           ] the outer host entry pins module storage; this
            // synchronous nested call borrows exactly sizeof(parameter) bytes.
            lib::llvm_jit_call_raw_host_api(module, 1u, nullptr, 0uz,
                                           std::addressof(parameter), sizeof(parameter));
            ++returns;
        }
    };

    struct host_module
    {
        container::u8string_view module_name{u8"trace-host"};
        using local_function_tuple = container::tuple<reenter>;
    };
}

int main(int argc, char** argv)
{
    if(argc != 4 ||
       (std::strcmp(argv[2], "instruction") != 0 && std::strcmp(argv[2], "unwind") != 0) ||
       (std::strcmp(argv[3], "normal") != 0 && std::strcmp(argv[3], "throw") != 0))
    {
        std::fputs("usage: probe module.wasm instruction|unwind normal|throw\n", stderr);
        return 2;
    }
    std::ifstream file(argv[1], std::ios::binary);
    std::vector<char> input{std::istreambuf_iterator<char>{file}, {}};
    UWVM2TEST_REQUIRE(file && !input.empty() && input.size() <= 65536uz);
    strict::byte_vec bytes(input.size());
    // [input: size initialized bytes] [bytes: size writable bytes]
    // [safe                        ] the complete fixture is copied without
    // pointer adjustment; both owning containers remain live during preparation.
    std::memcpy(bytes.data(), input.data(), input.size());
    auto features{strict::make_wasm1p1_feature_parameter()};
    auto& core3{uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(features)};
    core3.disable_exceptions = false;
    core3.disable_tail_call = false;
    types::local_imported_t host{host_module{}};
    auto prepared{strict::prepare_runtime_from_wasm(bytes, u8"host-tail-reentry", {}, features, {host})};
    // [prepared-runtime-owned module] remains live through every outer/nested
    // [safe                         ] invocation; clear this borrow before reset.
    reenter::module = prepared.mod;
    mode::global_runtime_llvm_jit_call_stack = std::strcmp(argv[2], "unwind") == 0 ?
        mode::runtime_llvm_jit_call_stack_t::unwind : mode::runtime_llvm_jit_call_stack_t::instruction;
    mode::global_runtime_compile_threads_resolved = 1;
    std::uint32_t parameter{std::strcmp(argv[3], "throw") == 0 ? 1u : 0u};
    lib::full_compile_run_config config{};
    config.entry_function_index = 3u;
    // [initialized entry parameter] no pointer arithmetic or retained guest address.
    // [safe                       ] the host entry validates the exact i32 ABI width.
    config.entry_abi_buffers.param_buffer = reinterpret_cast<std::byte const*>(std::addressof(parameter));
    config.entry_abi_buffers.param_bytes = sizeof(parameter);
    lib::full_compile_and_run_main_module(u8"host-tail-reentry", config);
    if(parameter != 0u)
    {
        std::fputs("FAIL uncaught reentry exception returned\n", stderr);
        return 1;
    }
    std::uint32_t normal_marker{};
    // [live writable i32 result] the read-marker export returns exactly one i32.
    // [safe                   ] its synchronous output cannot outlive this local.
    lib::llvm_jit_call_raw_host_api(prepared.mod, 4u, std::addressof(normal_marker),
                                  sizeof(normal_marker), nullptr, 0uz);
    UWVM2TEST_REQUIRE(reenter::calls == 1u && reenter::returns == 1u && normal_marker == 1u);
    reenter::module = nullptr;
    prepared.reset();
    std::puts("PASS normal host reentry calls=1 returns=1 surviving_entry_continued=1");
}
