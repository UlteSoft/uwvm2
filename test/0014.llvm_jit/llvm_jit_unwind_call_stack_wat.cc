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
    struct trap_case_t
    {
        char const* name;
        char const* body;
    };

    struct stack_shape_t
    {
        char const* name;
        ::std::vector<::std::size_t> expected_funcs;
        ::std::string (*make_wat)(::std::string_view);
    };

    struct mode_t
    {
        char const* name;
        char const* args;
    };

    struct run_result_t
    {
        bool valid{};
        ::std::vector<::std::size_t> func_indices{};
        ::std::filesystem::path output_path{};
    };

    inline constexpr ::std::array trap_cases{
        trap_case_t{"unreachable", "    unreachable\n"},
        trap_case_t{"divide_zero", R"(    i32.const 1
    i32.const 0
    i32.div_s
    drop
)"},
        trap_case_t{"float_to_int", R"(    f32.const nan
    i32.trunc_f32_s
    drop
)"},
        trap_case_t{"oob_load", R"(    i32.const -1
    i32.load
    drop
)"},
        trap_case_t{"oob_store", R"(    i32.const -1
    i64.const 1
    i64.store
)"},
    };

    inline constexpr ::std::array modes{
        mode_t{"full",         "-Rcm full -Rcc jit"          },
        mode_t{"lazy",         "-Rjit"                       },
        mode_t{"tiered",       "-Rtiered"                    },
        mode_t{"tiered_no_t0", "-Rtiered -Rtiered-disable-t0"},
        mode_t{"tiered_no_t2", "-Rtiered -Rtiered-disable-t2"},
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

    [[nodiscard]] ::std::filesystem::path env_path(char const* name)
    {
        if(auto const env{::std::getenv(name)}; env != nullptr && *env != '\0') { return env; }
        return {};
    }

    [[nodiscard]] ::std::string env_string(char const* name)
    {
        if(auto const env{::std::getenv(name)}; env != nullptr && *env != '\0') { return env; }
        return {};
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
        ::std::cout << "[llvm-jit-unwind-stack] " << command << '\n';
        if(command_succeeds(command)) { return true; }

        ::std::cerr << "wat2wasm failed for " << wat_path << '\n';
        return false;
    }

    [[nodiscard]] ::std::string make_deep_leaf_wat(::std::string_view trap_body)
    {
        return ::std::string{R"((module
  (memory 1)
  (global $sink (mut i32) (i32.const 0))
  (func $pad (param $base i32) (result i32)
    (local $i i32)
    local.get $base
    local.set $i
    block $exit
      loop $loop
        local.get $i
        i32.const 96
        i32.ge_u
        br_if $exit
        global.get $sink
        local.get $i
        i32.add
        global.set $sink
        local.get $i
        i32.const 1
        i32.add
        local.set $i
        br $loop
      end
    end
    global.get $sink)
  (func $leaf (param $trap i32)
    i32.const 0
    call $pad
    drop
    local.get $trap
    if
)"} + ::std::string{trap_body} + R"(    end)
  (func $mid (param $trap i32)
    i32.const 1
    call $pad
    drop
    local.get $trap
    call $leaf)
  (func $top (param $trap i32)
    i32.const 2
    call $pad
    drop
    local.get $trap
    call $mid)
  (func $_start (export "_start")
    i32.const 1
    call $top)
)
)";
    }

    [[nodiscard]] ::std::string make_post_call_trap_wat(::std::string_view trap_body)
    {
        return ::std::string{R"((module
  (memory 1)
  (global $sink (mut i32) (i32.const 0))
  (func $pad (param $base i32) (result i32)
    (local $i i32)
    local.get $base
    local.set $i
    block $exit
      loop $loop
        local.get $i
        i32.const 96
        i32.ge_u
        br_if $exit
        global.get $sink
        local.get $i
        i32.add
        global.set $sink
        local.get $i
        i32.const 1
        i32.add
        local.set $i
        br $loop
      end
    end
    global.get $sink)
  (func $helper (result i32)
    i32.const 3
    call $pad
    drop
    i32.const 7)
  (func $caller (param $trap i32)
    call $helper
    drop
    i32.const 4
    call $pad
    drop
    local.get $trap
    if
)"} + ::std::string{trap_body} + R"(    end)
  (func $_start (export "_start")
    i32.const 1
    call $caller)
)
)";
    }

    [[nodiscard]] ::std::string make_indirect_leaf_wat(::std::string_view trap_body)
    {
        return ::std::string{R"((module
  (type $v (func))
  (memory 1)
  (table 1 funcref)
  (elem (i32.const 0) $leaf)

  (func $leaf (type $v)
)"} + ::std::string{trap_body} + R"(  )

  (func $dispatch (type $v)
    i32.const 0
    call_indirect (type $v))

  (func $top (type $v) call $dispatch)
  (func $_start (export "_start") (type $v) call $top)
)
)";
    }

    [[nodiscard]] ::std::string make_recursive_countdown_wat(::std::string_view trap_body)
    {
        return ::std::string{R"((module
  (memory 1)

  (func $recurse (param $n i32)
    local.get $n
    if
      local.get $n
      i32.const 1
      i32.sub
      call $recurse
      return
    end
)"} + ::std::string{trap_body} + R"(  )

  (func $_start (export "_start")
    i32.const 3
    call $recurse)
)
)";
    }

    [[nodiscard]] ::std::string make_param_result_chain_wat(::std::string_view trap_body)
    {
        return ::std::string{R"((module
  (type $i2i (func (param i32) (result i32)))
  (memory 1)

  (func $leaf (type $i2i) (param $x i32) (result i32)
    local.get $x
    drop
)"} + ::std::string{trap_body} + R"(    i32.const 0)

  (func $mid (type $i2i) (param $x i32) (result i32)
    local.get $x
    call $leaf)

  (func $top (type $i2i) (param $x i32) (result i32)
    local.get $x
    call $mid)

  (func $_start (export "_start")
    i32.const 7
    call $top
    drop)
)
)";
    }

    [[nodiscard]] ::std::array<stack_shape_t, 5> make_shapes()
    {
        return {stack_shape_t{"deep_leaf", {1uz, 2uz, 3uz, 4uz}, make_deep_leaf_wat},
                stack_shape_t{"post_call_trap", {2uz, 3uz}, make_post_call_trap_wat},
                stack_shape_t{"indirect_leaf", {0uz, 1uz, 2uz, 3uz}, make_indirect_leaf_wat},
                stack_shape_t{"recursive_countdown", {0uz, 1uz}, make_recursive_countdown_wat},
                stack_shape_t{"param_result_chain", {0uz, 1uz, 2uz, 3uz}, make_param_result_chain_wat}};
    }

    [[nodiscard]] run_result_t run_case(::std::filesystem::path const& uwvm_path,
                                        ::std::string_view run_prefix,
                                        ::std::filesystem::path const& wasm_path,
                                        ::std::filesystem::path const& artifact_dir,
                                        char const* stem,
                                        mode_t const& mode,
                                        char const* policy)
    {
        auto const output_path{artifact_dir / (::std::string{stem} + "." + mode.name + "." + policy + ".out")};
        auto const command{(run_prefix.empty() ? ::std::string{} : ::std::string{run_prefix} + " ") + quote_argument(uwvm_path) + " " + mode.args +
                           " -Rllvm-cache-path disable -Rllvm-call-stack " + policy + " --run " + quote_argument(wasm_path)};
        auto const full_command{command + " > " + quote_argument(output_path) + " 2>&1"};
        ::std::cout << "[llvm-jit-unwind-stack] " << full_command << '\n';

        auto const status{run_system_command(full_command)};
        if(status == 0)
        {
            ::std::cerr << "trap command unexpectedly succeeded: " << stem << '/' << mode.name << '/' << policy << '\n';
            return {.output_path = output_path};
        }

        ::std::string output{};
        if(!read_text_file(output_path, output)) { return {.output_path = output_path}; }

        auto const plain_output{strip_ansi_codes(output)};
        auto funcs{parse_func_indices(plain_output)};
        auto const valid{plain_output.find("Runtime crash (") != ::std::string::npos && !funcs.empty()};
        if(!valid)
        {
            ::std::cerr << "failed to parse trap output for " << stem << '/' << mode.name << '/' << policy << ":\n" << output << '\n';
        }

        return {.valid = valid, .func_indices = ::std::move(funcs), .output_path = output_path};
    }

    void print_funcs(::std::ostream& out, ::std::vector<::std::size_t> const& funcs)
    {
        out << '[';
        for(::std::size_t i{}; i != funcs.size(); ++i)
        {
            if(i != 0uz) { out << ','; }
            out << funcs[i];
        }
        out << ']';
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

    auto const uwvm_path{[](::std::filesystem::path const& dir) {
        auto env_uwvm{env_path("UWVM")};
        if(!env_uwvm.empty()) { return env_uwvm; }
        return find_uwvm_binary(dir);
    }(executable_dir)};
    if(uwvm_path.empty())
    {
        ::std::cerr << "failed to locate uwvm next to test executable: " << executable << "; set UWVM to override\n";
        return 1;
    }
    if(!::std::filesystem::exists(uwvm_path))
    {
        ::std::cerr << "uwvm path does not exist: " << uwvm_path << '\n';
        return 1;
    }
    auto const run_prefix{env_string("UWVM_RUN_PREFIX")};

    auto const wat2wasm_path{find_wat2wasm(project_root)};
    if(wat2wasm_path.empty())
    {
        ::std::cout << "[llvm-jit-unwind-stack] skip: wat2wasm not found; set WAT2WASM or put wat2wasm in PATH\n";
        return 0;
    }

    auto const artifact_dir{[](::std::filesystem::path const& dir) {
        auto env_artifact_dir{env_path("UWVM_UNWIND_ARTIFACT_DIR")};
        if(!env_artifact_dir.empty()) { return env_artifact_dir; }
        return dir / "test-artifacts" / "0014.llvm_jit" / "unwind_call_stack_wat";
    }(executable_dir)};
    auto comparison_policy{env_string("UWVM_UNWIND_COMPARISON_POLICY")};
    if(comparison_policy.empty()) { comparison_policy = "unwind"; }
    bool ok{true};

    for(auto const& shape: make_shapes())
    {
        for(auto const& trap: trap_cases)
        {
            auto const stem{::std::string{shape.name} + "." + trap.name};
            auto const wat_path{artifact_dir / (stem + ".wat")};
            auto const wasm_path{artifact_dir / (stem + ".wasm")};
            if(!write_text_file(wat_path, shape.make_wat(trap.body))) { return 1; }
            if(!compile_wat(wat2wasm_path, wat_path, wasm_path)) { return 1; }

            for(auto const& mode: modes)
            {
                auto const instruction{run_case(uwvm_path, run_prefix, wasm_path, artifact_dir, stem.c_str(), mode, "instruction")};
                auto const unwind{run_case(uwvm_path, run_prefix, wasm_path, artifact_dir, stem.c_str(), mode, comparison_policy.c_str())};

                auto const instruction_matches_expected{instruction.valid && instruction.func_indices == shape.expected_funcs};
                if(!instruction_matches_expected)
                {
                    ok = false;
                    ::std::cerr << "[llvm-jit-unwind-stack] instruction baseline mismatch for " << stem << '/' << mode.name << " expected=";
                    print_funcs(::std::cerr, shape.expected_funcs);
                    ::std::cerr << " actual=";
                    print_funcs(::std::cerr, instruction.func_indices);
                    ::std::cerr << " output=" << instruction.output_path << '\n';
                }

                if(!unwind.valid || unwind.func_indices != shape.expected_funcs)
                {
                    ok = false;
                    ::std::cerr << "[llvm-jit-unwind-stack] unwind expected mismatch for " << stem << '/' << mode.name << " expected=";
                    print_funcs(::std::cerr, shape.expected_funcs);
                    ::std::cerr << " actual=";
                    print_funcs(::std::cerr, unwind.func_indices);
                    ::std::cerr << " output=" << unwind.output_path << '\n';
                }

                if(instruction.valid && unwind.valid && unwind.func_indices != instruction.func_indices)
                {
                    ok = false;
                    ::std::cerr << "[llvm-jit-unwind-stack] unwind mismatch for " << stem << '/' << mode.name << '\n';
                    ::std::cerr << "  instruction=";
                    print_funcs(::std::cerr, instruction.func_indices);
                    ::std::cerr << " output=" << instruction.output_path << '\n';
                    ::std::cerr << "  unwind=";
                    print_funcs(::std::cerr, unwind.func_indices);
                    ::std::cerr << " output=" << unwind.output_path << '\n';
                }
            }
        }
    }

    if(ok)
    {
        ::std::cout << "[llvm-jit-unwind-stack] all unwind call-stack WAT cases matched instruction baselines\n";
        return 0;
    }

    return 1;
}
