// UtfGen.cc
//
// Generates a valid UTF-8 workload into a file. This is meant to be run once
// before the per-implementation benchmarks so both implementations validate
// identical bytes in separate processes.
//
// Environment variables:
//   SCENARIO       : one of "ascii_only", "latin_mixed", "emoji_mixed"
//                    (default: "ascii_only")
//   UTF_BENCH_BYTES: approximate bytes of output buffer (default: 16MiB)
//   DATA_FILE      : output file path (default: "utf_data.bin")

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{
    std::size_t read_env_size(char const* name, std::size_t default_value)
    {
        if(char const* s = std::getenv(name))
        {
            char* end{};
            unsigned long long v = std::strtoull(s, &end, 10);
            if(end && *end == '\0' && v > 0) { return static_cast<std::size_t>(v); }
        }
        return default_value;
    }

    std::string read_env_string(char const* name, char const* default_value)
    {
        if(char const* s = std::getenv(name))
        {
            if(*s != '\0') { return std::string{s}; }
        }
        return std::string{default_value};
    }

    std::u8string make_ascii_chunk()
    {
        return u8"Hello, world! 0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZ "
               u8"abcdefghijklmnopqrstuvwxyz /tmp/www/index.html "
               u8"-- uwvm2 utf8 ascii-only benchmark payload --\n";
    }

    std::u8string make_latin_chunk()
    {
        return u8"caf\u00E9, cr\u00E8me br\u00FBl\u00E9e, Jahrtausendwende, "
               u8"\u00DCber, Stra\u00DFe, Espa\u00F1a, Portugu\u00EAs, "
               u8"uwvm2 utf8 latin-mixed payload.\n";
    }

    std::u8string make_emoji_chunk()
    {
        return u8"Emoji: \U0001F600 \U0001F602 \U0001F603 \U0001F604 "
               u8"Rocket: \U0001F680 Explosion: \U0001F4A5 "
               u8"CJK: \u4E2D\u6587\u6D4B\u8BD5 UTF-8 benchmark uwvm2.\n";
    }

    std::vector<std::byte> make_buffer(std::string const& scenario, std::size_t target_bytes)
    {
        std::u8string chunk;
        if(scenario == "ascii_only")
        {
            chunk = make_ascii_chunk();
        }
        else if(scenario == "latin_mixed")
        {
            chunk = make_latin_chunk();
        }
        else if(scenario == "emoji_mixed")
        {
            chunk = make_emoji_chunk();
        }
        else
        {
            std::fprintf(stderr, "UtfGen: unknown SCENARIO: %s\n", scenario.c_str());
            std::exit(1);
        }

        if(chunk.empty())
        {
            std::fprintf(stderr, "UtfGen: chunk unexpectedly empty (scenario=%s)\n", scenario.c_str());
            std::exit(1);
        }

        std::size_t const chunk_size{chunk.size()};
        std::size_t repeats = target_bytes / chunk_size;
        if(repeats == 0) { repeats = 1; }

        std::vector<std::byte> buffer;
        buffer.reserve(chunk_size * repeats);

        for(std::size_t i{}; i < repeats; ++i)
        {
            for(char8_t ch : chunk)
            {
                buffer.push_back(static_cast<std::byte>(ch));
            }
        }

        return buffer;
    }

    void write_file(std::string const& path, std::vector<std::byte> const& buf)
    {
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if(!f)
        {
            std::perror("fopen");
            std::fprintf(stderr, "UtfGen: failed to open DATA_FILE for write: %s\n", path.c_str());
            std::exit(1);
        }

        std::size_t const n = std::fwrite(buf.data(), 1, buf.size(), f);
        if(n != buf.size())
        {
            std::perror("fwrite");
            std::fprintf(stderr, "UtfGen: short write to DATA_FILE: %s (%zu/%zu)\n", path.c_str(), n, buf.size());
            std::fclose(f);
            std::exit(1);
        }

        std::fclose(f);
    }
}  // namespace

int main()
{
    std::string const scenario = read_env_string("SCENARIO", "ascii_only");
    std::size_t const target_bytes = read_env_size("UTF_BENCH_BYTES", 16u * 1024u * 1024u);
    std::string const data_file = read_env_string("DATA_FILE", "utf_data.bin");

    auto const buffer{make_buffer(scenario, target_bytes)};
    write_file(data_file, buffer);

    std::printf("uwvm2_utf_gen scenario=%s bytes=%zu file=%s\n",
                scenario.c_str(),
                buffer.size(),
                data_file.c_str());
    return 0;
}

