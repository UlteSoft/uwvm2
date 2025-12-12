// Xxh3UtfGen.cc
//
// Generates a UTF-8 style buffer (same content as the old combined benchmark)
// and writes it to a file. This is meant to be run once before the per-impl
// benchmarks so both implementations hash identical bytes in separate processes.
//
// Environment variables:
//   BYTES     : total size of the buffer to generate (default: 16MiB)
//   DATA_FILE : output path (default: "xxh3_utf_data.bin")

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{
    // Read an environment variable as size_t; fall back to a default if missing or invalid.
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

    // Create a UTF-8 style buffer by repeating a fixed base text that mixes
    // ASCII and multi-byte UTF-8 code points, until the requested size.
    std::vector<std::byte> make_utf8_buffer(std::size_t total_bytes)
    {
        std::u8string base_text =
            u8"Hello, 世界 — uwvm2 xxh3 UTF-8 benchmark.\n"
            u8"这里是一些中文字符，用来测试多字节 UTF-8 序列。\n"
            u8"また、いくつかの日本語テキストも含めます。\n"
            u8"Some ASCII-only lines as well to match typical WASI/UTF traffic.\n"
            u8"🚀✨ Unicode symbols and emoji-like code points.\n";

        std::vector<std::byte> buf;
        buf.reserve(total_bytes);

        while(buf.size() < total_bytes)
        {
            for(char8_t ch: base_text)
            {
                if(buf.size() >= total_bytes) { break; }
                buf.push_back(static_cast<std::byte>(ch));
            }
        }

        return buf;
    }

    void write_file(std::string const& path, std::vector<std::byte> const& buf)
    {
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if(!f)
        {
            std::perror("fopen");
            std::fprintf(stderr, "failed to open DATA_FILE for write: %s\n", path.c_str());
            std::exit(1);
        }

        std::size_t const n = std::fwrite(buf.data(), 1, buf.size(), f);
        if(n != buf.size())
        {
            std::perror("fwrite");
            std::fprintf(stderr, "short write to DATA_FILE: %s (%zu/%zu)\n", path.c_str(), n, buf.size());
            std::fclose(f);
            std::exit(1);
        }

        std::fclose(f);
    }
}  // namespace

int main()
{
    std::size_t const total_bytes = read_env_size("BYTES", 16u * 1024u * 1024u);
    std::string const data_file = read_env_string("DATA_FILE", "xxh3_utf_data.bin");

    std::puts("xxh3 UTF-8 data generator");
    std::printf("  total_bytes = %zu\n", total_bytes);
    std::printf("  data_file   = %s\n", data_file.c_str());

    auto const buffer{make_utf8_buffer(total_bytes)};
    write_file(data_file, buffer);

    std::printf("xxh3_utf_gen bytes=%zu file=%s\n", buffer.size(), data_file.c_str());
    return 0;
}

