// Share the exact trap/stack oracle, without restoring ROS's removed modes.
#define UWVM2TEST_TRAP_FULL_ONLY 1
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
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
    struct fixture_t
    {
        char const* name;
        char const* wat_name;
        char const* expected_trap_kind;
        ::std::array<::std::size_t, 4uz> expected_func_indices;
        ::std::size_t expected_func_count;
    };

    struct mode_t
    {
        ::std::string name;
        ::std::string args;
    };

    struct base_mode_t
    {
        ::std::string_view name;
        ::std::string_view args;
    };

    struct run_result_t
    {
        bool valid{};
        ::std::string trap_kind{};
        ::std::vector<::std::size_t> func_indices{};
        ::std::filesystem::path output_path{};
        ::std::filesystem::path log_path{};
    };

    inline constexpr ::std::uint_least64_t default_policy_seed{0x9e3779b97f4a7c15ULL};
    inline constexpr ::std::size_t seeded_policy_mode_count{10uz};
    inline constexpr ::std::array<::std::size_t, 4uz> standard_stack{0uz, 1uz, 2uz, 3uz};
    inline constexpr ::std::array<::std::size_t, 4uz> shifted_stack{1uz, 2uz, 3uz, 4uz};
    inline constexpr ::std::array<::std::size_t, 4uz> short_stack{0uz, 1uz, 2uz, 0uz};
    inline constexpr ::std::array<::std::size_t, 4uz> shifted_short_stack{1uz, 2uz, 3uz, 0uz};

    // OOM is deliberately absent: allocation failure is outside the Wasm trap model exercised by this matrix.
    inline constexpr ::std::array fixtures{
        fixture_t{"oob_unsigned_sum_load64",          "oob_unsigned_sum_load64.wat",          "memory access out of bounds",               standard_stack,      4uz},
        fixture_t{"oob_unsigned_sum_store64",         "oob_unsigned_sum_store64.wat",         "memory access out of bounds",               standard_stack,      4uz},
        fixture_t{"oob_unaligned_cross_end64",        "oob_unaligned_cross_end64.wat",        "memory access out of bounds",               standard_stack,      4uz},
        fixture_t{"oob_load",                        "oob_load.wat",                        "memory access out of bounds",               standard_stack,      4uz},
        fixture_t{"oob_store",                       "oob_store.wat",                       "memory access out of bounds",               standard_stack,      4uz},
        fixture_t{"oob_load8_s",                     "oob_load8_s.wat",                     "memory access out of bounds",               standard_stack,      4uz},
        fixture_t{"oob_store64",                     "oob_store64.wat",                     "memory access out of bounds",               standard_stack,      4uz},
        fixture_t{"divide_zero",                     "divide_zero.wat",                     "integer divide by zero",                    standard_stack,      4uz},
        fixture_t{"i32_divide_zero_u",               "i32_divide_zero_u.wat",               "integer divide by zero",                    standard_stack,      4uz},
        fixture_t{"i64_divide_zero",                 "i64_divide_zero.wat",                 "integer divide by zero",                    standard_stack,      4uz},
        fixture_t{"integer_overflow",                "integer_overflow.wat",                "integer overflow",                          standard_stack,      4uz},
        fixture_t{"i64_integer_overflow",            "i64_integer_overflow.wat",            "integer overflow",                          standard_stack,      4uz},
        fixture_t{"invalid_conversion",              "invalid_conversion.wat",              "invalid conversion to integer",             standard_stack,      4uz},
        fixture_t{"invalid_conversion_u32_nan",      "invalid_conversion_u32_nan.wat",      "invalid conversion to integer",             standard_stack,      4uz},
        fixture_t{"invalid_conversion_f64_i64",      "invalid_conversion_f64_i64.wat",      "invalid conversion to integer",             standard_stack,      4uz},
        fixture_t{"invalid_conversion_f32_overflow", "invalid_conversion_f32_overflow.wat", "integer overflow",                          standard_stack,      4uz},
        fixture_t{"invalid_conversion_f64_overflow", "invalid_conversion_f64_overflow.wat", "integer overflow",                          standard_stack,      4uz},
        fixture_t{"invalid_conversion_u64_overflow", "invalid_conversion_u64_overflow.wat", "integer overflow",                          standard_stack,      4uz},
        fixture_t{"unreachable",                     "unreachable.wat",                     "catch unreachable",                         standard_stack,      4uz},
        fixture_t{"call_indirect_null",              "call_indirect_null.wat",              "call_indirect: uninitialized element",       standard_stack,      4uz},
        fixture_t{"call_indirect_oob",               "call_indirect_oob.wat",               "call_indirect: table index out of bounds",   standard_stack,      4uz},
        fixture_t{"call_indirect_type",              "call_indirect_type.wat",              "call_indirect: signature mismatch",          shifted_stack,       4uz},
        fixture_t{"wasm2_memory_init_oob",           "wasm2_memory_init_oob.wat",           "memory access out of bounds",               short_stack,         3uz},
        fixture_t{"wasm2_memory_copy_oob",           "wasm2_memory_copy_oob.wat",           "memory access out of bounds",               short_stack,         3uz},
        fixture_t{"wasm2_table_get_oob",             "wasm2_table_get_oob.wat",             "table access out of bounds",                short_stack,         3uz},
        fixture_t{"wasm2_table_init_oob",            "wasm2_table_init_oob.wat",            "table access out of bounds",                shifted_short_stack, 3uz},
        fixture_t{"wasm2_table_copy_oob",            "wasm2_table_copy_oob.wat",            "table access out of bounds",                short_stack,         3uz},
    };

    // Each seeded policy mode is paired with one representative trap. The complete trap set still runs through every
    // base mode, while this deterministic sampling avoids multiplying the already-large subprocess matrix unnecessarily.
    inline constexpr ::std::array<::std::string_view, seeded_policy_mode_count> seeded_policy_fixtures{
        "unreachable",
        "invalid_conversion",
        "invalid_conversion_f32_overflow",
        "divide_zero",
        "integer_overflow",
        "oob_load",
        "call_indirect_null",
        "call_indirect_type",
        "wasm2_memory_copy_oob",
        "wasm2_table_init_oob"};

    inline constexpr ::std::array base_modes{
        base_mode_t{"full",                "-Rcm full -Rcc jit"                         },
        base_mode_t{"aot",                 "-Raot"                                      },
        base_mode_t{"lazy",                "-Rjit"                                      },
        base_mode_t{"lazy_verification",   "-Rcm lazy+verification -Rcc jit"            },
        base_mode_t{"tiered",              "-Rtiered"                                   },
        base_mode_t{"tiered_no_t0",        "-Rtiered -Rtiered-disable-t0"               },
        base_mode_t{"tiered_no_t2",        "-Rtiered -Rtiered-disable-t2"               },
        base_mode_t{"tiered_no_t0_no_t2",  "-Rtiered -Rtiered-disable-t0 -Rtiered-disable-t2"},
    };

    inline constexpr ::std::array compare_policies{"unwind", "unwind-uncheck", "auto"};

    [[nodiscard]] bool exhaustive_policies() noexcept
    {
        auto const* value{::std::getenv("UWVM_TRAP_MATRIX_ALL_POLICIES")};
        return value != nullptr && ::std::string_view{value} == "1";
    }

    [[nodiscard]] bool full_only() noexcept
    {
#ifdef UWVM2TEST_TRAP_FULL_ONLY
        return true; // ROS exposes the full JIT through -Raot, not -Rcm/-Rcc.
#else
        auto const* value{::std::getenv("UWVM_TRAP_MATRIX_FULL_ONLY")};
        return value != nullptr && ::std::string_view{value} == "1";
#endif
    }

    [[nodiscard]] ::std::uint_least64_t policy_seed() noexcept
    {
        auto seed{default_policy_seed};
        if(auto const env{::std::getenv("UWVM_LLVM_JIT_POLICY_SEED")}; env != nullptr && *env != '\0')
        {
            char* end{};
            auto const parsed{::std::strtoull(env, &end, 0)};
            if(end != env && end != nullptr && *end == '\0') { seed = static_cast<::std::uint_least64_t>(parsed); }
        }
        return seed == 0u ? default_policy_seed : seed;
    }

    [[nodiscard]] ::std::uint_least64_t next_policy_random(::std::uint_least64_t& state) noexcept
    {
        state ^= state << 13u;
        state ^= state >> 7u;
        state ^= state << 17u;
        return state;
    }

    [[nodiscard]] ::std::string policy_label(::std::string_view policy)
    {
        ::std::string label{policy};
        for(auto& ch: label)
        {
            if(ch == '-') { ch = '_'; }
        }
        return label;
    }

    [[nodiscard]] ::std::vector<mode_t> make_modes()
    {
        ::std::vector<mode_t> result{};
        result.reserve(base_modes.size() + seeded_policy_mode_count);
        for(auto const& mode: base_modes) { result.push_back({::std::string{mode.name}, ::std::string{mode.args}}); }

        constexpr ::std::array<::std::string_view, 6uz> full_policies{"auto", "debug", "legacy-light", "pb-o1", "pb-o2", "pb-o3"};
        constexpr ::std::array<::std::string_view, 4uz> lazy_policies{"auto", "debug", "light", "balanced"};
        constexpr ::std::array<::std::string_view, 5uz> tiered_policies{"debug", "default", "fast-compile", "balanced", "max"};
        constexpr ::std::array<::std::string_view, 4uz> tiered_modes{
            "-Rtiered",
            "-Rtiered -Rtiered-disable-t0",
            "-Rtiered -Rtiered-disable-t2",
            "-Rtiered -Rtiered-disable-t0 -Rtiered-disable-t2"};

        if(exhaustive_policies())
        {
            // Seeded smoke tests cannot prove that every optimizer preserves
            // trap PCs and native frames. Enumerate the policy/entry-mode
            // product for the audit path; every mode below runs every fixture.
            for(auto const policy: full_policies)
            {
                for(auto const& base: {base_modes[0], base_modes[1]})
                {
                    result.push_back({"all_" + ::std::string{base.name} + "_" + policy_label(policy),
                                      ::std::string{base.args} + " -Rllvm-full-policy " + ::std::string{policy}});
                }
            }
            for(auto const policy: lazy_policies)
            {
                result.push_back({"all_lazy_" + policy_label(policy), "-Rjit -Rllvm-lazy-policy " + ::std::string{policy}});
                result.push_back({"all_lazy_verification_" + policy_label(policy),
                                  "-Rcm lazy+verification -Rcc jit -Rllvm-lazy-policy " + ::std::string{policy}});
            }
            for(::std::size_t index{}; index != tiered_modes.size(); ++index)
            {
                for(auto const policy: tiered_policies)
                {
                    result.push_back({"all_tiered_" + ::std::to_string(index) + "_" + policy_label(policy),
                                      ::std::string{tiered_modes[index]} + " -Rllvm-policy " + ::std::string{policy}});
                }
            }
            return result;
        }

        auto random_state{policy_seed()};
        for(::std::size_t index{}; index != seeded_policy_mode_count; ++index)
        {
            auto const random_value{next_policy_random(random_state)};
            auto const prefix{"seeded_" + ::std::to_string(index) + "_"};
            switch(index % 5uz)
            {
                case 0uz:
                {
                    auto const selected{full_policies[random_value % full_policies.size()]};
                    result.push_back({prefix + "full_" + policy_label(selected),
                                      "-Rcm full -Rcc jit -Rllvm-full-policy " + ::std::string{selected}});
                    break;
                }
                case 1uz:
                {
                    auto const selected{full_policies[random_value % full_policies.size()]};
                    result.push_back({prefix + "aot_" + policy_label(selected), "-Raot -Rllvm-full-policy " + ::std::string{selected}});
                    break;
                }
                case 2uz:
                {
                    auto const selected{lazy_policies[random_value % lazy_policies.size()]};
                    result.push_back({prefix + "lazy_" + policy_label(selected), "-Rjit -Rllvm-lazy-policy " + ::std::string{selected}});
                    break;
                }
                case 3uz:
                {
                    auto const selected{lazy_policies[random_value % lazy_policies.size()]};
                    result.push_back({prefix + "lazy_verification_" + policy_label(selected),
                                      "-Rcm lazy+verification -Rcc jit -Rllvm-lazy-policy " + ::std::string{selected}});
                    break;
                }
                default:
                {
                    auto const selected{tiered_policies[random_value % tiered_policies.size()]};
                    auto const tiered_mode{tiered_modes[next_policy_random(random_state) % tiered_modes.size()]};
                    result.push_back({prefix + "tiered_" + policy_label(selected),
                                      ::std::string{tiered_mode} + " -Rllvm-policy " + ::std::string{selected}});
                    break;
                }
            }
        }

        return result;
    }

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

    [[nodiscard]] ::std::string env_string(char const* name)
    {
        if(auto const env{::std::getenv(name)}; env != nullptr && *env != '\0') { return env; }
        return {};
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

    [[nodiscard]] bool probe_default_call_stack_unwind(::std::filesystem::path const& uwvm_path,
                                                       ::std::filesystem::path const& wasm_path,
                                                       ::std::filesystem::path const& artifact_dir,
                                                       fixture_t const& fixture,
                                                       bool& authoritative_win64_unwind,
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

        auto const command{quote_argument(uwvm_path) + " -Raot -Rllvm-cache-path disable -Rclog file " + quote_argument(log_path) + " --run " +
                           quote_argument(wasm_path) + " > " + quote_argument(output_path) + " 2>&1"};
        ::std::cout << "[trap-matrix] " << command << '\n';
        if(run_system_command(command) == 0)
        {
            ::std::cerr << "call-stack capability probe trap unexpectedly succeeded\n";
            return false;
        }

        ::std::string output{};
        if(!read_text_file(output_path, output)) { return false; }
        auto const plain_output{strip_ansi_codes(output)};
        auto const trap_kind{parse_trap_kind(plain_output)};
        auto const func_indices{parse_func_indices(plain_output)};
        auto const stack_matches{func_indices.size() == fixture.expected_func_count &&
                                 ::std::equal(func_indices.begin(), func_indices.end(), fixture.expected_func_indices.begin())};
        if(trap_kind != fixture.expected_trap_kind || !stack_matches)
        {
            ::std::cerr << "call-stack capability probe did not preserve the authoritative Wasm trap stack:\n" << output << '\n';
            return false;
        }

        ::std::string log{};
        if(!read_text_file(log_path, log)) { return false; }
        auto const backend_pos{log.find("unwind_backend=")};
        if(backend_pos == ::std::string::npos)
        {
            ::std::cerr << "call-stack capability probe did not log the native-unwind backend:\n" << log << '\n';
            return false;
        }
        native_unwind_backend_available = log.find("unwind_backend=unavailable", backend_pos) == ::std::string::npos;

        auto const uses_instruction{log.find("call_stack=instruction") != ::std::string::npos};
        auto const uses_unwind{log.find("call_stack=unwind") != ::std::string::npos};
        auto const uses_none{log.find("call_stack=none") != ::std::string::npos};
        auto const emits_instruction_frames{log.find("call_stack_frames=emit") != ::std::string::npos};
        auto const omits_instruction_frames{log.find("call_stack_frames=omit") != ::std::string::npos};

        if(uses_instruction)
        {
            if(uses_unwind || uses_none || !emits_instruction_frames || omits_instruction_frames)
            {
                ::std::cerr << "auto instruction call-stack policy is inconsistent:\n" << log << '\n';
                return false;
            }
            authoritative_win64_unwind = false;
            return true;
        }

        if(uses_unwind)
        {
            if(!native_unwind_backend_available || !::uwvm2test::native_unwind::matches_policy(log, "unwind") || uses_none)
            {
                ::std::cerr << "auto unwind did not use checked native replacement with logical frames omitted:\n" << log << '\n';
                return false;
            }
            authoritative_win64_unwind = true;
            return true;
        }

        ::std::cerr << "unable to determine default LLVM JIT call-stack policy from probe log"
                    << (uses_none ? " (unexpected none policy)" : "") << ":\n"
                    << log << '\n';
        return false;
    }

    [[nodiscard]] bool mode_logs_full_call_stack_policy(mode_t const& mode) noexcept
    {
        return mode.args.find("-Rcm full") != ::std::string::npos || mode.args.find("-Raot") != ::std::string::npos;
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
        auto command{quote_argument(uwvm_path) + " " + mode.args + " -Rllvm-cache-path disable -Rllvm-call-stack " + policy +
                     " -Rclog file " + quote_argument(log_path)};
        if(::std::string_view{fixture.name}.starts_with("wasm2_")) { command += " --wasm-feature-wasm2"; }
        if(auto const extra_args{env_string("UWVM_LLVM_JIT_TEST_EXTRA_RUNTIME_ARGS")}; !extra_args.empty()) { command += " " + extra_args; }
        command += " --run " + quote_argument(wasm_path);
        auto const full_command{command + " > " + quote_argument(output_path) + " 2>&1"};
        ::std::cout << "[trap-matrix] " << full_command << '\n';

        auto const status{run_system_command(full_command)};
        if(status == 0)
        {
            ::std::cerr << "trap command unexpectedly succeeded: " << stem << '\n';
            return {.valid = false, .output_path = output_path, .log_path = log_path};
        }

        ::std::string output{};
        if(!read_text_file(output_path, output)) { return {.valid = false, .output_path = output_path, .log_path = log_path}; }

        ::std::string log{};
        if(!read_text_file(log_path, log)) { return {.valid = false, .output_path = output_path, .log_path = log_path}; }

        auto const plain_output{strip_ansi_codes(output)};
        auto trap_kind{parse_trap_kind(plain_output)};
        auto func_indices{parse_func_indices(plain_output)};
        auto const expected_func_begin{fixture.expected_func_indices.begin()};
        auto const stack_matches{func_indices.size() == fixture.expected_func_count &&
                                 ::std::equal(func_indices.begin(), func_indices.end(), expected_func_begin)};
        auto const trap_matches{trap_kind == fixture.expected_trap_kind};
        bool policy_matches{true};
        if(mode_logs_full_call_stack_policy(mode))
        {
            policy_matches = ::uwvm2test::native_unwind::matches_policy(log, policy);
        }
        auto const valid{trap_matches && stack_matches && policy_matches};
        if(!valid)
        {
            ::std::cerr << "failed to parse trap output for " << stem << ":\n" << output << '\n';
            ::std::cerr << "  expected trap=\"" << fixture.expected_trap_kind << "\" funcs=[";
            for(::std::size_t i{}; i != fixture.expected_func_count; ++i)
            {
                if(i != 0uz) { ::std::cerr << ','; }
                ::std::cerr << fixture.expected_func_indices[i];
            }
            ::std::cerr << "] actual trap=\"" << trap_kind << "\" funcs=[";
            for(::std::size_t i{}; i != func_indices.size(); ++i)
            {
                if(i != 0uz) { ::std::cerr << ','; }
                ::std::cerr << func_indices[i];
            }
            ::std::cerr << "] policy_preserves_logical_frames=" << policy_matches << "\n";
            if(!policy_matches) { ::std::cerr << "  policy log:\n" << log << '\n'; }
        }

        return {.valid = valid,
                .trap_kind = ::std::move(trap_kind),
                .func_indices = ::std::move(func_indices),
                .output_path = output_path,
                .log_path = log_path};
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
    auto const project_root{env_string("UWVM_TRAP_MATRIX_PROJECT_ROOT").empty()
                                ? find_parent_with(executable_dir, "test/0014.llvm_jit/wat/trap_matrix/oob_load.wat")
                                : ::std::filesystem::path{env_string("UWVM_TRAP_MATRIX_PROJECT_ROOT")}};
    if(project_root.empty())
    {
        ::std::cerr << "failed to locate project root from " << executable << '\n';
        return 1;
    }

    auto const uwvm_path{env_string("UWVM_TRAP_MATRIX_BINARY").empty()
                             ? find_uwvm_binary(executable_dir)
                             : ::std::filesystem::path{env_string("UWVM_TRAP_MATRIX_BINARY")}};
    if(uwvm_path.empty())
    {
        ::std::cerr << "failed to locate uwvm next to test executable: " << executable << '\n';
        return 1;
    }

    auto const strict_env{::std::getenv("UWVM_TRAP_MATRIX_STRICT")};
    bool const strict{strict_env == nullptr || *strict_env == '\0' || ::std::string_view{strict_env} != "0"};
    auto const wat2wasm_path{find_wat2wasm(project_root)};
    if(wat2wasm_path.empty())
    {
        if(strict)
        {
            ::std::cerr << "[trap-matrix] wat2wasm is required in strict mode; set WAT2WASM or put wat2wasm in PATH\n";
            return 1;
        }
        ::std::cout << "[trap-matrix] skip: wat2wasm not found; set WAT2WASM or put wat2wasm in PATH\n";
        return 0;
    }

    auto const wat_dir{project_root / "test" / "0014.llvm_jit" / "wat" / "trap_matrix"};
    auto const artifact_dir{[](::std::filesystem::path const& dir) {
        if(auto const env{::std::getenv("UWVM_TRAP_MATRIX_ARTIFACT_DIR")}; env != nullptr && *env != '\0') { return ::std::filesystem::path{env}; }
        return dir / "test-artifacts" / "0014.llvm_jit" / "trap_matrix";
    }(executable_dir)};

    bool ok{true};
    bool call_stack_capability_probed{};
    bool authoritative_win64_unwind{};
    bool native_unwind_backend_available{};
    ::std::size_t mismatch_count{};
    ::std::size_t executed_cases{};
    ::std::size_t unavailable_policy_cases{};
    auto const modes{make_modes()};
    ::std::cout << "[trap-matrix] exhaustive=" << exhaustive_policies() << " full_only=" << full_only()
                << " deterministic policy seed=" << policy_seed() << " modes=" << modes.size() << '\n';
    auto const fixture_filter{::std::getenv("UWVM_TRAP_MATRIX_FIXTURE")};
    ::std::size_t selected_fixtures{};
    for(auto const& fixture: fixtures)
    {
        // A fixture shard retains every backend/policy combination and its own capability probe. Keep artifacts
        // separate per shard; the default (no filter) remains the complete matrix, including seeded policies.
        if(fixture_filter != nullptr && *fixture_filter != '\0' && ::std::string_view{fixture.name} != fixture_filter) { continue; }
        ++selected_fixtures;
        auto const wat_path{wat_dir / fixture.wat_name};
        auto const wasm_path{artifact_dir / (::std::string{fixture.name} + ".wasm")};
        if(!compile_wat(wat2wasm_path, wat_path, wasm_path)) { return 1; }

        if(!call_stack_capability_probed)
        {
            if(!probe_default_call_stack_unwind(
                   uwvm_path, wasm_path, artifact_dir, fixture, authoritative_win64_unwind, native_unwind_backend_available))
            {
                return 1;
            }
            call_stack_capability_probed = true;
            if(env_string("UWVM_TRAP_MATRIX_REQUIRE_NATIVE") == "1" && !authoritative_win64_unwind)
            {
                ::std::cerr << "[trap-matrix] checked native replacement was required but auto selected instruction fallback\n";
                return 1;
            }
            ::std::cout << (authoritative_win64_unwind
                                ? "[trap-matrix] checked native unwind owns generated Wasm frames\n"
                                : "[trap-matrix] native self-check unavailable; auto retains logical instruction frames\n");
        }

        for(::std::size_t mode_index{}; mode_index != modes.size(); ++mode_index)
        {
            if(!exhaustive_policies() && mode_index >= base_modes.size() &&
               ::std::string_view{fixture.name} != seeded_policy_fixtures[mode_index - base_modes.size()])
            {
                continue;
            }

            auto const& mode{modes[mode_index]};
            if(full_only() && mode.args.find("-Raot") == ::std::string::npos) { continue; }
            auto const instruction{run_case(uwvm_path, wasm_path, artifact_dir, fixture, mode, "instruction")};
            ++executed_cases;
            if(!instruction.valid)
            {
                ++mismatch_count;
                ok = false;
                continue;
            }

            for(auto const* policy: compare_policies)
            {
                auto const policy_name{::std::string_view{policy}};
                if((policy_name == "unwind" && !authoritative_win64_unwind) ||
                   (policy_name == "unwind-uncheck" && !native_unwind_backend_available))
                {
                    ++unavailable_policy_cases;
                    continue;
                }

                auto const compared{run_case(uwvm_path, wasm_path, artifact_dir, fixture, mode, policy)};
                ++executed_cases;
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

    if(selected_fixtures == 0uz)
    {
        ::std::cerr << "[trap-matrix] no fixture matched UWVM_TRAP_MATRIX_FIXTURE\n";
        return 1;
    }
    if(ok && mismatch_count == 0uz)
    {
        // A fallback pass is useful but is not proof that a native unwinder ran.
        ::std::cout << "[trap-matrix] all trap outputs matched instruction baselines; executed=" << executed_cases
                    << " unavailable_policy_cases=" << unavailable_policy_cases << '\n';
        return 0;
    }

    ::std::cout << "[trap-matrix] mismatches=" << mismatch_count << (strict ? " strict=1\n" : " strict=0\n");
    return strict && !ok ? 1 : 0;
}
