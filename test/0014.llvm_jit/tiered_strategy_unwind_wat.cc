#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    struct strategy_case_t
    {
        char const* name;
        char const* args;
        ::std::vector<::std::size_t> expected_funcs;
        ::std::vector<::std::string_view> required_log_patterns;
        ::std::vector<::std::string_view> forbidden_log_patterns;
        ::std::string wat;
    };

    struct run_result_t
    {
        bool valid{};
        ::std::vector<::std::size_t> func_indices{};
        ::std::filesystem::path output_path{};
        ::std::filesystem::path log_path{};
    };

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

    [[nodiscard]] bool command_succeeds(::std::string const& command)
    {
        return run_system_command(command) == 0;
    }

    [[nodiscard]] bool read_text_file(::std::filesystem::path const& path, ::std::string& text)
    {
        ::std::ifstream input(path);
        if(!input)
        {
            ::std::cerr << "failed to open text file: " << path << '\n';
            return false;
        }

        text.assign(::std::istreambuf_iterator<char>{input}, ::std::istreambuf_iterator<char>{});
        if(input.bad())
        {
            ::std::cerr << "failed to read text file: " << path << '\n';
            return false;
        }

        return true;
    }

    [[nodiscard]] bool write_text_file(::std::filesystem::path const& path, ::std::string_view text)
    {
        ::std::error_code ec{};
        ::std::filesystem::create_directories(path.parent_path(), ec);
        if(ec)
        {
            ::std::cerr << "failed to create output directory: " << path.parent_path() << '\n';
            return false;
        }

        ::std::ofstream output(path, ::std::ios::binary | ::std::ios::trunc);
        if(!output)
        {
            ::std::cerr << "failed to open text output: " << path << '\n';
            return false;
        }

        output.write(text.data(), static_cast<::std::streamsize>(text.size()));
        if(!output)
        {
            ::std::cerr << "failed to write text output: " << path << '\n';
            return false;
        }

        return true;
    }

    [[nodiscard]] ::std::filesystem::path find_parent_with(::std::filesystem::path dir, ::std::filesystem::path const& child)
    {
        for(;;)
        {
            if(::std::filesystem::exists(dir / child)) { return dir; }
            if(dir == dir.root_path()) { return {}; }
            dir = dir.parent_path();
        }
    }

    [[nodiscard]] ::std::filesystem::path find_uwvm_binary(::std::filesystem::path dir)
    {
        for(;;)
        {
            auto const candidate{dir / "uwvm"};
            if(::std::filesystem::exists(candidate)) { return candidate; }
#ifdef _WIN32
            auto const windows_candidate{dir / "uwvm.exe"};
            if(::std::filesystem::exists(windows_candidate)) { return windows_candidate; }
#endif
            if(dir == dir.root_path()) { return {}; }
            dir = dir.parent_path();
        }
    }

    [[nodiscard]] ::std::filesystem::path find_wat2wasm(::std::filesystem::path const& project_root)
    {
        if(auto const env{::std::getenv("WAT2WASM")}; env != nullptr && *env != '\0')
        {
            ::std::filesystem::path const p{env};
            if(::std::filesystem::exists(p)) { return p; }
        }

#ifdef _WIN32
        constexpr char const* name{"wat2wasm.exe"};
#else
        constexpr char const* name{"wat2wasm"};
#endif
        ::std::array candidates{
            project_root / "build" / "test" / "third-parties" / "wabt" / "build" / name,
            project_root / "build" / "test" / "third-parties" / "wabt" / "build" / "bin" / name,
            project_root / "build" / "test" / "third-parties" / "wabt" / "build" / "Release" / name,
            project_root / "build" / "test" / "third-parties" / "wabt" / "build-ninja" / name,
            project_root / "wabt" / "build" / name,
            project_root / "wabt" / "build" / "bin" / name,
            project_root / "wabt" / "build" / "Release" / name,
            project_root / "wabt" / "build-ninja" / name,
        };

        for(auto const& p: candidates)
        {
            if(::std::filesystem::exists(p)) { return p; }
        }

#ifdef _WIN32
        if(command_succeeds("wat2wasm --version > NUL 2>&1")) { return "wat2wasm"; }
#else
        if(command_succeeds("wat2wasm --version > /dev/null 2>&1")) { return "wat2wasm"; }
#endif
        return {};
    }

    [[nodiscard]] ::std::string strip_ansi_codes(::std::string_view text)
    {
        ::std::string out{};
        out.reserve(text.size());

        for(::std::size_t i{}; i != text.size();)
        {
            if(text[i] == '\x1b' && i + 1uz < text.size() && text[i + 1uz] == '[')
            {
                i += 2uz;
                while(i != text.size())
                {
                    auto const ch{text[i++]};
                    if(ch >= '@' && ch <= '~') { break; }
                }
                continue;
            }

            out.push_back(text[i++]);
        }

        return out;
    }

    [[nodiscard]] ::std::vector<::std::size_t> parse_func_indices(::std::string_view plain_output)
    {
        ::std::vector<::std::size_t> result{};
        constexpr ::std::string_view prefix{" func_idx="};
        ::std::size_t pos{};

        for(;;)
        {
            pos = plain_output.find(prefix, pos);
            if(pos == ::std::string_view::npos) { return result; }
            pos += prefix.size();

            while(pos != plain_output.size() && (plain_output[pos] < '0' || plain_output[pos] > '9')) { ++pos; }

            ::std::size_t value{};
            auto const value_begin{pos};
            while(pos != plain_output.size())
            {
                auto const ch{plain_output[pos]};
                if(ch < '0' || ch > '9') { break; }
                value = value * 10uz + static_cast<::std::size_t>(ch - '0');
                ++pos;
            }

            if(pos != value_begin) { result.push_back(value); }
        }
    }

    [[nodiscard]] bool compile_wat(::std::filesystem::path const& wat2wasm,
                                   ::std::filesystem::path const& wat_path,
                                   ::std::filesystem::path const& wasm_path)
    {
        auto const command{quote_argument(wat2wasm) + " " + quote_argument(wat_path) + " -o " + quote_argument(wasm_path)};
        ::std::cout << "[tiered-strategy] " << command << '\n';
        if(command_succeeds(command)) { return true; }

        ::std::cerr << "wat2wasm failed for " << wat_path << '\n';
        return false;
    }

    [[nodiscard]] run_result_t run_case(::std::filesystem::path const& uwvm_path,
                                        ::std::filesystem::path const& wasm_path,
                                        ::std::filesystem::path const& artifact_dir,
                                        strategy_case_t const& test_case,
                                        char const* policy)
    {
        auto const stem{::std::string{test_case.name} + "." + policy};
        auto const output_path{artifact_dir / (stem + ".out")};
        auto const log_path{artifact_dir / (stem + ".log")};
        auto const command{quote_argument(uwvm_path) + " " + test_case.args + " -Rllvm-call-stack " + policy + " -Rclog file " + quote_argument(log_path) +
                           " --run " + quote_argument(wasm_path)};
        auto const full_command{command + " > " + quote_argument(output_path) + " 2>&1"};
        ::std::cout << "[tiered-strategy] " << full_command << '\n';

        auto const status{run_system_command(full_command)};
        if(status == 0)
        {
            ::std::cerr << "strategy trap unexpectedly succeeded: " << stem << '\n';
            return {.output_path = output_path, .log_path = log_path};
        }

        ::std::string output{};
        if(!read_text_file(output_path, output)) { return {.output_path = output_path, .log_path = log_path}; }

        auto const plain_output{strip_ansi_codes(output)};
        auto funcs{parse_func_indices(plain_output)};
        auto const valid{funcs == test_case.expected_funcs};
        if(!valid)
        {
            ::std::cerr << "unexpected stack for strategy=" << test_case.name << " policy=" << policy << '\n';
            ::std::cerr << "  expected=[";
            for(::std::size_t i{}; i != test_case.expected_funcs.size(); ++i)
            {
                if(i != 0uz) { ::std::cerr << ','; }
                ::std::cerr << test_case.expected_funcs[i];
            }
            ::std::cerr << "] actual=[";
            for(::std::size_t i{}; i != funcs.size(); ++i)
            {
                if(i != 0uz) { ::std::cerr << ','; }
                ::std::cerr << funcs[i];
            }
            ::std::cerr << "] output=" << output_path << '\n';
        }

        return {.valid = valid, .func_indices = ::std::move(funcs), .output_path = output_path, .log_path = log_path};
    }

    [[nodiscard]] bool check_log_patterns(strategy_case_t const& test_case, ::std::filesystem::path const& log_path)
    {
        ::std::string log{};
        if(!read_text_file(log_path, log)) { return false; }

        bool ok{true};
        for(auto const pattern: test_case.required_log_patterns)
        {
            if(log.find(pattern) != ::std::string::npos) { continue; }
            ok = false;
            ::std::cerr << "missing required log pattern for strategy=" << test_case.name << ": " << pattern << '\n';
        }
        for(auto const pattern: test_case.forbidden_log_patterns)
        {
            if(log.find(pattern) == ::std::string::npos) { continue; }
            ok = false;
            ::std::cerr << "forbidden log pattern appeared for strategy=" << test_case.name << ": " << pattern << '\n';
        }

        if(!ok) { ::std::cerr << "  log=" << log_path << '\n'; }
        return ok;
    }

    [[nodiscard]] ::std::string make_nops(::std::size_t count)
    {
        ::std::string nops{};
        nops.reserve(count * 8uz);
        for(::std::size_t i{}; i != count; ++i) { nops += "    nop\n"; }
        return nops;
    }

    [[nodiscard]] ::std::string make_t0_fallback_wat()
    {
        return R"((module
  (type $v (func))
  (func $leaf (type $v) unreachable)
  (func $mid (type $v) call $leaf)
  (func $top (type $v) call $mid)
  (func $_start (export "_start") (type $v) call $top))
)";
    }

    [[nodiscard]] ::std::string make_tier1_inline_wat()
    {
        return ::std::string{R"((module
  (type $v (func))
  (type $i (func (param i32)))

  (func $hot (type $i) (param $trap i32)
    (local $i i32)
)"} + make_nops(120uz) + R"(    i32.const 0
    local.set $i
    block $exit
      loop $hot_loop
        local.get $i
        i32.const 8
        i32.ge_u
        br_if $exit
        local.get $i
        i32.const 1
        i32.add
        local.set $i
        br $hot_loop
      end
    end
    local.get $trap
    if
      unreachable
    end)

  (func $_start (export "_start") (type $v)
    (local $i i32)
    i32.const 0
    local.set $i
    block $exit
      loop $call_loop
        local.get $i
        i32.const 9000
        i32.ge_u
        br_if $exit
        i32.const 0
        call $hot
        local.get $i
        i32.const 1
        i32.add
        local.set $i
        br $call_loop
      end
    end
    i32.const 1
    call $hot))
)";
    }

    [[nodiscard]] ::std::string make_tier1_urgent_wat()
    {
        return R"((module
  (type $v (func))
  (type $i (func (param i32)))

  (func $hot (type $i) (param $trap i32)
    i32.const 1
    i32.const 2
    i32.add
    drop
    local.get $trap
    if
      unreachable
    end)

  (func $_start (export "_start") (type $v)
    (local $i i32)
    i32.const 0
    local.set $i
    block $exit
      loop $call_loop
        local.get $i
        i32.const 9000
        i32.ge_u
        br_if $exit
        i32.const 0
        call $hot
        local.get $i
        i32.const 1
        i32.add
        local.set $i
        br $call_loop
      end
    end
    i32.const 1
    call $hot))
)";
    }

    [[nodiscard]] ::std::vector<strategy_case_t> make_cases()
    {
        ::std::vector<strategy_case_t> cases{};
        cases.push_back({"t0_interpreter_fallback",
                         "-Rtiered",
                         {0uz, 1uz, 2uz, 3uz},
                         {"[uwvm-int-lazy] demand-request"},
                         {"[llvm-jit-lazy] tiered-demand-request", "[llvm-jit-lazy] tiered-osr-request", "[llvm-jit-lazy] tiered-full-request"},
                         make_t0_fallback_wat()});
        cases.push_back({"tier1_function_entry_inline",
                         "-Rtiered",
                         {0uz, 1uz},
                         {"[llvm-jit-lazy] tiered-demand-request", "lane=inline"},
                         {},
                         make_tier1_inline_wat()});
        cases.push_back({"tier1_function_entry_urgent",
                         "-Rtiered",
                         {0uz, 1uz},
                         {"[llvm-jit-lazy] tiered-demand-request", "lane=urgent"},
                         {},
                         make_tier1_urgent_wat()});
        cases.push_back({"no_t0_raw_entry",
                         "-Rtiered -Rtiered-disable-t0",
                         {0uz, 1uz, 2uz, 3uz},
                         {"[llvm-jit-lazy] demand-request", "lane=inline"},
                         {"[uwvm-int-lazy] demand-request"},
                         make_t0_fallback_wat()});
        return cases;
    }
}  // namespace

int main(int argc, char** argv)
{
    if(argc <= 0 || argv == nullptr || argv[0] == nullptr)
    {
        ::std::cerr << "missing argv[0]\n";
        return 1;
    }

    auto const executable{::std::filesystem::absolute(argv[0])};
    auto const executable_dir{executable.parent_path()};
    auto const project_root{find_parent_with(executable_dir, "xmake.lua")};
    if(project_root.empty())
    {
        ::std::cerr << "failed to locate project root from " << executable << '\n';
        return 1;
    }

    auto const uwvm_path{find_uwvm_binary(executable_dir)};
    if(uwvm_path.empty())
    {
        ::std::cerr << "failed to locate uwvm next to test executable: " << executable << '\n';
        return 1;
    }

    auto const wat2wasm_path{find_wat2wasm(project_root)};
    if(wat2wasm_path.empty())
    {
        ::std::cout << "[tiered-strategy] skip: wat2wasm not found; set WAT2WASM or put wat2wasm in PATH\n";
        return 0;
    }

    auto const artifact_dir{executable_dir / "test-artifacts" / "0014.llvm_jit" / "tiered_strategy_wat"};
    bool ok{true};
    for(auto const& test_case: make_cases())
    {
        auto const wat_path{artifact_dir / (::std::string{test_case.name} + ".wat")};
        auto const wasm_path{artifact_dir / (::std::string{test_case.name} + ".wasm")};
        if(!write_text_file(wat_path, test_case.wat)) { return 1; }
        if(!compile_wat(wat2wasm_path, wat_path, wasm_path)) { return 1; }

        auto const instruction{run_case(uwvm_path, wasm_path, artifact_dir, test_case, "instruction")};
        auto const unwind{run_case(uwvm_path, wasm_path, artifact_dir, test_case, "unwind")};
        if(!instruction.valid || !unwind.valid || instruction.func_indices != unwind.func_indices)
        {
            ok = false;
            ::std::cerr << "[tiered-strategy] stack mismatch or parse failure for " << test_case.name << '\n';
        }
        if(!check_log_patterns(test_case, unwind.log_path)) { ok = false; }
    }

    if(ok)
    {
        ::std::cout << "[tiered-strategy] all strategy call stacks and log patterns matched\n";
        return 0;
    }

    return 1;
}
