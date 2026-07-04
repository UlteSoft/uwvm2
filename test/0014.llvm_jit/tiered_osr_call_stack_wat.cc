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
        char const* wat_name;
        char const* wasm_name;
        char const* label;
    };

    struct mode_t
    {
        char const* name;
        char const* args;
        bool expect_osr_request;
    };

    struct run_result_t
    {
        bool valid{};
        ::std::vector<::std::size_t> func_indices{};
        ::std::filesystem::path output_path{};
        ::std::filesystem::path log_path{};
    };

    inline constexpr ::std::array fixtures{
        fixture_t{"tiered_osr_direct_trap.wat",        "tiered_osr_direct_trap.wasm",        "tiered_osr_direct"       },
        fixture_t{"tiered_osr_call_indirect_trap.wat", "tiered_osr_call_indirect_trap.wasm", "tiered_osr_call_indirect"},
    };

    inline constexpr ::std::array modes{
        mode_t{"tiered",       "-Rtiered",                     true },
        mode_t{"tiered_no_t0", "-Rtiered -Rtiered-disable-t0", false},
        mode_t{"tiered_no_t2", "-Rtiered -Rtiered-disable-t2", true },
    };

    [[nodiscard]] ::std::string quote_argument(::std::filesystem::path const& path)
    {
        return ::std::string{"\""} + path.string() + "\"";
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
            auto const candidate{dir / child};
            if(::std::filesystem::exists(candidate)) { return dir; }
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

    [[nodiscard]] ::std::string env_string(char const* name)
    {
        if(auto const env{::std::getenv(name)}; env != nullptr && *env != '\0') { return env; }
        return {};
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

    [[nodiscard]] ::std::filesystem::path find_wat2wasm(::std::filesystem::path const& project_root)
    {
        if(auto const env{::std::getenv("WAT2WASM")}; env != nullptr && *env != '\0')
        {
            ::std::filesystem::path const p{env};
            if(::std::filesystem::exists(p)) { return p; }
        }

        constexpr char const* names[]{
#ifdef _WIN32
            "wat2wasm.exe",
#else
            "wat2wasm",
#endif
        };

        for(auto const* name: names)
        {
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
        }

#ifdef _WIN32
        if(command_succeeds("wat2wasm --version > NUL 2>&1")) { return "wat2wasm"; }
#else
        if(command_succeeds("wat2wasm --version > /dev/null 2>&1")) { return "wat2wasm"; }
#endif
        return {};
    }

    [[nodiscard]] bool write_osr_padded_wat(::std::filesystem::path const& wat_path,
                                            ::std::filesystem::path const& generated_wat_path,
                                            char const* label)
    {
        ::std::string wat{};
        if(!read_text_file(wat_path, wat)) { return false; }

        constexpr ::std::string_view marker{";; UWVM_TIERED_OSR_PAD_NOPS"};
        auto const pos{wat.find(marker)};
        if(pos == ::std::string::npos)
        {
            ::std::cerr << "missing OSR padding marker in " << label << ": " << wat_path << '\n';
            return false;
        }

        ::std::string padded{};
        padded.reserve(wat.size() + 8192uz);
        padded.append(wat, 0uz, pos);
        for(::std::size_t i{}; i != 1600uz; ++i) { padded += "    nop\n"; }
        padded.append(wat, pos + marker.size(), ::std::string::npos);

        if(!write_text_file(generated_wat_path, padded)) { return false; }
        return true;
    }

    [[nodiscard]] bool compile_wat(::std::filesystem::path const& wat2wasm,
                                   ::std::filesystem::path const& wat_path,
                                   ::std::filesystem::path const& generated_wat_path,
                                   ::std::filesystem::path const& wasm_path,
                                   char const* label)
    {
        ::std::error_code ec{};
        ::std::filesystem::create_directories(wasm_path.parent_path(), ec);
        if(ec)
        {
            ::std::cerr << "failed to create wasm output directory for " << label << ": " << wasm_path.parent_path() << '\n';
            return false;
        }

        if(!write_osr_padded_wat(wat_path, generated_wat_path, label)) { return false; }

        auto const command{quote_argument(wat2wasm) + " " + quote_argument(generated_wat_path) + " -o " + quote_argument(wasm_path)};
        ::std::cout << "[tiered-osr-wat] " << command << '\n';
        if(command_succeeds(command)) { return true; }

        ::std::cerr << "wat2wasm failed for " << label << '\n';
        return false;
    }

    [[nodiscard]] bool run_trap_command(::std::string const& command, ::std::filesystem::path const& output_path, char const* label)
    {
        auto const full_command{command + " > " + quote_argument(output_path) + " 2>&1"};
        ::std::cout << "[tiered-osr-wat] " << full_command << '\n';

        auto const status{run_system_command(full_command)};
        if(status != 0) { return true; }

        ::std::cerr << "uwvm trap command unexpectedly returned success for " << label << '\n';
        return false;
    }

    [[nodiscard]] ::std::vector<::std::size_t> parse_func_indices(::std::string_view output)
    {
        ::std::vector<::std::size_t> result{};
        constexpr ::std::string_view prefix{" func_idx="};
        ::std::size_t pos{};

        for(;;)
        {
            pos = output.find(prefix, pos);
            if(pos == ::std::string_view::npos) { return result; }
            pos += prefix.size();

            auto digit_pos{pos};
            while(digit_pos != output.size() && output[digit_pos] != '\n')
            {
                auto const ch{output[digit_pos]};
                if(ch >= '0' && ch <= '9') { break; }
                ++digit_pos;
            }

            ::std::size_t value{};
            auto const value_begin{digit_pos};
            while(digit_pos != output.size())
            {
                auto const ch{output[digit_pos]};
                if(ch < '0' || ch > '9') { break; }
                value = value * 10uz + static_cast<::std::size_t>(ch - '0');
                ++digit_pos;
            }

            if(digit_pos != value_begin) { result.push_back(value); }
            pos = digit_pos == value_begin ? digit_pos + 1uz : digit_pos;
        }
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
                    if((ch >= '@' && ch <= '~')) { break; }
                }
                continue;
            }

            out.push_back(text[i++]);
        }

        return out;
    }

    [[nodiscard]] run_result_t read_trap_result(::std::filesystem::path const& output_path,
                                                ::std::filesystem::path const& log_path,
                                                char const* label,
                                                char const* mode,
                                                char const* policy)
    {
        ::std::string output{};
        if(!read_text_file(output_path, output)) { return {.output_path = output_path, .log_path = log_path}; }

        auto const plain_output{strip_ansi_codes(output)};
        auto func_indices{parse_func_indices(plain_output)};
        if(plain_output.find("Call stack:") != ::std::string::npos && plain_output.find(" module=") != ::std::string::npos &&
           func_indices == ::std::vector<::std::size_t>{0uz, 1uz, 2uz})
        {
            return {.valid = true, .func_indices = ::std::move(func_indices), .output_path = output_path, .log_path = log_path};
        }

        ::std::cerr << "missing tiered OSR trap call-stack frames in " << label << '/' << mode << '/' << policy << " output:\n" << output << '\n';
        return {.valid = false, .func_indices = ::std::move(func_indices), .output_path = output_path, .log_path = log_path};
    }

    [[nodiscard]] bool check_osr_log(::std::filesystem::path const& log_path,
                                     char const* label,
                                     char const* mode,
                                     char const* policy,
                                     bool expect_osr_request)
    {
        ::std::string log{};
        if(!read_text_file(log_path, log)) { return false; }

        if(!expect_osr_request || log.find("tiered-osr-request") != ::std::string::npos) { return true; }

        ::std::cerr << "missing tiered OSR request evidence in " << label << '/' << mode << '/' << policy << " log:\n" << log << '\n';
        return false;
    }

    [[nodiscard]] run_result_t run_trap_case(::std::filesystem::path const& uwvm_path,
                                             ::std::filesystem::path const& wasm_path,
                                             ::std::filesystem::path const& artifact_dir,
                                             fixture_t const& fixture,
                                             mode_t const& mode,
                                             char const* policy)
    {
        auto const stem{::std::string{fixture.label} + "." + mode.name + "." + policy};
        auto const output_path{artifact_dir / (stem + ".out")};
        auto const log_path{artifact_dir / (stem + ".log")};
        auto command{quote_argument(uwvm_path) + " " + mode.args + " -Rllvm-cache-path disable -Rllvm-call-stack " + policy +
                     " -Rclog file " + quote_argument(log_path)};
        if(auto const extra_args{env_string("UWVM_LLVM_JIT_TEST_EXTRA_RUNTIME_ARGS")}; !extra_args.empty()) { command += " " + extra_args; }
        command += " --run " + quote_argument(wasm_path);

        if(!run_trap_command(command, output_path, stem.c_str())) { return {.output_path = output_path, .log_path = log_path}; }
        auto result{read_trap_result(output_path, log_path, fixture.label, mode.name, policy)};
        if(!check_osr_log(log_path, fixture.label, mode.name, policy, mode.expect_osr_request)) { result.valid = false; }
        return result;
    }

    [[nodiscard]] bool run_fixture(::std::filesystem::path const& uwvm_path,
                                   ::std::filesystem::path const& wat2wasm_path,
                                   ::std::filesystem::path const& wat_dir,
                                   ::std::filesystem::path const& artifact_dir,
                                   fixture_t const& fixture)
    {
        auto const wat_path{wat_dir / fixture.wat_name};
        auto const generated_wat_path{artifact_dir / (::std::string{fixture.label} + ".padded.wat")};
        auto const wasm_path{artifact_dir / fixture.wasm_name};

        if(!compile_wat(wat2wasm_path, wat_path, generated_wat_path, wasm_path, fixture.label)) { return false; }

        for(auto const& mode: modes)
        {
            auto const instruction{run_trap_case(uwvm_path, wasm_path, artifact_dir, fixture, mode, "instruction")};
            auto const unwind{run_trap_case(uwvm_path, wasm_path, artifact_dir, fixture, mode, "unwind")};
            if(!instruction.valid || !unwind.valid) { return false; }
            if(instruction.func_indices == unwind.func_indices) { continue; }

            ::std::cerr << "tiered OSR unwind stack mismatch for " << fixture.label << '/' << mode.name << '\n';
            ::std::cerr << "  instruction output: " << instruction.output_path << '\n';
            ::std::cerr << "  unwind output: " << unwind.output_path << '\n';
            return false;
        }

        return true;
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
    auto const project_root{find_parent_with(executable_dir, "test/0014.llvm_jit/wat/tiered_osr_direct_trap.wat")};
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
        ::std::cout << "[tiered-osr-wat] skip: wat2wasm not found; set WAT2WASM or put wat2wasm in PATH\n";
        return 0;
    }

    auto const wat_dir{project_root / "test" / "0014.llvm_jit" / "wat"};
    auto const artifact_dir{[](::std::filesystem::path const& dir) {
        if(auto const env{::std::getenv("UWVM_TIERED_OSR_ARTIFACT_DIR")}; env != nullptr && *env != '\0') { return ::std::filesystem::path{env}; }
        return dir / "test-artifacts" / "0014.llvm_jit" / "tiered_osr_wat";
    }(executable_dir)};

    for(auto const& fixture: fixtures)
    {
        if(!run_fixture(uwvm_path, wat2wasm_path, wat_dir, artifact_dir, fixture)) { return 1; }
    }

    return 0;
}
