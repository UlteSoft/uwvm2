#include <array>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace
{
    inline constexpr ::std::array<unsigned char, 98uz> nontrivial_start_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u,
        0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x00u, 0x05u, 0x03u, 0x01u, 0x00u, 0x01u, 0x07u,
        0x0au, 0x01u, 0x06u, 0x5fu, 0x73u, 0x74u, 0x61u, 0x72u, 0x74u, 0x00u, 0x00u, 0x0au,
        0x3du, 0x01u, 0x3bu, 0x01u, 0x01u, 0x7fu, 0x41u, 0x00u, 0x21u, 0x00u, 0x02u, 0x40u,
        0x03u, 0x40u, 0x20u, 0x00u, 0x41u, 0x05u, 0x48u, 0x45u, 0x0du, 0x01u, 0x20u, 0x00u,
        0x41u, 0x01u, 0x6au, 0x21u, 0x00u, 0x0cu, 0x00u, 0x0bu, 0x0bu, 0x20u, 0x00u, 0x41u,
        0x05u, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x41u, 0x00u, 0x41u, 0x2au, 0x36u, 0x02u,
        0x00u, 0x41u, 0x00u, 0x28u, 0x02u, 0x00u, 0x41u, 0x2au, 0x47u, 0x04u, 0x40u, 0x00u,
        0x0bu, 0x0bu};

    struct cache_file_state
    {
        ::std::string relative_path{};
        ::std::uintmax_t size{};
        ::std::filesystem::file_time_type write_time{};

        [[nodiscard]] friend bool operator==(cache_file_state const&, cache_file_state const&) = default;
    };

    struct env_guard
    {
        char const* name{};
        bool had_old{};
        ::std::string old_value{};

        env_guard(char const* env_name, ::std::filesystem::path const& value) : name{env_name}
        {
            if(auto const old{::std::getenv(name)}; old != nullptr)
            {
                had_old = true;
                old_value = old;
            }

#ifdef _WIN32
            ::_putenv_s(name, value.string().c_str());
#else
            ::setenv(name, value.string().c_str(), 1);
#endif
        }

        ~env_guard()
        {
#ifdef _WIN32
            ::_putenv_s(name, had_old ? old_value.c_str() : "");
#else
            if(had_old) { ::setenv(name, old_value.c_str(), 1); }
            else { ::unsetenv(name); }
#endif
        }

        env_guard(env_guard const&) = delete;
        env_guard& operator=(env_guard const&) = delete;
    };

    [[nodiscard]] ::std::string quote_argument(::std::filesystem::path const& path)
    { return ::std::string{"\""} + path.string() + "\""; }

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
        if(!input) { return false; }
        text.assign(::std::istreambuf_iterator<char>{input}, ::std::istreambuf_iterator<char>{});
        return !input.bad();
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

    [[nodiscard]] bool write_fixture(::std::filesystem::path const& wasm_path)
    {
        ::std::error_code ec{};
        ::std::filesystem::create_directories(wasm_path.parent_path(), ec);
        if(ec)
        {
            ::std::cerr << "failed to create fixture directory: " << wasm_path.parent_path() << '\n';
            return false;
        }

        ::std::ofstream output(wasm_path, ::std::ios::binary | ::std::ios::trunc);
        if(!output)
        {
            ::std::cerr << "failed to open fixture output: " << wasm_path << '\n';
            return false;
        }

        output.write(reinterpret_cast<char const*>(nontrivial_start_wasm.data()),
                     static_cast<::std::streamsize>(nontrivial_start_wasm.size()));
        if(!output)
        {
            ::std::cerr << "failed to write fixture output: " << wasm_path << '\n';
            return false;
        }

        return true;
    }

    [[nodiscard]] ::std::vector<cache_file_state> snapshot_cache(::std::filesystem::path const& cache_dir)
    {
        ::std::vector<cache_file_state> out{};
        if(!::std::filesystem::exists(cache_dir)) { return out; }

        for(auto const& entry: ::std::filesystem::recursive_directory_iterator{cache_dir})
        {
            if(!entry.is_regular_file()) { continue; }
            auto const path{entry.path()};
            cache_file_state state{};
            state.relative_path = ::std::filesystem::relative(path, cache_dir).generic_string();
            state.size = entry.file_size();
            state.write_time = entry.last_write_time();
            out.push_back(::std::move(state));
        }

        ::std::sort(out.begin(), out.end(), [](cache_file_state const& lhs, cache_file_state const& rhs) noexcept
        { return lhs.relative_path < rhs.relative_path; });
        return out;
    }

    void print_snapshot(::std::vector<cache_file_state> const& snapshot)
    {
        for(auto const& file: snapshot) { ::std::cerr << "  " << file.relative_path << " size=" << file.size << '\n'; }
    }

    [[nodiscard]] bool run_uwvm(::std::filesystem::path const& uwvm_path,
                                ::std::filesystem::path const& artifact_dir,
                                ::std::filesystem::path const& wasm_path,
                                ::std::string_view runtime_args,
                                ::std::string_view cache_args,
                                ::std::string_view label)
    {
        auto const output_path{artifact_dir / (::std::string{label} + ".out")};
        auto command{quote_argument(uwvm_path) + " " + ::std::string{runtime_args}};
        if(!cache_args.empty()) { command += " " + ::std::string{cache_args}; }
        command += " --run " + quote_argument(wasm_path) + " > " + quote_argument(output_path) + " 2>&1";

        ::std::cout << "[llvm_jit_cache] " << command << '\n';
        auto const status{run_system_command(command)};
        if(status == 0) { return true; }

        ::std::string output{};
        (void)read_text_file(output_path, output);
        ::std::cerr << "uwvm returned non-zero status for " << label << ": " << status << "\n" << output << '\n';
        return false;
    }

    [[nodiscard]] bool run_cached_mode_twice(::std::filesystem::path const& uwvm_path,
                                             ::std::filesystem::path const& artifact_dir,
                                             ::std::filesystem::path const& wasm_path,
                                             ::std::filesystem::path const& cache_dir,
                                             ::std::string_view runtime_args,
                                             ::std::string_view label)
    {
        ::std::filesystem::remove_all(cache_dir);
        ::std::filesystem::create_directories(cache_dir);

        auto const cache_args{::std::string{"--runtime-llvm-jit-cache-path path "} + quote_argument(cache_dir)};
        if(!run_uwvm(uwvm_path, artifact_dir, wasm_path, runtime_args, cache_args, ::std::string{label} + "_first")) { return false; }

        auto const first{snapshot_cache(cache_dir)};
        if(first.empty())
        {
            ::std::cerr << "expected non-empty cache for " << label << '\n';
            return false;
        }

        ::std::this_thread::sleep_for(::std::chrono::milliseconds{1200});
        if(!run_uwvm(uwvm_path, artifact_dir, wasm_path, runtime_args, cache_args, ::std::string{label} + "_second")) { return false; }

        auto const second{snapshot_cache(cache_dir)};
        if(first != second)
        {
            ::std::cerr << "cache was rewritten instead of reused for " << label << "\nfirst:\n";
            print_snapshot(first);
            ::std::cerr << "second:\n";
            print_snapshot(second);
            return false;
        }

        return true;
    }

    [[nodiscard]] bool test_cache_path_modes(::std::filesystem::path const& uwvm_path,
                                             ::std::filesystem::path const& artifact_dir,
                                             ::std::filesystem::path const& wasm_path)
    {
        auto const disabled_cache{artifact_dir / "cache-disabled"};
        auto const disabled_env{artifact_dir / "env-disabled"};
        ::std::filesystem::remove_all(disabled_cache);
        ::std::filesystem::remove_all(disabled_env);
        ::std::filesystem::create_directories(disabled_cache);
        ::std::filesystem::create_directories(disabled_env);

        {
            env_guard home_guard{"HOME", disabled_env};
            env_guard xdg_guard{"XDG_CACHE_HOME", disabled_env};
            env_guard localappdata_guard{"LOCALAPPDATA", disabled_env};
            env_guard userprofile_guard{"USERPROFILE", disabled_env};
            env_guard tmpdir_guard{"TMPDIR", disabled_env};
            if(!run_uwvm(uwvm_path, artifact_dir, wasm_path, "-Rjit", "--runtime-llvm-jit-cache-path disable", "cache_path_disable")) { return false; }
        }
        if(!snapshot_cache(disabled_cache).empty())
        {
            ::std::cerr << "disabled cache path still produced files\n";
            return false;
        }
        if(!snapshot_cache(disabled_env).empty())
        {
            ::std::cerr << "disabled default cache path still produced files\n";
            return false;
        }

        auto const default_env{artifact_dir / "env-default"};
        ::std::filesystem::remove_all(default_env);
        ::std::filesystem::create_directories(default_env);
        {
            env_guard home_guard{"HOME", default_env};
            env_guard xdg_guard{"XDG_CACHE_HOME", default_env};
            env_guard localappdata_guard{"LOCALAPPDATA", default_env};
            env_guard userprofile_guard{"USERPROFILE", default_env};
            env_guard tmpdir_guard{"TMPDIR", default_env};
            if(!run_uwvm(uwvm_path, artifact_dir, wasm_path, "-Rjit", "--runtime-llvm-jit-cache-path default", "cache_path_default")) { return false; }
        }
        if(snapshot_cache(default_env).empty())
        {
            ::std::cerr << "default cache path mode did not produce cache output in isolated env root\n";
            return false;
        }

        auto const implicit_default_env{artifact_dir / "env-implicit-default"};
        ::std::filesystem::remove_all(implicit_default_env);
        ::std::filesystem::create_directories(implicit_default_env);
        {
            env_guard home_guard{"HOME", implicit_default_env};
            env_guard xdg_guard{"XDG_CACHE_HOME", implicit_default_env};
            env_guard localappdata_guard{"LOCALAPPDATA", implicit_default_env};
            env_guard userprofile_guard{"USERPROFILE", implicit_default_env};
            env_guard tmpdir_guard{"TMPDIR", implicit_default_env};
            if(!run_uwvm(uwvm_path, artifact_dir, wasm_path, "-Rjit", "", "cache_path_implicit_default")) { return false; }
        }
        if(snapshot_cache(implicit_default_env).empty())
        {
            ::std::cerr << "implicit default cache path mode did not produce cache output in isolated env root\n";
            return false;
        }

        return true;
    }

    [[nodiscard]] bool test_shared_cache_key_isolation(::std::filesystem::path const& uwvm_path,
                                                       ::std::filesystem::path const& artifact_dir,
                                                       ::std::filesystem::path const& wasm_path)
    {
        auto const cache_dir{artifact_dir / "cache-shared"};
        ::std::filesystem::remove_all(cache_dir);
        ::std::filesystem::create_directories(cache_dir);
        auto const cache_args{::std::string{"--runtime-llvm-jit-cache-path path "} + quote_argument(cache_dir)};

        ::std::size_t previous_count{};
        auto const expect_new_cache_entry{[&](::std::string_view label, ::std::string_view args) -> bool
        {
            if(!run_uwvm(uwvm_path, artifact_dir, wasm_path, args, cache_args, ::std::string{"shared_"} + ::std::string{label}))
            {
                return false;
            }

            auto const snapshot{snapshot_cache(cache_dir)};
            if(snapshot.size() <= previous_count)
            {
                ::std::cerr << "cache key was not isolated for mode " << label << "; previous_count=" << previous_count
                            << " current_count=" << snapshot.size() << '\n';
                print_snapshot(snapshot);
                return false;
            }
            previous_count = snapshot.size();
            return true;
        }};

        if(!expect_new_cache_entry("lazy", "-Rjit")) { return false; }
        if(!expect_new_cache_entry("lazy_verify", "-Rcm lazy+verification -Rcc jit")) { return false; }
        if(!expect_new_cache_entry("full", "-Rcm full -Rcc jit")) { return false; }

        auto const full_snapshot{snapshot_cache(cache_dir)};
        ::std::this_thread::sleep_for(::std::chrono::milliseconds{1200});
        if(!run_uwvm(uwvm_path, artifact_dir, wasm_path, "-Raot", cache_args, "shared_aot")) { return false; }
        auto const aot_snapshot{snapshot_cache(cache_dir)};
        if(aot_snapshot != full_snapshot)
        {
            ::std::cerr << "AOT did not reuse the existing full LLVM cache object\nfull:\n";
            print_snapshot(full_snapshot);
            ::std::cerr << "aot:\n";
            print_snapshot(aot_snapshot);
            return false;
        }
        previous_count = aot_snapshot.size();

        return expect_new_cache_entry("tiered_no_t0", "-Rtiered -Rtiered-disable-t0");
    }

    [[nodiscard]] bool test_default_tiered_smoke(::std::filesystem::path const& uwvm_path,
                                                 ::std::filesystem::path const& artifact_dir,
                                                 ::std::filesystem::path const& wasm_path)
    {
        auto const cache_dir{artifact_dir / "cache-tiered-default"};
        ::std::filesystem::remove_all(cache_dir);
        ::std::filesystem::create_directories(cache_dir);
        auto const cache_args{::std::string{"--runtime-llvm-jit-cache-path path "} + quote_argument(cache_dir)};
        return run_uwvm(uwvm_path, artifact_dir, wasm_path, "-Rtiered", cache_args, "tiered_default");
    }

    [[nodiscard]] bool test_unsigned_cache_policy(::std::filesystem::path const& uwvm_path,
                                                  ::std::filesystem::path const& artifact_dir,
                                                  ::std::filesystem::path const& wasm_path)
    {
        auto const cache_dir{artifact_dir / "cache-unsigned"};
        ::std::filesystem::remove_all(cache_dir);
        ::std::filesystem::create_directories(cache_dir);
        auto const cache_args{::std::string{"--runtime-llvm-jit-cache-path path "} + quote_argument(cache_dir)};

        if(!run_uwvm(uwvm_path,
                     artifact_dir,
                     wasm_path,
                     "-Rjit",
                     cache_args + " --runtime-llvm-jit-cache-no-sign",
                     "unsigned_write"))
        {
            return false;
        }
        auto const unsigned_snapshot{snapshot_cache(cache_dir)};
        if(unsigned_snapshot.empty())
        {
            ::std::cerr << "unsigned cache write produced no files\n";
            return false;
        }

        ::std::this_thread::sleep_for(::std::chrono::milliseconds{1200});
        if(!run_uwvm(uwvm_path,
                     artifact_dir,
                     wasm_path,
                     "-Rjit",
                     cache_args + " --runtime-llvm-jit-cache-no-sign --runtime-llvm-jit-cache-no-verify",
                     "unsigned_no_verify_reuse"))
        {
            return false;
        }
        auto const no_verify_snapshot{snapshot_cache(cache_dir)};
        if(unsigned_snapshot != no_verify_snapshot)
        {
            ::std::cerr << "unsigned cache was rewritten when no-verify was enabled\n";
            return false;
        }

        ::std::this_thread::sleep_for(::std::chrono::milliseconds{1200});
        if(!run_uwvm(uwvm_path, artifact_dir, wasm_path, "-Rjit", cache_args, "unsigned_verify_rewrite")) { return false; }
        auto const verified_snapshot{snapshot_cache(cache_dir)};
        if(verified_snapshot == no_verify_snapshot)
        {
            ::std::cerr << "signed verification did not reject/rewrite an unsigned cache object\n";
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
    auto const uwvm_path{find_uwvm_binary(executable_dir)};
    if(uwvm_path.empty())
    {
        ::std::cerr << "failed to locate uwvm next to test executable: " << executable << '\n';
        return 1;
    }

    auto const artifact_dir{executable_dir / "test-artifacts" / "0014.llvm_jit_cache"};
    ::std::filesystem::remove_all(artifact_dir);
    ::std::filesystem::create_directories(artifact_dir);
    auto const wasm_path{artifact_dir / "nontrivial_start.wasm"};
    if(!write_fixture(wasm_path)) { return 1; }

    if(!test_cache_path_modes(uwvm_path, artifact_dir, wasm_path)) { return 1; }
    if(!run_cached_mode_twice(uwvm_path, artifact_dir, wasm_path, artifact_dir / "cache-lazy", "-Rjit", "lazy")) { return 1; }
    if(!run_cached_mode_twice(uwvm_path, artifact_dir, wasm_path, artifact_dir / "cache-lazy-verify", "-Rcm lazy+verification -Rcc jit", "lazy_verify"))
    {
        return 1;
    }
    if(!run_cached_mode_twice(uwvm_path, artifact_dir, wasm_path, artifact_dir / "cache-full", "-Rcm full -Rcc jit", "full")) { return 1; }
    if(!run_cached_mode_twice(uwvm_path, artifact_dir, wasm_path, artifact_dir / "cache-aot", "-Raot", "aot")) { return 1; }
    if(!test_default_tiered_smoke(uwvm_path, artifact_dir, wasm_path)) { return 1; }
    if(!run_cached_mode_twice(uwvm_path,
                              artifact_dir,
                              wasm_path,
                              artifact_dir / "cache-tiered-no-t0",
                              "-Rtiered -Rtiered-disable-t0",
                              "tiered_no_t0"))
    {
        return 1;
    }
    if(!test_shared_cache_key_isolation(uwvm_path, artifact_dir, wasm_path)) { return 1; }
    if(!test_unsigned_cache_policy(uwvm_path, artifact_dir, wasm_path)) { return 1; }

    return 0;
}
