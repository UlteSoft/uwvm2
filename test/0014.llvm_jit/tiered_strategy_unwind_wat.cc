#include <array>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>
#include "native_unwind_test_policy.h"

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

    inline constexpr ::std::array comparison_policies{"unwind", "unwind-uncheck", "auto"};

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

    [[nodiscard]] bool probe_auto_call_stack_unwind(::std::filesystem::path const& uwvm_path,
                                                    ::std::filesystem::path const& wasm_path,
                                                    ::std::filesystem::path const& artifact_dir,
                                                    bool& auto_uses_authoritative_unwind,
                                                    bool& native_unwind_backend_available)
    {
        auto const output_path{artifact_dir / "default_call_stack_probe.out"};
        auto const log_path{artifact_dir / "default_call_stack_probe.log"};
        ::std::error_code ec{};
        ::std::filesystem::remove(log_path, ec);
        if(ec)
        {
            ::std::cerr << "failed to remove stale call-stack probe log: " << log_path << '\n';
            return false;
        }

        auto const command{quote_argument(uwvm_path) + " -Raot -Rllvm-cache-path disable -Rllvm-call-stack auto -Rclog file " +
                           quote_argument(log_path) + " --run " + quote_argument(wasm_path) + " > " + quote_argument(output_path) + " 2>&1"};
        ::std::cout << "[tiered-strategy] " << command << '\n';
        if(run_system_command(command) == 0)
        {
            ::std::cerr << "call-stack capability probe trap unexpectedly succeeded\n";
            return false;
        }

        ::std::string output{};
        if(!read_text_file(output_path, output)) { return false; }
        if(strip_ansi_codes(output).find("Runtime crash (") == ::std::string::npos)
        {
            ::std::cerr << "call-stack capability probe did not reach a runtime trap:\n" << output << '\n';
            return false;
        }

        ::std::string log{};
        if(!read_text_file(log_path, log)) { return false; }
        native_unwind_backend_available = log.find("unwind_backend=unwind.h") != ::std::string::npos ||
                                          log.find("unwind_backend=win64-seh") != ::std::string::npos;
        if(log.find("call_stack=unwind") != ::std::string::npos)
        {
            if(!::uwvm2test::native_unwind::matches_policy(log, "unwind") || log.find("unwind_check=live") == ::std::string::npos)
            {
                ::std::cerr << "auto selected native frame replacement without a successful live probe and omitted JIT logical frames:\n" << log << '\n';
                return false;
            }
            if(log.find("call_stack=instruction") != ::std::string::npos)
            {
                ::std::cerr << "auto unwind also enabled instruction-frame conversion:\n" << log << '\n';
                return false;
            }
            auto_uses_authoritative_unwind = true;
            return true;
        }
        if(log.find("call_stack=instruction") != ::std::string::npos)
        {
            auto const plain_output{strip_ansi_codes(output)};
            if(!::uwvm2test::native_unwind::matches_policy(log, "instruction") || plain_output.find(" func_idx=") == ::std::string::npos)
            {
                ::std::cerr << "auto call-stack policy did not preserve authoritative logical instruction frames:\n"
                            << log << "\noutput:\n"
                            << output << '\n';
                return false;
            }
            auto_uses_authoritative_unwind = false;
            return true;
        }

        ::std::cerr << "unable to determine default LLVM JIT call-stack policy from probe log:\n" << log << '\n';
        return false;
    }

    [[nodiscard]] bool policy_requests_native_unwind(::std::string_view policy, bool auto_uses_authoritative_unwind) noexcept
    {
        return policy == "unwind" || policy == "unwind-uncheck" || (policy == "auto" && auto_uses_authoritative_unwind);
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
        ::std::error_code ec{};
        ::std::filesystem::remove(log_path, ec);
        if(ec)
        {
            ::std::cerr << "failed to remove stale strategy log: " << log_path << '\n';
            return {.output_path = output_path, .log_path = log_path};
        }
        auto command{quote_argument(uwvm_path) + " " + test_case.args + " -Rllvm-call-stack " + policy + " -Rclog file " + quote_argument(log_path)};
        if(auto const extra_args{env_string("UWVM_LLVM_JIT_TEST_EXTRA_RUNTIME_ARGS")}; !extra_args.empty())
        {
            auto const case_args{::std::string_view{test_case.args}};
            auto const case_has_high_level_policy{case_args.find("-Rllvm-policy") != ::std::string_view::npos ||
                                                  case_args.find("--runtime-llvm-jit-policy") != ::std::string_view::npos};
            auto const extra_has_policy{extra_args.find("-Rllvm-policy") != ::std::string::npos ||
                                        extra_args.find("--runtime-llvm-jit-policy") != ::std::string::npos ||
                                        extra_args.find("-Rllvm-lazy-policy") != ::std::string::npos ||
                                        extra_args.find("--runtime-llvm-jit-lazy-policy") != ::std::string::npos ||
                                        extra_args.find("-Rllvm-full-policy") != ::std::string::npos ||
                                        extra_args.find("--runtime-llvm-jit-full-policy") != ::std::string::npos};
            if(!(case_has_high_level_policy && extra_has_policy))
            {
                command += " " + extra_args;
            }
        }
        command += " --run " + quote_argument(wasm_path);
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
        ::std::string log{};
        if(!read_text_file(log_path, log)) { return {.output_path = output_path, .log_path = log_path}; }

        auto const plain_output{strip_ansi_codes(output)};
        auto funcs{parse_func_indices(plain_output)};
        auto const reached_runtime_trap{plain_output.find("Runtime crash (") != ::std::string::npos};
        auto const stack_matches{funcs == test_case.expected_funcs};
        auto const valid{reached_runtime_trap && stack_matches};
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

    [[nodiscard]] bool check_call_stack_semantics(strategy_case_t const& test_case,
                                                  ::std::filesystem::path const& log_path,
                                                  char const* policy,
                                                  bool auto_uses_authoritative_unwind)
    {
        auto const policy_view{::std::string_view{policy}};
        if(policy_view == "instruction") { return true; }

        ::std::string log{};
        if(!read_text_file(log_path, log)) { return false; }

        // Lazy-only paths do not always emit a full-module policy record. Their exact frame chain and demand/OSR
        // lanes are checked independently; whenever a policy record exists, it must agree with the live-probed mode.
        if(log.find("call_stack=") == ::std::string::npos) { return true; }
        auto const expected{policy_view == "auto" ? (auto_uses_authoritative_unwind ? "unwind" : "instruction") : policy};
        if(!::uwvm2test::native_unwind::matches_policy(log, expected))
        {
            ::std::cerr << "native/instruction emission policy mismatch for strategy=" << test_case.name << " policy=" << policy
                        << "\n  log=" << log_path << '\n';
            return false;
        }

        return true;
    }

    [[nodiscard]] bool check_log_patterns(strategy_case_t const& test_case,
                                          ::std::filesystem::path const& log_path,
                                          ::std::string_view policy,
                                          bool auto_uses_authoritative_unwind)
    {
        ::std::string log{};
        if(!read_text_file(log_path, log)) { return false; }

        bool ok{true};
        auto const serialize_native_compilation{policy_requests_native_unwind(policy, auto_uses_authoritative_unwind)};
        bool expects_tiered_demand_or_osr{};
        bool expects_tiered_full{};
        for(auto const pattern: test_case.required_log_patterns)
        {
            if(pattern.find("tiered-demand-request") != ::std::string_view::npos || pattern.find("tiered-osr-request") != ::std::string_view::npos)
            {
                expects_tiered_demand_or_osr = true;
            }
            if(pattern.find("tiered-full-") != ::std::string_view::npos) { expects_tiered_full = true; }
            if(serialize_native_compilation &&
               (pattern == "lane=urgent" || pattern == "lane=normal" || pattern.find("tiered-full-") != ::std::string_view::npos))
            {
                continue;
            }
            if(log.find(pattern) != ::std::string::npos) { continue; }
            ok = false;
            ::std::cerr << "missing required log pattern for strategy=" << test_case.name << ": " << pattern << '\n';
        }
        if(serialize_native_compilation && expects_tiered_demand_or_osr && log.find("lane=inline") == ::std::string::npos)
        {
            ok = false;
            ::std::cerr << "native unwind did not serialize tiered demand/OSR compilation for strategy=" << test_case.name << '\n';
        }
        if(serialize_native_compilation && expects_tiered_full &&
           (log.find("[llvm-jit-lazy] tiered-full-request") != ::std::string::npos ||
            log.find("[llvm-jit-lazy] tiered-full-ready") != ::std::string::npos))
        {
            ok = false;
            ::std::cerr << "native unwind allowed background Tier 2 compilation for strategy=" << test_case.name << '\n';
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

    [[nodiscard]] ::std::string make_dummy_funcs(::std::size_t count)
    {
        ::std::string funcs{};
        funcs.reserve(count * 32uz);
        for(::std::size_t i{}; i != count; ++i)
        {
            funcs += "  (func $dummy_";
            funcs += ::std::to_string(i);
            funcs += " (type $v))\n";
        }
        return funcs;
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

    [[nodiscard]] ::std::string make_osr_lane_wat(::std::size_t dummy_count, ::std::size_t pad_nops, ::std::uint_least32_t loop_limit)
    {
        return ::std::string{R"((module
  (type $v (func))
  (type $i (func (param i32)))

  (func $leaf (type $i) (param $trap i32)
    local.get $trap
    if
      unreachable
    end)

  (func $loop_then_trap (type $i) (param $trap i32)
    (local $i i32)
)"} + make_nops(pad_nops) + R"(    i32.const 0
    local.set $i
    block $exit
      loop $hot_loop
        local.get $i
        i32.const )" + ::std::to_string(loop_limit) + R"(
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
    call $leaf)

  (func $_start (export "_start") (type $v)
    i32.const 1
    call $loop_then_trap)
)" + make_dummy_funcs(dummy_count) + R"()
)";
    }

    [[nodiscard]] ::std::string make_tier2_full_direct_wat(::std::string_view leaf_trap_body)
    {
        return ::std::string{R"((module
  (type $v (func))
  (type $i (func (param i32)))
  (memory 1)

  (func $leaf (type $i) (param $trap i32)
)"} + ::std::string{leaf_trap_body} + R"(
  )

  (func $_start (export "_start") (type $v)
    (local $i i32)
    i32.const 0
    local.set $i
    block $exit
      loop $hot
        local.get $i
        i32.const 2000000
        i32.ge_u
        br_if $exit
        i32.const 0
        call $leaf
        local.get $i
        i32.const 1
        i32.add
        local.set $i
        br $hot
      end
    end
    i32.const 1
    call $leaf))
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
        cases.push_back({"tiered_loop_osr_inline",
                         "-Rtiered -Rtiered-disable-t2",
                         {0uz, 1uz, 2uz},
                         {"[llvm-jit-lazy] tiered-osr-request", "lane=inline"},
                         {},
                         make_osr_lane_wat(14uz, 1600uz, 350000u)});
        cases.push_back({"tiered_loop_osr_urgent",
                         "-Rtiered -Rtiered-disable-t2",
                         {0uz, 1uz, 2uz},
                         {"[llvm-jit-lazy] tiered-osr-request", "lane=urgent"},
                         {},
                         make_osr_lane_wat(510uz, 4096uz, 500000u)});
        cases.push_back({"tiered_loop_osr_normal",
                         "-Rtiered -Rtiered-disable-t2",
                         {0uz, 1uz, 2uz},
                         {"[llvm-jit-lazy] tiered-osr-request", "lane=normal"},
                         {},
                         make_osr_lane_wat(130uz, 32uz, 12000000u)});
        // These cases verify Tier 2 compilation/materialization publication plus the final trap and logical stack.
        // A `tiered-full-ready` record does not prove that the trapping invocation entered the published Tier 2 code.
        auto const add_full_ready_case{[&](char const* name, ::std::string_view leaf_trap_body)
                                       {
                                           cases.push_back({name,
                                                            "-Rtiered -Rct 2 -Rllvm-policy max",
                                                            {0uz, 1uz},
                                                            {"[llvm-jit-lazy] tiered-full-request", "[llvm-jit-lazy] tiered-full-ready"},
                                                            {},
                                                            make_tier2_full_direct_wat(leaf_trap_body)});
                                       }};

        add_full_ready_case("tiered_full_ready_unreachable",
                            R"(    local.get $trap
    if
      unreachable
    end)");
        add_full_ready_case("tiered_full_ready_i32_divide_zero",
                            R"(    local.get $trap
    if
      i32.const 1
      i32.const 0
      i32.div_s
      drop
    end)");
        add_full_ready_case("tiered_full_ready_i64_integer_overflow",
                            R"(    local.get $trap
    if
      i64.const -9223372036854775808
      i64.const -1
      i64.div_s
      drop
    end)");
        add_full_ready_case("tiered_full_ready_invalid_conversion",
                            R"(    local.get $trap
    if
      f32.const nan
      i32.trunc_f32_s
      drop
    end)");
        add_full_ready_case("tiered_full_ready_oob_load",
                            R"(    local.get $trap
    if
      i32.const -1
      i32.load
      drop
    end)");
        add_full_ready_case("tiered_full_ready_oob_store",
                            R"(    local.get $trap
    if
      i32.const -1
      i64.const 1
      i64.store
    end)");
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

    auto const artifact_dir{[](::std::filesystem::path const& dir) {
        if(auto const env{::std::getenv("UWVM_TIERED_STRATEGY_ARTIFACT_DIR")}; env != nullptr && *env != '\0') { return ::std::filesystem::path{env}; }
        return dir / "test-artifacts" / "0014.llvm_jit" / "tiered_strategy_wat";
    }(executable_dir)};

    bool ok{true};
    bool call_stack_capability_probed{};
    bool auto_uses_authoritative_unwind{};
    bool native_unwind_backend_available{};
    auto const case_filter{::std::getenv("UWVM_TIERED_STRATEGY_CASE")};
    ::std::size_t selected_cases{};
    for(auto const& test_case: make_cases())
    {
        if(case_filter != nullptr && *case_filter != '\0' && ::std::string_view{test_case.name} != case_filter) { continue; }
        ++selected_cases;
        auto const wat_path{artifact_dir / (::std::string{test_case.name} + ".wat")};
        auto const wasm_path{artifact_dir / (::std::string{test_case.name} + ".wasm")};
        if(!write_text_file(wat_path, test_case.wat)) { return 1; }
        if(!compile_wat(wat2wasm_path, wat_path, wasm_path)) { return 1; }

        if(!call_stack_capability_probed)
        {
            if(!probe_auto_call_stack_unwind(
                   uwvm_path, wasm_path, artifact_dir, auto_uses_authoritative_unwind, native_unwind_backend_available))
            {
                return 1;
            }
            call_stack_capability_probed = true;
            if(!auto_uses_authoritative_unwind)
            {
                ::std::cout << "[tiered-strategy] checked native unwind is unavailable; auto retains logical instruction frames\n";
            }
        }

        auto const instruction{run_case(uwvm_path, wasm_path, artifact_dir, test_case, "instruction")};
        if(!instruction.valid)
        {
            ok = false;
            ::std::cerr << "[tiered-strategy] instruction baseline parse failure for " << test_case.name << '\n';
        }
        if(!check_log_patterns(test_case, instruction.log_path, "instruction", auto_uses_authoritative_unwind)) { ok = false; }

        for(auto const* policy: comparison_policies)
        {
            auto const policy_view{::std::string_view{policy}};
            if(policy_view == "unwind" && !auto_uses_authoritative_unwind) { continue; }
            if(policy_view == "unwind-uncheck" && !native_unwind_backend_available) { continue; }
            auto const compared{run_case(uwvm_path, wasm_path, artifact_dir, test_case, policy)};
            if(!instruction.valid || !compared.valid || instruction.func_indices != compared.func_indices)
            {
                ok = false;
                ::std::cerr << "[tiered-strategy] stack mismatch or parse failure for " << test_case.name << '/' << policy << '\n';
            }
            if(!check_log_patterns(test_case, compared.log_path, policy_view, auto_uses_authoritative_unwind)) { ok = false; }
            if(!check_call_stack_semantics(test_case, compared.log_path, policy, auto_uses_authoritative_unwind)) { ok = false; }
        }
    }

    if(selected_cases == 0uz)
    {
        ::std::cerr << "[tiered-strategy] no case matched UWVM_TIERED_STRATEGY_CASE\n";
        return 1;
    }
    if(ok)
    {
        ::std::cout << "[tiered-strategy] all strategy call stacks and log patterns matched\n";
        return 0;
    }

    return 1;
}
