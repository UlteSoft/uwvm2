#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace
{

    inline constexpr ::std::string_view wasm1p1_cpp_source{R"cpp(
#include <stddef.h>
#include <stdint.h>

extern "C" volatile int32_t g_i32 = 0x000000fb;
extern "C" volatile float g_f32 = -7.25f;
extern "C" volatile uint32_t g_len = 96;
extern "C" uint8_t g_src[128] = {};
extern "C" uint8_t g_dst[128] = {};
extern "C" volatile int g_sink = 0;

extern "C" __attribute__((noinline)) int sign_extend_probe(volatile int32_t* p)
{
    return static_cast<int8_t>(*p);
}

extern "C" __attribute__((noinline)) int saturating_probe(float v)
{
    return __builtin_wasm_trunc_saturate_s_i32_f32(v);
}

extern "C" __attribute__((noinline)) int indirect_a(int x) { return x + 13; }
extern "C" __attribute__((noinline)) int indirect_b(int x) { return x * 3; }

extern "C" __attribute__((noinline)) int copy_probe(uint8_t* dst, uint8_t const* src, uint32_t len)
{
    __builtin_memcpy(dst, src, len);
    return dst[0] + dst[len - 1];
}

int main()
{
    for(uint32_t i = 0; i != 128; ++i) { g_src[i] = static_cast<uint8_t>((i * 17u + 9u) & 255u); }

    int (*table[2])(int) = {indirect_a, indirect_b};
    auto const len = g_len;
    auto const copied = copy_probe(g_dst, g_src, len);
    auto const sign = sign_extend_probe(&g_i32);
    auto const sat = saturating_probe(g_f32);
    auto const indirect = table[(copied ^ static_cast<uint32_t>(sign)) & 1u](sat + copied);

    if(g_dst[0] != 9u || g_dst[len - 1u] != static_cast<uint8_t>(((len - 1u) * 17u + 9u) & 255u)) { __builtin_trap(); }
    if(sign != -5 || sat != -7) { __builtin_trap(); }
    g_sink = indirect;
    return 0;
}
)cpp"};

    inline constexpr ::std::string_view wasm1p1_feature_args{
        "--runtime-llvm-jit-cache-path disable --wasm-feature-wasm1.1"};

    inline constexpr ::std::string_view wasm1p1_explicit_feature_args{
        "--runtime-llvm-jit-cache-path disable --wasm-feature-wasm1.1 "
        "--wasm-feature-wasm1.1"};

    [[nodiscard]] ::std::string quote_argument(::std::filesystem::path const& path)
    {
        return ::std::string{"\""} + path.string() + "\"";
    }

    [[nodiscard]] int run_system_command(::std::string const& command)
    {
#ifdef _WIN32
        auto const wrapped{::std::string{"cmd.exe /S /C \""} + command + "\""};
        return ::std::system(wrapped.c_str());
#else
        return ::std::system(command.c_str());
#endif
    }

    [[nodiscard]] bool run_command(::std::string const& command, char const* label)
    {
        ::std::cout << "[llvm_jit_wasm1p1_cpp] " << command << '\n';

        auto const status{run_system_command(command)};
        if(status == 0) [[likely]] { return true; }

        ::std::cerr << "command returned non-zero status for " << label << ": " << status << '\n';
        return false;
    }

    [[nodiscard]] bool run_expect_failure(::std::string const& command, char const* label)
    {
        ::std::cout << "[llvm_jit_wasm1p1_cpp] " << command << '\n';

        auto const status{run_system_command(command)};
        if(status != 0) [[likely]] { return true; }

        ::std::cerr << "command unexpectedly succeeded for " << label << '\n';
        return false;
    }

    [[nodiscard]] ::std::filesystem::path find_uwvm_binary(::std::filesystem::path dir)
    {
        for(;;)
        {
            auto const candidate{dir / "uwvm"};
            if(::std::filesystem::exists(candidate)) [[likely]] { return candidate; }
#ifdef _WIN32
            auto const windows_candidate{dir / "uwvm.exe"};
            if(::std::filesystem::exists(windows_candidate)) [[likely]] { return windows_candidate; }
#endif
            if(dir == dir.root_path()) [[unlikely]] { return {}; }
            dir = dir.parent_path();
        }
    }

    [[nodiscard]] ::std::filesystem::path find_project_root(::std::filesystem::path dir)
    {
        for(;;)
        {
            if(::std::filesystem::exists(dir / "xmake.lua") && ::std::filesystem::exists(dir / "test" / "0014.llvm_jit")) { return dir; }
            if(dir == dir.root_path()) [[unlikely]] { return {}; }
            dir = dir.parent_path();
        }
    }

    [[nodiscard]] ::std::filesystem::path getenv_path(char const* name)
    {
        auto const value{::std::getenv(name)};
        if(value == nullptr || *value == '\0') { return {}; }
        return ::std::filesystem::path{value};
    }

    [[nodiscard]] bool command_available(::std::filesystem::path const& command_path)
    {
#ifdef _WIN32
        auto const null_device{"NUL"};
#else
        auto const null_device{"/dev/null"};
#endif
        return run_system_command(quote_argument(command_path) + " --version > " + null_device + " 2>&1") == 0;
    }

    [[nodiscard]] ::std::filesystem::path find_clangxx(::std::filesystem::path const& project_root)
    {
        auto env_clangxx{getenv_path("UWVM_WASM_CLANGXX")};
        if(!env_clangxx.empty()) { return env_clangxx; }

        auto const workspace_root{project_root.parent_path().parent_path()};
        for(auto const& candidate: {workspace_root / "tool-chain" / "tools" / "aarch64-apple-darwin-llvm" / "llvm" / "bin" / "clang++",
                                    ::std::filesystem::path{"/Users/liyinan/Documents/MacroModel/tool-chain/tools/aarch64-apple-darwin-llvm/llvm/bin/clang++"}})
        {
            if(::std::filesystem::exists(candidate)) { return candidate; }
        }

        return ::std::filesystem::path{"clang++"};
    }

    [[nodiscard]] ::std::filesystem::path find_wasi_sysroot(::std::filesystem::path const& project_root)
    {
        auto env_sysroot{getenv_path("UWVM_WASI_SYSROOT")};
        if(!env_sysroot.empty()) { return env_sysroot; }

        auto const src_root{project_root.parent_path()};
        for(auto const& candidate: {src_root / "wasi-libc" / "build-mvp" / "sysroot",
                                    ::std::filesystem::path{"/Users/liyinan/Documents/MacroModel/src/wasi-libc/build-mvp/sysroot"}})
        {
            if(::std::filesystem::exists(candidate)) { return candidate; }
        }

        return {};
    }

    [[nodiscard]] bool write_text_file(::std::filesystem::path const& path, ::std::string_view text)
    {
        ::std::error_code ec{};
        ::std::filesystem::create_directories(path.parent_path(), ec);
        if(ec) [[unlikely]]
        {
            ::std::cerr << "failed to create directory for " << path << ": " << ec.message() << '\n';
            return false;
        }

        ::std::ofstream output(path, ::std::ios::trunc);
        if(!output) [[unlikely]]
        {
            ::std::cerr << "failed to open output file: " << path << '\n';
            return false;
        }

        output.write(text.data(), static_cast<::std::streamsize>(text.size()));
        if(!output) [[unlikely]]
        {
            ::std::cerr << "failed to write output file: " << path << '\n';
            return false;
        }

        return true;
    }

    [[nodiscard]] bool read_binary_file(::std::filesystem::path const& path, ::std::vector<unsigned char>& bytes)
    {
        ::std::ifstream input(path, ::std::ios::binary);
        if(!input) [[unlikely]]
        {
            ::std::cerr << "failed to open wasm file: " << path << '\n';
            return false;
        }

        bytes.assign(::std::istreambuf_iterator<char>{input}, ::std::istreambuf_iterator<char>{});
        if(input.bad()) [[unlikely]]
        {
            ::std::cerr << "failed to read wasm file: " << path << '\n';
            return false;
        }

        return true;
    }

    [[nodiscard]] bool read_varuint32(::std::vector<unsigned char> const& bytes,
                                      ::std::size_t& pos,
                                      ::std::size_t end,
                                      ::std::uint_least32_t& value) noexcept
    {
        value = 0u;

        for(unsigned shift{}; shift < 35u && pos < end; shift += 7u)
        {
            auto const byte{bytes[pos++]};
            value |= static_cast<::std::uint_least32_t>(byte & 0x7fu) << shift;
            if((byte & 0x80u) == 0u) { return true; }
        }

        return false;
    }

    [[nodiscard]] bool find_code_section(::std::vector<unsigned char> const& bytes, ::std::size_t& begin, ::std::size_t& end) noexcept
    {
        if(bytes.size() < 8uz || bytes[0] != 0x00u || bytes[1] != 0x61u || bytes[2] != 0x73u || bytes[3] != 0x6du) { return false; }

        ::std::size_t pos{8uz};
        while(pos < bytes.size())
        {
            auto const section_id{bytes[pos++]};
            ::std::uint_least32_t section_size{};
            if(!read_varuint32(bytes, pos, bytes.size(), section_size)) { return false; }

            auto const payload_begin{pos};
            auto const payload_end{payload_begin + static_cast<::std::size_t>(section_size)};
            if(payload_end > bytes.size()) { return false; }

            if(section_id == 10u)
            {
                begin = payload_begin;
                end = payload_end;
                return true;
            }

            pos = payload_end;
        }

        return false;
    }

    [[nodiscard]] bool code_section_has(::std::vector<unsigned char> const& bytes,
                                        ::std::size_t begin,
                                        ::std::size_t end,
                                        ::std::initializer_list<unsigned char> opcode) noexcept
    {
        if(opcode.size() == 0uz || end < begin || end - begin < opcode.size()) { return false; }
        return ::std::search(bytes.begin() + static_cast<::std::ptrdiff_t>(begin),
                             bytes.begin() + static_cast<::std::ptrdiff_t>(end),
                             opcode.begin(),
                             opcode.end()) != bytes.begin() + static_cast<::std::ptrdiff_t>(end);
    }

    [[nodiscard]] bool check_wasm1p1_opcodes(::std::filesystem::path const& wasm_path)
    {
        ::std::vector<unsigned char> bytes{};
        if(!read_binary_file(wasm_path, bytes)) [[unlikely]] { return false; }

        ::std::size_t code_begin{};
        ::std::size_t code_end{};
        if(!find_code_section(bytes, code_begin, code_end)) [[unlikely]]
        {
            ::std::cerr << "compiled wasm is missing a valid code section: " << wasm_path << '\n';
            return false;
        }

        struct expected_opcode
        {
            char const* name;
            ::std::initializer_list<unsigned char> bytes;
        };

        for(auto const expected: {expected_opcode{"memory.copy", {0xfcu, 0x0au}},
                                  expected_opcode{"i32.trunc_sat_f32_s", {0xfcu, 0x00u}},
                                  expected_opcode{"i32.extend8_s", {0xc0u}},
                                  expected_opcode{"call_indirect", {0x11u}}})
        {
            if(!code_section_has(bytes, code_begin, code_end, expected.bytes)) [[unlikely]]
            {
                ::std::cerr << "compiled wasm is missing expected opcode " << expected.name << ": " << wasm_path << '\n';
                return false;
            }
        }

        return true;
    }

    [[nodiscard]] bool compile_wasm1p1_cpp(::std::filesystem::path const& clangxx,
                                           ::std::filesystem::path const& sysroot,
                                           ::std::filesystem::path const& source_path,
                                           ::std::filesystem::path const& wasm_path)
    {
        auto command{quote_argument(clangxx)};
        command += " -o " + quote_argument(wasm_path);
        command += " " + quote_argument(source_path);
        command += " -Ofast -Wno-deprecated-ofast -s -flto -fuse-ld=lld";
        command += " -fno-rtti -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-exceptions";
        command += " --target=wasm32-wasip1 --sysroot=" + quote_argument(sysroot);
        command += " -std=c++26";
        command += " -mbulk-memory -mbulk-memory-opt -mnontrapping-fptoint -msign-ext";
        command += " -mmutable-globals -mmultivalue -mreference-types -mcall-indirect-overlong";

        if(!run_command(command, "clang++ wasm1p1 compile")) [[unlikely]] { return false; }
        return check_wasm1p1_opcodes(wasm_path);
    }

    [[nodiscard]] ::std::string make_uwvm_command(::std::filesystem::path const& uwvm_path,
                                                  ::std::string_view mode_args,
                                                  ::std::string_view extra_args,
                                                  ::std::filesystem::path const& wasm_path)
    {
        auto command{quote_argument(uwvm_path) + " " + ::std::string{mode_args}};
        if(!extra_args.empty())
        {
            command.push_back(' ');
            command.append(extra_args);
        }
        command += " --run " + quote_argument(wasm_path);
        return command;
    }

    [[nodiscard]] bool run_execution_matrix(::std::filesystem::path const& uwvm_path, ::std::filesystem::path const& wasm_path)
    {
        struct runtime_mode
        {
            char const* label;
            char const* args;
        };

        constexpr ::std::array runtime_modes{
            runtime_mode{"full", "-Rcm full -Rcc jit"},
            runtime_mode{"aot", "-Raot"},
            runtime_mode{"lazy", "-Rjit"},
            runtime_mode{"lazy_verification", "-Rcm lazy+verification -Rcc jit"},
            runtime_mode{"tiered", "-Rtiered"},
            runtime_mode{"tiered_no_t0", "-Rtiered -Rtiered-disable-t0"},
            runtime_mode{"tiered_no_t2", "-Rtiered -Rtiered-disable-t2"},
            runtime_mode{"tiered_no_t0_no_t2", "-Rtiered -Rtiered-disable-t0 -Rtiered-disable-t2"}};

        for(auto const& mode: runtime_modes)
        {
            auto const command{make_uwvm_command(uwvm_path, mode.args, wasm1p1_feature_args, wasm_path)};
            if(!run_command(command, mode.label)) [[unlikely]] { return false; }
        }

        auto const explicit_feature_command{make_uwvm_command(uwvm_path, "-Rjit", wasm1p1_explicit_feature_args, wasm_path)};
        return run_command(explicit_feature_command, "explicit wasm1p1 feature subset");
    }

    [[nodiscard]] bool run_feature_gate_check(::std::filesystem::path const& uwvm_path, ::std::filesystem::path const& wasm_path)
    {
        auto const command{make_uwvm_command(uwvm_path, "-Rcm full -Rcc jit", "--runtime-llvm-jit-cache-path disable", wasm_path)};
        return run_expect_failure(command, "missing wasm1p1 feature flags");
    }

    [[nodiscard]] bool run_policy_matrix(::std::filesystem::path const& uwvm_path, ::std::filesystem::path const& wasm_path)
    {
        for(::std::string_view policy:
            {::std::string_view{"debug"}, ::std::string_view{"legacy-light"}, ::std::string_view{"pb-o1"}, ::std::string_view{"pb-o2"}, ::std::string_view{"pb-o3"}})
        {
            auto extra_args{::std::string{wasm1p1_feature_args} + " --runtime-llvm-jit-full-policy " + ::std::string{policy}};
            auto const command{make_uwvm_command(uwvm_path, "-Rcm full -Rcc jit", extra_args, wasm_path)};
            if(!run_command(command, (::std::string{"full-policy-"} + ::std::string{policy}).c_str())) [[unlikely]] { return false; }
        }

        for(::std::string_view policy: {::std::string_view{"debug"}, ::std::string_view{"light"}, ::std::string_view{"balanced"}})
        {
            auto extra_args{::std::string{wasm1p1_feature_args} + " --runtime-llvm-jit-lazy-policy " + ::std::string{policy}};
            auto const command{make_uwvm_command(uwvm_path, "-Rjit", extra_args, wasm_path)};
            if(!run_command(command, (::std::string{"lazy-policy-"} + ::std::string{policy}).c_str())) [[unlikely]] { return false; }
        }

        for(::std::string_view policy:
            {::std::string_view{"debug"}, ::std::string_view{"default"}, ::std::string_view{"fast-compile"}, ::std::string_view{"balanced"}, ::std::string_view{"max"}})
        {
            auto extra_args{::std::string{wasm1p1_feature_args} + " --runtime-llvm-jit-policy " + ::std::string{policy}};
            auto const command{make_uwvm_command(uwvm_path, "-Rtiered", extra_args, wasm_path)};
            if(!run_command(command, (::std::string{"tiered-policy-"} + ::std::string{policy}).c_str())) [[unlikely]] { return false; }
        }

        return true;
    }

    [[nodiscard]] bool run_call_stack_matrix(::std::filesystem::path const& uwvm_path, ::std::filesystem::path const& wasm_path)
    {
        for(::std::string_view policy: {::std::string_view{"instruction"}, ::std::string_view{"none"}})
        {
            auto extra_args{::std::string{wasm1p1_feature_args} + " --runtime-llvm-jit-call-stack " + ::std::string{policy}};
            auto const command{make_uwvm_command(uwvm_path, "-Rcm full -Rcc jit", extra_args, wasm_path)};
            if(!run_command(command, (::std::string{"call-stack-"} + ::std::string{policy}).c_str())) [[unlikely]] { return false; }
        }

        for(::std::string_view policy: {::std::string_view{"unwind"}, ::std::string_view{"unwind-uncheck"}})
        {
            auto extra_args{::std::string{wasm1p1_feature_args} + " --runtime-llvm-jit-call-stack " + ::std::string{policy}};
            auto const command{make_uwvm_command(uwvm_path, "-Rcm full -Rcc jit", extra_args, wasm_path)};
            if(run_system_command(command) != 0)
            {
                ::std::cout << "[llvm_jit_wasm1p1_cpp] skip unsupported call-stack policy: " << policy << '\n';
                continue;
            }
            ::std::cout << "[llvm_jit_wasm1p1_cpp] " << command << '\n';
        }

        return true;
    }

    [[nodiscard]] bool cache_directory_has_entries(::std::filesystem::path const& cache_dir)
    {
        ::std::error_code ec{};
        if(!::std::filesystem::exists(cache_dir, ec) || ec) { return false; }
        return ::std::filesystem::recursive_directory_iterator{cache_dir, ec} != ::std::filesystem::recursive_directory_iterator{};
    }

    [[nodiscard]] bool run_cache_matrix(::std::filesystem::path const& uwvm_path,
                                        ::std::filesystem::path const& artifact_dir,
                                        ::std::filesystem::path const& wasm_path)
    {
        auto const cache_dir{artifact_dir / "llvm-cache"};
        ::std::error_code ec{};
        ::std::filesystem::remove_all(cache_dir, ec);
        ec.clear();
        ::std::filesystem::create_directories(cache_dir, ec);
        if(ec) [[unlikely]]
        {
            ::std::cerr << "failed to create cache directory " << cache_dir << ": " << ec.message() << '\n';
            return false;
        }

        auto const cache_args{::std::string{"--runtime-llvm-jit-cache-path path "} + quote_argument(cache_dir) + " --wasm-feature-wasm1.1"};
        for(auto const& mode: {::std::pair{::std::string_view{"cached-full"}, ::std::string_view{"-Rcm full -Rcc jit"}},
                               ::std::pair{::std::string_view{"cached-lazy-verification"}, ::std::string_view{"-Rcm lazy+verification -Rcc jit"}},
                               ::std::pair{::std::string_view{"cached-tiered"}, ::std::string_view{"-Rtiered"}}})
        {
            auto const command{make_uwvm_command(uwvm_path, mode.second, cache_args, wasm_path)};
            if(!run_command(command, ::std::string{mode.first}.c_str())) [[unlikely]] { return false; }
        }

        if(!cache_directory_has_entries(cache_dir)) [[unlikely]]
        {
            ::std::cerr << "LLVM JIT cache directory stayed empty after cached wasm1p1 runs: " << cache_dir << '\n';
            return false;
        }

        return true;
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

    auto const project_root{find_project_root(executable_dir)};
    if(project_root.empty()) [[unlikely]]
    {
        ::std::cerr << "failed to locate project root from test executable: " << executable << '\n';
        return 1;
    }

    auto const clangxx{find_clangxx(project_root)};
    if(!command_available(clangxx))
    {
        ::std::cout << "[llvm_jit_wasm1p1_cpp] skip: no wasm-capable clang++ found; set UWVM_WASM_CLANGXX to enable this test\n";
        return 0;
    }

    auto const sysroot{find_wasi_sysroot(project_root)};
    if(sysroot.empty() || !::std::filesystem::exists(sysroot))
    {
        ::std::cout << "[llvm_jit_wasm1p1_cpp] skip: WASI sysroot not found; set UWVM_WASI_SYSROOT to enable this test\n";
        return 0;
    }

    auto const artifact_dir{executable_dir / "test-artifacts" / "0014.llvm_jit_wasm1p1_clang_cpp_matrix"};
    auto const source_path{artifact_dir / "fft.cc"};
    auto const wasm_path{artifact_dir / "fft.wasm"};

    if(!write_text_file(source_path, wasm1p1_cpp_source)) [[unlikely]] { return 1; }
    if(!compile_wasm1p1_cpp(clangxx, sysroot, source_path, wasm_path)) [[unlikely]] { return 1; }
    if(!run_feature_gate_check(uwvm_path, wasm_path)) [[unlikely]] { return 1; }
    if(!run_execution_matrix(uwvm_path, wasm_path)) [[unlikely]] { return 1; }
    if(!run_policy_matrix(uwvm_path, wasm_path)) [[unlikely]] { return 1; }
    if(!run_call_stack_matrix(uwvm_path, wasm_path)) [[unlikely]] { return 1; }
    if(!run_cache_matrix(uwvm_path, artifact_dir, wasm_path)) [[unlikely]] { return 1; }

    return 0;
}
