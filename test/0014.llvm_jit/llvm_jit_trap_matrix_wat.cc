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
    struct fixture_t
    {
        char const* name;
        char const* wat_name;
    };

    struct mode_t
    {
        char const* name;
        char const* args;
    };

    struct run_result_t
    {
        bool valid{};
        ::std::string trap_kind{};
        ::std::vector<::std::size_t> func_indices{};
        ::std::filesystem::path output_path{};
    };

    inline constexpr ::std::array fixtures{
        fixture_t{"oob_load",           "oob_load.wat"          },
        fixture_t{"oob_store",          "oob_store.wat"         },
        fixture_t{"divide_zero",        "divide_zero.wat"       },
        fixture_t{"i64_divide_zero",    "i64_divide_zero.wat"   },
        fixture_t{"integer_overflow",   "integer_overflow.wat"  },
        fixture_t{"invalid_conversion", "invalid_conversion.wat"},
        fixture_t{"unreachable",        "unreachable.wat"       },
        fixture_t{"call_indirect_null", "call_indirect_null.wat"},
    };

    inline constexpr ::std::array modes{
        mode_t{"full",                "-Rcm full -Rcc jit"                         },
        mode_t{"aot",                 "-Raot"                                      },
        mode_t{"lazy",                "-Rjit"                                      },
        mode_t{"lazy_verification",   "-Rcm lazy+verification -Rcc jit"            },
        mode_t{"tiered",              "-Rtiered"                                   },
        mode_t{"tiered_no_t0",        "-Rtiered -Rtiered-disable-t0"               },
        mode_t{"tiered_no_t2",        "-Rtiered -Rtiered-disable-t2"               },
        mode_t{"tiered_no_t0_no_t2",  "-Rtiered -Rtiered-disable-t0 -Rtiered-disable-t2"},
    };

    inline constexpr ::std::array compare_policies{"unwind", "auto"};

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

    [[nodiscard]] bool command_succeeds(::std::string const& command)
    {
        return run_system_command(command) == 0;
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

    [[nodiscard]] ::std::string parse_trap_kind(::std::string_view plain_output)
    {
        constexpr ::std::string_view prefix{"Runtime crash ("};
        auto const begin{plain_output.find(prefix)};
        if(begin == ::std::string_view::npos) { return {}; }

        auto const value_begin{begin + prefix.size()};
        auto const value_end{plain_output.find(')', value_begin)};
        if(value_end == ::std::string_view::npos) { return {}; }

        return ::std::string{plain_output.substr(value_begin, value_end - value_begin)};
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
        ::std::error_code ec{};
        ::std::filesystem::create_directories(wasm_path.parent_path(), ec);
        if(ec)
        {
            ::std::cerr << "failed to create wasm directory: " << wasm_path.parent_path() << '\n';
            return false;
        }

        auto const command{quote_argument(wat2wasm) + " " + quote_argument(wat_path) + " -o " + quote_argument(wasm_path)};
        ::std::cout << "[trap-matrix] " << command << '\n';
        if(command_succeeds(command)) { return true; }

        ::std::cerr << "wat2wasm failed for " << wat_path << '\n';
        return false;
    }

    [[nodiscard]] run_result_t run_case(::std::filesystem::path const& uwvm_path,
                                        ::std::filesystem::path const& wasm_path,
                                        ::std::filesystem::path const& artifact_dir,
                                        fixture_t const& fixture,
                                        mode_t const& mode,
                                        char const* policy)
    {
        auto const stem{::std::string{fixture.name} + "." + mode.name + "." + policy};
        auto const output_path{artifact_dir / (stem + ".out")};
        auto const log_path{artifact_dir / (stem + ".log")};
        auto const command{quote_argument(uwvm_path) + " " + mode.args + " -Rllvm-call-stack " + policy + " -Rclog file " + quote_argument(log_path) +
                           " --run " + quote_argument(wasm_path)};
        auto const full_command{command + " > " + quote_argument(output_path) + " 2>&1"};
        ::std::cout << "[trap-matrix] " << full_command << '\n';

        auto const status{run_system_command(full_command)};
        if(status == 0)
        {
            ::std::cerr << "trap command unexpectedly succeeded: " << stem << '\n';
            return {.valid = false, .output_path = output_path};
        }

        ::std::string output{};
        if(!read_text_file(output_path, output)) { return {.valid = false, .output_path = output_path}; }

        auto const plain_output{strip_ansi_codes(output)};
        auto trap_kind{parse_trap_kind(plain_output)};
        auto func_indices{parse_func_indices(plain_output)};
        auto const valid{!trap_kind.empty() && !func_indices.empty()};
        if(!valid)
        {
            ::std::cerr << "failed to parse trap output for " << stem << ":\n" << output << '\n';
        }

        return {.valid = valid, .trap_kind = ::std::move(trap_kind), .func_indices = ::std::move(func_indices), .output_path = output_path};
    }

    [[nodiscard]] bool same_result(run_result_t const& a, run_result_t const& b)
    {
        return a.valid && b.valid && a.trap_kind == b.trap_kind && a.func_indices == b.func_indices;
    }

    void print_result(::std::ostream& out, run_result_t const& result)
    {
        out << "trap=\"" << result.trap_kind << "\" funcs=[";
        for(::std::size_t i{}; i != result.func_indices.size(); ++i)
        {
            if(i != 0uz) { out << ','; }
            out << result.func_indices[i];
        }
        out << "] output=" << result.output_path;
    }
}

int main(int argc, char** argv)
{
    if(argc <= 0 || argv == nullptr || argv[0] == nullptr)
    {
        ::std::cerr << "missing argv[0]\n";
        return 1;
    }

    auto const executable{::std::filesystem::absolute(argv[0])};
    auto const executable_dir{executable.parent_path()};
    auto const project_root{find_parent_with(executable_dir, "test/0014.llvm_jit/wat/trap_matrix/oob_load.wat")};
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
        ::std::cout << "[trap-matrix] skip: wat2wasm not found; set WAT2WASM or put wat2wasm in PATH\n";
        return 0;
    }

    auto const strict_env{::std::getenv("UWVM_TRAP_MATRIX_STRICT")};
    bool const strict{strict_env != nullptr && *strict_env != '\0' && ::std::string_view{strict_env} != "0"};
    auto const wat_dir{project_root / "test" / "0014.llvm_jit" / "wat" / "trap_matrix"};
    auto const artifact_dir{executable_dir / "test-artifacts" / "0014.llvm_jit" / "trap_matrix"};

    bool ok{true};
    ::std::size_t mismatch_count{};
    for(auto const& fixture: fixtures)
    {
        auto const wat_path{wat_dir / fixture.wat_name};
        auto const wasm_path{artifact_dir / (::std::string{fixture.name} + ".wasm")};
        if(!compile_wat(wat2wasm_path, wat_path, wasm_path)) { return 1; }

        for(auto const& mode: modes)
        {
            auto const instruction{run_case(uwvm_path, wasm_path, artifact_dir, fixture, mode, "instruction")};
            if(!instruction.valid)
            {
                ok = false;
                continue;
            }

            for(auto const* policy: compare_policies)
            {
                auto const compared{run_case(uwvm_path, wasm_path, artifact_dir, fixture, mode, policy)};
                if(same_result(instruction, compared)) { continue; }

                ++mismatch_count;
                ok = false;
                ::std::cerr << "[trap-matrix] mismatch fixture=" << fixture.name << " mode=" << mode.name << " policy=" << policy << '\n';
                ::std::cerr << "  instruction: ";
                print_result(::std::cerr, instruction);
                ::std::cerr << '\n';
                ::std::cerr << "  " << policy << ": ";
                print_result(::std::cerr, compared);
                ::std::cerr << '\n';
            }
        }
    }

    if(mismatch_count == 0uz)
    {
        ::std::cout << "[trap-matrix] all trap outputs matched instruction baselines\n";
        return 0;
    }

    ::std::cout << "[trap-matrix] mismatches=" << mismatch_count << (strict ? " strict=1\n" : " strict=0\n");
    return strict && !ok ? 1 : 0;
}
