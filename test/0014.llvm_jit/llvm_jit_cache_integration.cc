#include <array>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
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

    inline constexpr ::std::array<unsigned char, 138uz> numeric_types_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u,
        0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x00u, 0x08u, 0x01u, 0x00u, 0x0au, 0x73u, 0x01u,
        0x71u, 0x04u, 0x01u, 0x7fu, 0x01u, 0x7eu, 0x01u, 0x7du, 0x01u, 0x7cu, 0x41u, 0x28u,
        0x41u, 0x02u, 0x6au, 0x21u, 0x00u, 0x20u, 0x00u, 0x41u, 0x2au, 0x47u, 0x04u, 0x40u,
        0x00u, 0x0bu, 0x42u, 0x80u, 0xc8u, 0xafu, 0xa0u, 0x25u, 0x42u, 0xeau, 0x01u, 0x7cu,
        0x21u, 0x01u, 0x20u, 0x01u, 0x42u, 0xeau, 0xc9u, 0xafu, 0xa0u, 0x25u, 0x52u, 0x04u,
        0x40u, 0x00u, 0x0bu, 0x43u, 0x00u, 0x00u, 0xc0u, 0x3fu, 0x43u, 0x00u, 0x00u, 0x10u,
        0x40u, 0x92u, 0x21u, 0x02u, 0x20u, 0x02u, 0x43u, 0x00u, 0x00u, 0x70u, 0x40u, 0x5cu,
        0x04u, 0x40u, 0x00u, 0x0bu, 0x44u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x22u,
        0x40u, 0x44u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xe0u, 0x3fu, 0xa2u, 0x21u,
        0x03u, 0x20u, 0x03u, 0x44u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x12u, 0x40u,
        0x62u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x0bu};

    inline constexpr ::std::array<unsigned char, 130uz> memory_table_global_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x08u, 0x02u, 0x60u,
        0x00u, 0x01u, 0x7fu, 0x60u, 0x00u, 0x00u, 0x03u, 0x04u, 0x03u, 0x00u, 0x00u, 0x01u,
        0x04u, 0x04u, 0x01u, 0x70u, 0x00u, 0x02u, 0x05u, 0x03u, 0x01u, 0x00u, 0x01u, 0x06u,
        0x06u, 0x01u, 0x7fu, 0x01u, 0x41u, 0x03u, 0x0bu, 0x08u, 0x01u, 0x02u, 0x09u, 0x08u,
        0x01u, 0x00u, 0x41u, 0x00u, 0x0bu, 0x02u, 0x00u, 0x01u, 0x0au, 0x3cu, 0x03u, 0x04u,
        0x00u, 0x41u, 0x0bu, 0x0bu, 0x07u, 0x00u, 0x23u, 0x00u, 0x41u, 0x04u, 0x6au, 0x0bu,
        0x2du, 0x00u, 0x41u, 0x00u, 0x41u, 0x07u, 0x36u, 0x02u, 0x00u, 0x41u, 0x00u, 0x28u,
        0x02u, 0x00u, 0x41u, 0x07u, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x41u, 0x01u, 0x11u,
        0x00u, 0x00u, 0x41u, 0x07u, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x41u, 0x08u, 0x28u,
        0x02u, 0x00u, 0x41u, 0x2au, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x0bu, 0x0bu, 0x0au,
        0x01u, 0x00u, 0x41u, 0x08u, 0x0bu, 0x04u, 0x2au, 0x00u, 0x00u, 0x00u};

    inline constexpr ::std::array<unsigned char, 71uz> control_flow_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u,
        0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x00u, 0x08u, 0x01u, 0x00u, 0x0au, 0x30u, 0x01u,
        0x2eu, 0x01u, 0x01u, 0x7fu, 0x41u, 0x00u, 0x21u, 0x00u, 0x02u, 0x40u, 0x03u, 0x40u,
        0x20u, 0x00u, 0x41u, 0x10u, 0x4fu, 0x0du, 0x01u, 0x20u, 0x00u, 0x41u, 0x01u, 0x6au,
        0x21u, 0x00u, 0x0cu, 0x00u, 0x0bu, 0x0bu, 0x20u, 0x00u, 0x41u, 0x10u, 0x47u, 0x04u,
        0x40u, 0x00u, 0x0bu, 0x41u, 0x02u, 0x0eu, 0x01u, 0x00u, 0x00u, 0x00u, 0x0bu};

    inline constexpr ::std::array<unsigned char, 90uz> wasm1p1_scalar_start_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x04u, 0x01u, 0x60u,
        0x00u, 0x00u, 0x03u, 0x02u, 0x01u, 0x00u, 0x07u, 0x0au, 0x01u, 0x06u, 0x5fu, 0x73u,
        0x74u, 0x61u, 0x72u, 0x74u, 0x00u, 0x00u, 0x0au, 0x3au, 0x01u, 0x38u, 0x00u, 0x41u,
        0x80u, 0x01u, 0xc0u, 0x41u, 0x80u, 0x7fu, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x42u,
        0x80u, 0x01u, 0xc2u, 0x42u, 0x80u, 0x7fu, 0x52u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x41u,
        0x05u, 0x41u, 0x07u, 0x41u, 0x00u, 0x1cu, 0x01u, 0x7fu, 0x41u, 0x07u, 0x47u, 0x04u,
        0x40u, 0x00u, 0x0bu, 0x43u, 0x00u, 0x00u, 0xc0u, 0xbfu, 0xfcu, 0x01u, 0x41u, 0x00u,
        0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x0bu};

    inline constexpr ::std::array<unsigned char, 59uz> wasm1p1_multivalue_start_wasm{
        0x00u, 0x61u, 0x73u, 0x6du, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x09u, 0x02u, 0x60u,
        0x00u, 0x02u, 0x7fu, 0x7fu, 0x60u, 0x00u, 0x00u, 0x03u, 0x03u, 0x02u, 0x00u, 0x01u,
        0x07u, 0x0au, 0x01u, 0x06u, 0x5fu, 0x73u, 0x74u, 0x61u, 0x72u, 0x74u, 0x00u, 0x01u,
        0x0au, 0x15u, 0x02u, 0x06u, 0x00u, 0x41u, 0x0au, 0x41u, 0x20u, 0x0bu, 0x0cu, 0x00u,
        0x10u, 0x00u, 0x6au, 0x41u, 0x2au, 0x47u, 0x04u, 0x40u, 0x00u, 0x0bu, 0x0bu};

    struct wasm_fixture_def
    {
        ::std::string_view label{};
        ::std::string_view file_name{};
        unsigned char const* data{};
        ::std::size_t size{};
    };

    struct wasm_fixture_file
    {
        ::std::string label{};
        ::std::filesystem::path path{};
    };

    template <::std::size_t size>
    [[nodiscard]] constexpr wasm_fixture_def make_wasm_fixture_def(::std::string_view label,
                                                                    ::std::string_view file_name,
                                                                    ::std::array<unsigned char, size> const& bytes) noexcept
    {
        return {.label = label, .file_name = file_name, .data = bytes.data(), .size = bytes.size()};
    }

    inline constexpr ::std::array wasm_fixture_defs{
        make_wasm_fixture_def("nontrivial_start", "nontrivial_start.wasm", nontrivial_start_wasm),
        make_wasm_fixture_def("numeric_types", "numeric types.wasm", numeric_types_wasm),
        make_wasm_fixture_def("memory_table_global", "memory;table;global.wasm", memory_table_global_wasm),
        make_wasm_fixture_def("control_flow", "control-flow.wasm", control_flow_wasm)};

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
    {
        auto text{path.string()};
#ifdef _WIN32
        auto trailing_backslashes{0uz};
        for(auto it{text.rbegin()}; it != text.rend() && *it == '\\'; ++it) { ++trailing_backslashes; }
        text.append(trailing_backslashes, '\\');
#endif
        return ::std::string{"\""} + text + "\"";
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
        if(!input) { return false; }
        text.assign(::std::istreambuf_iterator<char>{input}, ::std::istreambuf_iterator<char>{});
        return !input.bad();
    }

    [[nodiscard]] bool read_binary_file(::std::filesystem::path const& path, ::std::vector<unsigned char>& bytes)
    {
        ::std::ifstream input(path, ::std::ios::binary);
        if(!input) { return false; }
        bytes.assign(::std::istreambuf_iterator<char>{input}, ::std::istreambuf_iterator<char>{});
        return !input.bad();
    }

    [[nodiscard]] bool write_binary_file(::std::filesystem::path const& path, ::std::vector<unsigned char> const& bytes)
    {
        ::std::ofstream output(path, ::std::ios::binary | ::std::ios::trunc);
        if(!output) { return false; }
        if(!bytes.empty())
        {
            output.write(reinterpret_cast<char const*>(bytes.data()), static_cast<::std::streamsize>(bytes.size()));
        }
        return static_cast<bool>(output);
    }

    [[nodiscard]] bool read_output(::std::filesystem::path const& artifact_dir, ::std::string_view label, ::std::string& output)
    { return read_text_file(artifact_dir / (::std::string{label} + ".out"), output); }

    [[nodiscard]] bool output_contains(::std::filesystem::path const& artifact_dir, ::std::string_view label, ::std::string_view needle)
    {
        ::std::string output{};
        return read_output(artifact_dir, label, output) && output.find(needle) != ::std::string::npos;
    }

    [[nodiscard]] ::std::uint_least64_t read_u64_le(::std::vector<unsigned char> const& bytes, ::std::size_t offset)
    {
        if(bytes.size() < offset + 8uz) { return 0u; }
        ::std::uint_least64_t value{};
        for(::std::size_t i{}; i != 8uz; ++i)
        {
            value |= static_cast<::std::uint_least64_t>(bytes[offset + i]) << (i * 8uz);
        }
        return value;
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

    [[nodiscard]] bool write_fixture(::std::filesystem::path const& wasm_path, unsigned char const* data, ::std::size_t size)
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

        output.write(reinterpret_cast<char const*>(data), static_cast<::std::streamsize>(size));
        if(!output)
        {
            ::std::cerr << "failed to write fixture output: " << wasm_path << '\n';
            return false;
        }

        return true;
    }

    [[nodiscard]] bool write_fixture(::std::filesystem::path const& wasm_path, wasm_fixture_def const& fixture)
    { return write_fixture(wasm_path, fixture.data, fixture.size); }

    [[nodiscard]] bool write_cache_fixtures(::std::filesystem::path const& artifact_dir, ::std::vector<wasm_fixture_file>& fixtures)
    {
        fixtures.clear();
        fixtures.reserve(wasm_fixture_defs.size());

        for(auto const& fixture: wasm_fixture_defs)
        {
            auto const wasm_path{artifact_dir / fixture.file_name};
            if(!write_fixture(wasm_path, fixture)) { return false; }
            fixtures.push_back({::std::string{fixture.label}, wasm_path});
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

    [[nodiscard]] ::std::vector<::std::filesystem::path> cache_regular_files(::std::filesystem::path const& cache_dir)
    {
        ::std::vector<::std::filesystem::path> out{};
        if(!::std::filesystem::exists(cache_dir)) { return out; }

        for(auto const& entry: ::std::filesystem::recursive_directory_iterator{cache_dir})
        {
            if(entry.is_regular_file()) { out.push_back(entry.path()); }
        }

        ::std::sort(out.begin(), out.end());
        return out;
    }

    [[nodiscard]] bool first_cache_file(::std::filesystem::path const& cache_dir, ::std::filesystem::path& out)
    {
        auto files{cache_regular_files(cache_dir)};
        if(files.empty()) { return false; }
        out = ::std::move(files.front());
        return true;
    }

    [[nodiscard]] bool rewrite_first_cache_file(::std::filesystem::path const& cache_dir,
                                                bool (*mutate)(::std::vector<unsigned char>&),
                                                ::std::string_view label)
    {
        ::std::filesystem::path file{};
        if(!first_cache_file(cache_dir, file))
        {
            ::std::cerr << "missing cache file for mutation " << label << '\n';
            return false;
        }

        ::std::vector<unsigned char> bytes{};
        if(!read_binary_file(file, bytes))
        {
            ::std::cerr << "failed to read cache file for mutation " << label << ": " << file << '\n';
            return false;
        }

        if(!mutate(bytes))
        {
            ::std::cerr << "cache mutation had no effect for " << label << '\n';
            return false;
        }

        if(!write_binary_file(file, bytes))
        {
            ::std::cerr << "failed to write mutated cache file for " << label << ": " << file << '\n';
            return false;
        }

        return true;
    }

    [[nodiscard]] bool flip_magic_byte(::std::vector<unsigned char>& bytes)
    {
        if(bytes.empty()) { return false; }
        bytes[0] ^= 0x80u;
        return true;
    }

    [[nodiscard]] bool truncate_cache_blob(::std::vector<unsigned char>& bytes)
    {
        if(bytes.size() <= 1uz) { return false; }
        bytes.resize(bytes.size() / 2uz);
        return true;
    }

    [[nodiscard]] bool flip_context_abi_byte(::std::vector<unsigned char>& bytes)
    {
        auto const needle{::std::string_view{"uwvm2-runtime-abi-v4"}};
        auto const iter{::std::search(bytes.begin(), bytes.end(), needle.begin(), needle.end())};
        if(iter == bytes.end()) { return false; }
        auto const offset{static_cast<::std::size_t>(iter - bytes.begin())};
        bytes[offset + needle.size() - 1uz] ^= 0x01u;
        return true;
    }

    [[nodiscard]] bool flip_payload_byte(::std::vector<unsigned char>& bytes)
    {
        constexpr ::std::size_t header_size{64uz};
        if(bytes.size() < header_size) { return false; }

        auto const payload_size{read_u64_le(bytes, 32uz)};
        auto const isa_size{read_u64_le(bytes, 40uz)};
        auto const context_size{read_u64_le(bytes, 48uz)};
        auto const signature_size{read_u64_le(bytes, 56uz)};
        auto const payload_offset{header_size + static_cast<::std::size_t>(isa_size) + static_cast<::std::size_t>(context_size) +
                                  static_cast<::std::size_t>(signature_size)};
        if(payload_size == 0u || payload_offset >= bytes.size()) { return false; }
        auto const payload_size_sz{static_cast<::std::size_t>(payload_size)};
        if(payload_size_sz > bytes.size() - payload_offset) { return false; }

        bytes[payload_offset + payload_size_sz / 2uz] ^= 0x5au;
        return true;
    }

    [[nodiscard]] bool deterministic_fuzz_cache_blob(::std::vector<unsigned char>& bytes, ::std::size_t iteration)
    {
        ::std::uint_least64_t state{0x9e3779b97f4a7c15ull ^ static_cast<::std::uint_least64_t>(iteration)};
        auto next_byte{[&]() noexcept -> unsigned char
        {
            state = state * 6364136223846793005ull + 1442695040888963407ull;
            return static_cast<unsigned char>((state >> 32u) & 0xffu);
        }};

        switch(iteration % 4uz)
        {
            case 0uz:
            {
                auto const size{static_cast<::std::size_t>((iteration * 29uz + 17uz) % 96uz)};
                bytes.resize(size);
                for(auto& b: bytes) { b = next_byte(); }
                return true;
            }
            case 1uz:
            {
                if(bytes.empty()) { bytes.resize(1uz); }
                for(::std::size_t n{}; n != 8uz; ++n)
                {
                    auto const pos{static_cast<::std::size_t>(next_byte()) % bytes.size()};
                    bytes[pos] ^= static_cast<unsigned char>(next_byte() | 1u);
                }
                return true;
            }
            case 2uz:
            {
                if(bytes.size() <= 1uz) { return false; }
                bytes.resize((iteration * 13uz) % bytes.size());
                return true;
            }
            default:
            {
                auto const old_size{bytes.size()};
                bytes.resize(old_size + 33uz);
                for(auto pos{old_size}; pos != bytes.size(); ++pos) { bytes[pos] = next_byte(); }
                if(!bytes.empty()) { bytes[static_cast<::std::size_t>(next_byte()) % bytes.size()] ^= 0xa5u; }
                return true;
            }
        }
    }

    void print_snapshot(::std::vector<cache_file_state> const& snapshot)
    {
        for(auto const& file: snapshot) { ::std::cerr << "  " << file.relative_path << " size=" << file.size << '\n'; }
    }

    [[nodiscard]] bool run_uwvm_from(::std::filesystem::path const& uwvm_path,
                                     ::std::filesystem::path const& artifact_dir,
                                     ::std::filesystem::path const& wasm_path,
                                     ::std::string_view runtime_args,
                                     ::std::string_view cache_args,
                                     ::std::string_view label,
                                     ::std::filesystem::path const& current_dir)
    {
        auto const output_path{artifact_dir / (::std::string{label} + ".out")};
        auto command{quote_argument(uwvm_path) + " " + ::std::string{runtime_args}};
        if(!cache_args.empty()) { command += " " + ::std::string{cache_args}; }
        command += " --run " + quote_argument(wasm_path) + " > " + quote_argument(output_path) + " 2>&1";

        if(!current_dir.empty())
        {
#ifdef _WIN32
            command = "cd /d " + quote_argument(current_dir) + " && " + command;
#else
            command = "cd " + quote_argument(current_dir) + " && " + command;
#endif
        }

        ::std::cout << "[llvm_jit_cache] " << command << '\n';
        auto const status{run_system_command(command)};
        if(status == 0) { return true; }

        ::std::string output{};
        (void)read_text_file(output_path, output);
        ::std::cerr << "uwvm returned non-zero status for " << label << ": " << status << "\n" << output << '\n';
        return false;
    }

    [[nodiscard]] bool run_uwvm(::std::filesystem::path const& uwvm_path,
                                ::std::filesystem::path const& artifact_dir,
                                ::std::filesystem::path const& wasm_path,
                                ::std::string_view runtime_args,
                                ::std::string_view cache_args,
                                ::std::string_view label)
    {
        return run_uwvm_from(uwvm_path, artifact_dir, wasm_path, runtime_args, cache_args, label, {});
    }

    [[nodiscard]] bool run_cached_mode_twice_with_cache_arg(::std::filesystem::path const& uwvm_path,
                                                            ::std::filesystem::path const& artifact_dir,
                                                            ::std::filesystem::path const& wasm_path,
                                                            ::std::filesystem::path const& cache_arg_path,
                                                            ::std::filesystem::path const& cache_snapshot_dir,
                                                            ::std::string_view runtime_args,
                                                            ::std::string_view label,
                                                            ::std::filesystem::path const& current_dir)
    {
        ::std::filesystem::remove_all(cache_snapshot_dir);
        ::std::filesystem::create_directories(cache_snapshot_dir);

        auto const cache_args{::std::string{"--runtime-llvm-jit-cache-path path "} + quote_argument(cache_arg_path)};
        auto runtime_args_with_log{::std::string{runtime_args} + " -Rclog out"};
        if(!run_uwvm_from(
               uwvm_path, artifact_dir, wasm_path, runtime_args_with_log, cache_args, ::std::string{label} + "_first", current_dir))
        {
            return false;
        }

        auto const first{snapshot_cache(cache_snapshot_dir)};
        if(first.empty())
        {
            ::std::cerr << "expected non-empty cache for " << label << '\n';
            return false;
        }

        ::std::this_thread::sleep_for(::std::chrono::milliseconds{1200});
        auto const second_label{::std::string{label} + "_second"};
        if(!run_uwvm_from(uwvm_path, artifact_dir, wasm_path, runtime_args_with_log, cache_args, second_label, current_dir)) { return false; }

        ::std::string second_output{};
        if(!read_text_file(artifact_dir / (second_label + ".out"), second_output))
        {
            ::std::cerr << "failed to read second cache output for " << label << '\n';
            return false;
        }
        if(second_output.find("object-cache-hit") == ::std::string::npos)
        {
            ::std::cerr << "expected object-cache-hit runtime log for " << label << '\n';
            return false;
        }

        auto const second{snapshot_cache(cache_snapshot_dir)};
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

    [[nodiscard]] bool run_cached_mode_twice(::std::filesystem::path const& uwvm_path,
                                             ::std::filesystem::path const& artifact_dir,
                                             ::std::filesystem::path const& wasm_path,
                                             ::std::filesystem::path const& cache_dir,
                                             ::std::string_view runtime_args,
                                             ::std::string_view label)
    {
        return run_cached_mode_twice_with_cache_arg(uwvm_path, artifact_dir, wasm_path, cache_dir, cache_dir, runtime_args, label, {});
    }

    [[nodiscard]] bool expect_rewrite_after_cache_mutation(::std::filesystem::path const& uwvm_path,
                                                           ::std::filesystem::path const& artifact_dir,
                                                           ::std::filesystem::path const& wasm_path,
                                                           ::std::filesystem::path const& cache_dir,
                                                           ::std::string_view cache_args,
                                                           ::std::string_view label,
                                                           bool (*mutate)(::std::vector<unsigned char>&))
    {
        if(!rewrite_first_cache_file(cache_dir, mutate, label)) { return false; }

        auto const run_label{::std::string{"tampered_"} + ::std::string{label}};
        if(!run_uwvm(uwvm_path, artifact_dir, wasm_path, "-Rjit -Rclog out", cache_args, run_label)) { return false; }

        ::std::string output{};
        if(!read_output(artifact_dir, run_label, output))
        {
            ::std::cerr << "failed to read tampered cache output for " << label << '\n';
            return false;
        }
        if(output.find("object-cache-hit") != ::std::string::npos)
        {
            ::std::cerr << "tampered cache unexpectedly hit for " << label << '\n';
            return false;
        }
        if(output.find("object-cache-store") == ::std::string::npos)
        {
            ::std::cerr << "tampered cache did not trigger rewrite for " << label << '\n';
            return false;
        }

        return true;
    }

    [[nodiscard]] bool expect_clean_cache_hit(::std::filesystem::path const& uwvm_path,
                                              ::std::filesystem::path const& artifact_dir,
                                              ::std::filesystem::path const& wasm_path,
                                              ::std::string_view cache_args,
                                              ::std::string_view label)
    {
        if(!run_uwvm(uwvm_path, artifact_dir, wasm_path, "-Rjit -Rclog out", cache_args, label)) { return false; }
        if(!output_contains(artifact_dir, label, "object-cache-hit"))
        {
            ::std::cerr << "expected clean cache hit for " << label << '\n';
            return false;
        }
        return true;
    }

    [[nodiscard]] bool test_signed_cache_integrity(::std::filesystem::path const& uwvm_path,
                                                   ::std::filesystem::path const& artifact_dir,
                                                   ::std::filesystem::path const& wasm_path)
    {
        auto const cache_dir{artifact_dir / "cache-signed-integrity"};
        ::std::filesystem::remove_all(cache_dir);
        ::std::filesystem::create_directories(cache_dir);
        auto const cache_args{::std::string{"--runtime-llvm-jit-cache-path path "} + quote_argument(cache_dir)};

        if(!run_uwvm(uwvm_path, artifact_dir, wasm_path, "-Rjit -Rclog out", cache_args, "signed_integrity_write")) { return false; }
        if(snapshot_cache(cache_dir).empty())
        {
            ::std::cerr << "signed cache integrity setup produced no cache file\n";
            return false;
        }
        if(!expect_clean_cache_hit(uwvm_path, artifact_dir, wasm_path, cache_args, "signed_integrity_clean_hit")) { return false; }

        if(!expect_rewrite_after_cache_mutation(
               uwvm_path, artifact_dir, wasm_path, cache_dir, cache_args, "bad_magic", flip_magic_byte))
        {
            return false;
        }
        if(!expect_rewrite_after_cache_mutation(
               uwvm_path, artifact_dir, wasm_path, cache_dir, cache_args, "truncated", truncate_cache_blob))
        {
            return false;
        }
        if(!expect_rewrite_after_cache_mutation(
               uwvm_path, artifact_dir, wasm_path, cache_dir, cache_args, "bad_context_abi", flip_context_abi_byte))
        {
            return false;
        }
        if(!expect_rewrite_after_cache_mutation(
               uwvm_path, artifact_dir, wasm_path, cache_dir, cache_args, "bad_payload_signature", flip_payload_byte))
        {
            return false;
        }

        return expect_clean_cache_hit(uwvm_path, artifact_dir, wasm_path, cache_args, "signed_integrity_final_hit");
    }

    [[nodiscard]] bool fuzz_first_cache_file(::std::filesystem::path const& cache_dir, ::std::size_t iteration)
    {
        ::std::filesystem::path file{};
        if(!first_cache_file(cache_dir, file))
        {
            ::std::cerr << "missing cache file for fuzz iteration " << iteration << '\n';
            return false;
        }

        ::std::vector<unsigned char> bytes{};
        if(!read_binary_file(file, bytes))
        {
            ::std::cerr << "failed to read cache file for fuzz iteration " << iteration << ": " << file << '\n';
            return false;
        }

        if(!deterministic_fuzz_cache_blob(bytes, iteration))
        {
            ::std::cerr << "deterministic cache fuzz mutation failed at iteration " << iteration << '\n';
            return false;
        }

        if(!write_binary_file(file, bytes))
        {
            ::std::cerr << "failed to write fuzzed cache file at iteration " << iteration << ": " << file << '\n';
            return false;
        }

        return true;
    }

    [[nodiscard]] bool test_cache_fuzz_recovery(::std::filesystem::path const& uwvm_path,
                                                ::std::filesystem::path const& artifact_dir,
                                                ::std::filesystem::path const& wasm_path)
    {
        auto const cache_dir{artifact_dir / "cache-fuzz"};
        ::std::filesystem::remove_all(cache_dir);
        ::std::filesystem::create_directories(cache_dir);
        auto const cache_args{::std::string{"--runtime-llvm-jit-cache-path path "} + quote_argument(cache_dir)};

        if(!run_uwvm(uwvm_path, artifact_dir, wasm_path, "-Rjit -Rclog out", cache_args, "fuzz_seed_write")) { return false; }
        if(snapshot_cache(cache_dir).empty())
        {
            ::std::cerr << "cache fuzz setup produced no cache file\n";
            return false;
        }

        constexpr ::std::size_t fuzz_iterations{12uz};
        for(::std::size_t iteration{}; iteration != fuzz_iterations; ++iteration)
        {
            if(!fuzz_first_cache_file(cache_dir, iteration)) { return false; }

            auto const label{::std::string{"fuzz_recovery_"} + ::std::to_string(iteration)};
            if(!run_uwvm(uwvm_path, artifact_dir, wasm_path, "-Rjit -Rclog out", cache_args, label)) { return false; }

            ::std::string output{};
            if(!read_output(artifact_dir, label, output))
            {
                ::std::cerr << "failed to read fuzz output for iteration " << iteration << '\n';
                return false;
            }
            if(output.find("object-cache-hit") != ::std::string::npos)
            {
                ::std::cerr << "fuzzed cache unexpectedly hit at iteration " << iteration << '\n';
                return false;
            }
            if(output.find("object-cache-store") == ::std::string::npos)
            {
                ::std::cerr << "fuzzed cache did not rewrite at iteration " << iteration << '\n';
                return false;
            }
        }

        return expect_clean_cache_hit(uwvm_path, artifact_dir, wasm_path, cache_args, "fuzz_final_hit");
    }

    [[nodiscard]] bool test_cache_path_modes(::std::filesystem::path const& uwvm_path,
                                             ::std::filesystem::path const& artifact_dir,
                                             ::std::filesystem::path const& wasm_path)
    {
        auto const custom_absolute_cache{artifact_dir / "cache path variants" / "abs space;semi" / "inner"};
        if(!run_cached_mode_twice(
               uwvm_path, artifact_dir, wasm_path, custom_absolute_cache, "-Rjit", "cache_path_absolute_space_semi"))
        {
            return false;
        }

        auto const trailing_base{artifact_dir / "cache-trailing-separator"};
        auto trailing_arg{trailing_base.string()};
        if(trailing_arg.empty() || trailing_arg.back() != ::std::filesystem::path::preferred_separator)
        {
            trailing_arg.push_back(::std::filesystem::path::preferred_separator);
        }
        if(!run_cached_mode_twice_with_cache_arg(uwvm_path,
                                                 artifact_dir,
                                                 wasm_path,
                                                 ::std::filesystem::path{trailing_arg},
                                                 trailing_base,
                                                 "-Rjit",
                                                 "cache_path_trailing_separator",
                                                 {}))
        {
            return false;
        }

        auto const relative_cwd{artifact_dir / "relative-cache-cwd"};
        auto const relative_arg{::std::filesystem::path{"relative cache;semi"} / "nested"};
        auto const relative_cache{relative_cwd / relative_arg};
        if(!run_cached_mode_twice_with_cache_arg(
               uwvm_path, artifact_dir, wasm_path, relative_arg, relative_cache, "-Rjit", "cache_path_relative_space_semi", relative_cwd))
        {
            return false;
        }

        auto const empty_path_cwd{artifact_dir / "empty-cache-path-cwd"};
        if(!run_cached_mode_twice_with_cache_arg(uwvm_path,
                                                 artifact_dir,
                                                 wasm_path,
                                                 {},
                                                 empty_path_cwd,
                                                 "-Rjit",
                                                 "cache_path_empty_uses_cwd",
                                                 empty_path_cwd))
        {
            return false;
        }

        auto const blocked_cache{artifact_dir / "cache-path-blocked-file"};
        ::std::filesystem::remove_all(blocked_cache);
        {
            ::std::ofstream blocked_file(blocked_cache, ::std::ios::binary | ::std::ios::trunc);
            blocked_file << "not a directory";
        }
        auto const blocked_cache_args{::std::string{"--runtime-llvm-jit-cache-path path "} + quote_argument(blocked_cache)};
        if(!run_uwvm(uwvm_path, artifact_dir, wasm_path, "-Rjit -Rclog out", blocked_cache_args, "cache_path_blocked_first"))
        {
            return false;
        }
        if(!run_uwvm(uwvm_path, artifact_dir, wasm_path, "-Rjit -Rclog out", blocked_cache_args, "cache_path_blocked_second"))
        {
            return false;
        }
        if(output_contains(artifact_dir, "cache_path_blocked_second", "object-cache-hit"))
        {
            ::std::cerr << "blocked regular-file cache path unexpectedly produced a cache hit\n";
            return false;
        }
        if(!output_contains(artifact_dir, "cache_path_blocked_second", "object-cache-store-complete") ||
           !output_contains(artifact_dir, "cache_path_blocked_second", "status=io-error"))
        {
            ::std::cerr << "blocked regular-file cache path did not report the real async write failure\n";
            return false;
        }
        if(!::std::filesystem::is_regular_file(blocked_cache))
        {
            ::std::cerr << "blocked cache path was modified instead of remaining a regular file\n";
            return false;
        }

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

        ::std::this_thread::sleep_for(::std::chrono::milliseconds{1200});
        if(!run_uwvm(uwvm_path, artifact_dir, wasm_path, "-Rtiered -Rtiered-disable-t0", cache_args, "shared_tiered_no_t0"))
        {
            return false;
        }
        auto const tiered_snapshot{snapshot_cache(cache_dir)};
        if(tiered_snapshot != aot_snapshot)
        {
            ::std::cerr << "tiered/no-T0 did not reuse the existing lazy LLVM cache object\nafter aot:\n";
            print_snapshot(aot_snapshot);
            ::std::cerr << "tiered/no-T0:\n";
            print_snapshot(tiered_snapshot);
            return false;
        }

        return true;
    }

    struct cache_runtime_mode
    {
        ::std::string_view label{};
        ::std::string_view args{};
    };

    inline constexpr ::std::array cache_runtime_modes{
        cache_runtime_mode{"lazy", "-Rjit"},
        cache_runtime_mode{"lazy_verify", "-Rcm lazy+verification -Rcc jit"},
        cache_runtime_mode{"full", "-Rcm full -Rcc jit"},
        cache_runtime_mode{"aot", "-Raot"},
        cache_runtime_mode{"tiered_no_t0", "-Rtiered -Rtiered-disable-t0"}};

    [[nodiscard]] bool test_wasm_cache_matrix(::std::filesystem::path const& uwvm_path,
                                              ::std::filesystem::path const& artifact_dir,
                                              ::std::vector<wasm_fixture_file> const& fixtures)
    {
        for(auto const& fixture: fixtures)
        {
            for(auto const& mode: cache_runtime_modes)
            {
                auto const label{::std::string{"matrix_"} + fixture.label + "_" + ::std::string{mode.label}};
                auto const cache_dir{artifact_dir / ("cache-" + label)};
                if(!run_cached_mode_twice(uwvm_path, artifact_dir, fixture.path, cache_dir, mode.args, label)) { return false; }
            }
        }

        return true;
    }

    [[nodiscard]] bool test_wasm1p1_feature_cache_smoke(::std::filesystem::path const& uwvm_path,
                                                        ::std::filesystem::path const& artifact_dir)
    {
        auto const scalar_wasm_path{artifact_dir / "wasm1p1_scalar_start.wasm"};
        if(!write_fixture(scalar_wasm_path, wasm1p1_scalar_start_wasm.data(), wasm1p1_scalar_start_wasm.size())) { return false; }

        auto const scalar_cache_dir{artifact_dir / "cache-wasm1p1-feature-smoke"};
        constexpr ::std::string_view wasm1p1_feature_args{
            "-Rjit --wasm-feature-enable-sign-extension --wasm-feature-enable-nontrapping-float-to-int"};
        if(!run_cached_mode_twice(uwvm_path,
                                  artifact_dir,
                                  scalar_wasm_path,
                                  scalar_cache_dir,
                                  wasm1p1_feature_args,
                                  "wasm1p1_feature_smoke"))
        {
            return false;
        }

        auto const multivalue_wasm_path{artifact_dir / "wasm1p1_multivalue_start.wasm"};
        if(!write_fixture(multivalue_wasm_path, wasm1p1_multivalue_start_wasm.data(), wasm1p1_multivalue_start_wasm.size())) { return false; }

        auto const multivalue_cache_dir{artifact_dir / "cache-wasm1p1-multivalue-smoke"};
        constexpr ::std::string_view wasm1p1_multivalue_feature_args{"-Rjit --wasm-feature-enable-multi-value"};
        return run_cached_mode_twice(uwvm_path,
                                     artifact_dir,
                                     multivalue_wasm_path,
                                     multivalue_cache_dir,
                                     wasm1p1_multivalue_feature_args,
                                     "wasm1p1_multivalue_smoke");
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
                     "-Rjit -Rclog out",
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
                     "-Rjit -Rclog out",
                     cache_args + " --runtime-llvm-jit-cache-no-sign --runtime-llvm-jit-cache-no-verify",
                     "unsigned_no_verify_reuse"))
        {
            return false;
        }
        if(!output_contains(artifact_dir, "unsigned_no_verify_reuse", "object-cache-hit"))
        {
            ::std::cerr << "unsigned cache was not reused when no-verify was enabled\n";
            return false;
        }
        if(!output_contains(artifact_dir, "unsigned_no_verify_reuse", "signature_verified=0"))
        {
            ::std::cerr << "unsigned no-verify cache hit did not report signature_verified=0\n";
            return false;
        }
        auto const no_verify_snapshot{snapshot_cache(cache_dir)};
        if(unsigned_snapshot != no_verify_snapshot)
        {
            ::std::cerr << "unsigned cache was rewritten when no-verify was enabled\n";
            return false;
        }

        ::std::this_thread::sleep_for(::std::chrono::milliseconds{1200});
        if(!run_uwvm(uwvm_path, artifact_dir, wasm_path, "-Rjit -Rclog out", cache_args, "unsigned_verify_rewrite")) { return false; }
        if(output_contains(artifact_dir, "unsigned_verify_rewrite", "object-cache-hit"))
        {
            ::std::cerr << "signed verification unexpectedly reused unsigned cache object\n";
            return false;
        }
        if(!output_contains(artifact_dir, "unsigned_verify_rewrite", "object-cache-store"))
        {
            ::std::cerr << "signed verification did not rewrite unsigned cache object\n";
            return false;
        }
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
    ::std::vector<wasm_fixture_file> fixtures{};
    if(!write_cache_fixtures(artifact_dir, fixtures)) { return 1; }
    if(fixtures.empty())
    {
        ::std::cerr << "no wasm cache fixtures were written\n";
        return 1;
    }
    auto const& wasm_path{fixtures.front().path};

    if(!test_cache_path_modes(uwvm_path, artifact_dir, wasm_path)) { return 1; }
    if(!test_wasm_cache_matrix(uwvm_path, artifact_dir, fixtures)) { return 1; }
    if(!test_wasm1p1_feature_cache_smoke(uwvm_path, artifact_dir)) { return 1; }
    if(!test_default_tiered_smoke(uwvm_path, artifact_dir, wasm_path)) { return 1; }
    if(!test_signed_cache_integrity(uwvm_path, artifact_dir, wasm_path)) { return 1; }
    if(!test_cache_fuzz_recovery(uwvm_path, artifact_dir, wasm_path)) { return 1; }
    if(!test_shared_cache_key_isolation(uwvm_path, artifact_dir, wasm_path)) { return 1; }
    if(!test_unsigned_cache_policy(uwvm_path, artifact_dir, wasm_path)) { return 1; }

    return 0;
}
