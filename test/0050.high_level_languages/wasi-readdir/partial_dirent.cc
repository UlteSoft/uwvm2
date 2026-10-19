// Real production fd_readdir entry points, with a native two-page guest memory.
// The Python driver supplies a private directory of forty 125-byte file names.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <set>
#include <string>
#include <vector>
#include <sys/stat.h>

#if HLL_WASM64
# include <uwvm2/imported/wasi/wasip1/func/fd_readdir_wasm64.h>
#else
# include <uwvm2/imported/wasi/wasip1/func/fd_readdir.h>
#endif

namespace abi = uwvm2::imported::wasi::wasip1::abi;
using memory_t = uwvm2::object::memory::linear::native_memory_t;
using environment_t = uwvm2::imported::wasi::wasip1::environment::wasip1_environment<memory_t>;
#if HLL_WASM64
using size_t_wasi = abi::wasi_size_wasm64_t;
using ptr_t_wasi = abi::wasi_void_ptr_wasm64_t;
using cookie_t = abi::dircookie_wasm64_t;
#else
using size_t_wasi = abi::wasi_size_t;
using ptr_t_wasi = abi::wasi_void_ptr_t;
using cookie_t = abi::dircookie_t;
#endif

constexpr std::size_t memory_bytes = 2 * 65536;
constexpr std::size_t result_at = 2048;
constexpr unsigned char canary = 0xa5;
std::size_t checks{};

void require(bool ok, char const* message)
{
    ++checks;
    if(!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

std::uint64_t load_le(unsigned char const* p, std::size_t n)
{
    std::uint64_t value{};
    for(std::size_t i{}; i != n; ++i) { value |= std::uint64_t(p[i]) << (8 * i); }
    return value;
}

struct record
{
    std::size_t offset{};
    std::uint64_t next{}, ino{};
    std::string name;
    unsigned char type{};
    std::vector<unsigned char> canonical;
};

std::vector<record> parse_complete(std::vector<unsigned char> const& bytes)
{
    std::vector<record> records;
    for(std::size_t at{}; bytes.size() - at >= 24;)
    {
        auto const* p = bytes.data() + at;
        auto const length = load_le(p + 16, 4);
        if(length > bytes.size() - at - 24) { break; }
        record r{.offset = at, .next = load_le(p, 8), .ino = load_le(p + 8, 8),
                 .name = std::string(reinterpret_cast<char const*>(p + 24), length), .type = p[20], .canonical = {}};
        r.canonical.assign(p, p + 24 + length);
        // Padding is not interpreted as part of the WASI fields. The candidate
        // additionally defines it as zero, independently checked below.
        r.canonical[21] = r.canonical[22] = r.canonical[23] = 0;
        at += 24 + length;
        records.push_back(std::move(r));
    }
    return records;
}

auto call(environment_t& env, std::size_t at, std::size_t capacity, std::uint64_t cookie)
{
#if HLL_WASM64
    return uwvm2::imported::wasi::wasip1::func::fd_readdir_wasm64(
        env, 4, static_cast<ptr_t_wasi>(at), static_cast<size_t_wasi>(capacity),
        static_cast<cookie_t>(cookie), static_cast<ptr_t_wasi>(result_at));
#else
    return uwvm2::imported::wasi::wasip1::func::fd_readdir(
        env, 4, static_cast<ptr_t_wasi>(at), static_cast<size_t_wasi>(capacity),
        static_cast<cookie_t>(cookie), static_cast<ptr_t_wasi>(result_at));
#endif
}

std::vector<unsigned char> read(environment_t& env, memory_t& memory,
                                std::size_t capacity, std::uint64_t cookie, bool memory_end = false)
{
    auto* raw = reinterpret_cast<unsigned char*>(memory.memory_begin);
    auto const at = memory_end ? memory_bytes - capacity : 4096;
    require(at >= 16 && at + capacity <= memory_bytes, "test buffer bounds");
    std::memset(raw + at - 16, canary, capacity + 16 + (memory_end ? 0 : 16));
    std::memset(raw + result_at - 16, canary, sizeof(size_t_wasi) + 32);
    auto const error = call(env, at, capacity, cookie);
    require(error == abi::errno_t::esuccess, "fd_readdir returned non-success");
    auto const used = load_le(raw + result_at, sizeof(size_t_wasi));
    require(used <= capacity, "buf_used exceeds requested length");
    for(std::size_t i{}; i != 16; ++i)
    {
        require(raw[at - 16 + i] == canary, "prefix guard changed");
        if(!memory_end) { require(raw[at + capacity + i] == canary, "suffix guard changed"); }
        require(raw[result_at - 16 + i] == canary, "buf_used prefix guard changed");
        require(raw[result_at + sizeof(size_t_wasi) + i] == canary, "buf_used suffix guard changed");
    }
    for(std::size_t i = static_cast<std::size_t>(used); i != capacity; ++i)
    { require(raw[at + i] == canary, "bytes beyond buf_used changed"); }
    return {raw + at, raw + at + used};
}

int main(int argc, char** argv)
{
    memory_t memory{};
    memory.init_by_page_count(2uz);
    environment_t env{.wasip1_memory = std::addressof(memory), .argv = {}, .envs = {},
                      .fd_storage = {}, .mount_dir_roots = {}, .trace_wasip1_call = false};
    env.fd_storage.opens.resize(10uz);
    auto& fd = *env.fd_storage.opens.index_unchecked(4uz).fd_p;
    fd.rights_base = static_cast<abi::rights_t>(-1);
    fd.rights_inherit = static_cast<abi::rights_t>(-1);
    fd.wasi_fd.ptr->wasi_fd_storage.reset_type(uwvm2::imported::wasi::wasip1::fd_manager::wasi_fd_type_e::dir);
    uwvm2::imported::wasi::wasip1::fd_manager::dir_stack_entry_ref_t entry{};
    entry.ptr->dir_stack.storage.file = fast_io::dir_file{u8"."};
    fd.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack.dir_stack.push_back(std::move(entry));

    if(argc == 2 && std::strcmp(argv[1], "--oob") == 0)
    {
        std::fputs("ABOUT_TO_CALL_OOB\n", stderr);
        std::fflush(stderr);
        (void)call(env, memory_bytes - 22, 23, 2);
        std::fputs("FAIL: OOB call returned\n", stderr);
        return 1;
    }

    auto const all = read(env, memory, 16384, 0);
    auto const records = parse_complete(all);
    require(records.size() == 42, "fixture must enumerate dot, dotdot and forty files");
    require(records.front().name == "." && records[1].name == "..", "synthetic root entries");
    require(records.back().offset + records.back().canonical.size() == all.size(), "full reference has no partial tail");
    std::set<std::string> names;
    for(std::size_t i{}; i != records.size(); ++i)
    {
        auto const& r = records[i];
        require(r.next == i + 1, "ordinal retry cookie");
        require(names.insert(r.name).second, "duplicate name");
        require(i < 2 || r.name.size() == 125, "file name size fixes 4096-byte tail independently of filesystem order");
        struct stat st{};
        require(::stat(r.name.c_str(), &st) == 0, "native fixture stat");
        require(r.ino == (i == 1 ? 0 : std::uint64_t(st.st_ino)), "inode/root dotdot contract");
        require(r.type == (i < 2 ? 3 : 4), "directory/regular file type bytes");
    }
    require(records[29].offset == 4074, "4096-byte regression must leave exactly 22 bytes");
    auto const short_page = read(env, memory, 4096, 0);
    if(argc == 2 && std::strcmp(argv[1], "--expect-old-short-page") == 0)
    {
        // This negative control deliberately does not require zero padding or
        // other candidate behavior. It proves the original premature EOF only.
        require(short_page.size() == 4074, "old implementation no longer reproduces short-page EOF");
        require(parse_complete(short_page).size() == 29, "old complete-entry count");
        std::puts("EXPECTED_FALSE_EOF used=4074 capacity=4096 complete=29 total=42 remaining_header=22");
        return 42;
    }
    require(short_page.size() == 4096, "4096-byte page falsely reports EOF before all entries");

    std::vector<unsigned char> canonical;
    for(auto const& r : records) { canonical.insert(canonical.end(), r.canonical.begin(), r.canonical.end()); }
    require(all == canonical, "full LE header bytes and zero padding");
    require(std::equal(short_page.begin(), short_page.end(), canonical.begin()), "4096-byte page actual entry prefix");

    // All partial headers, exact headers, all name prefixes and one byte of the
    // next record: dot/dotdot/normal and last normal record, also at memory end.
    for(std::size_t index : {0uz, 1uz, 2uz, 29uz, 41uz})
    {
        auto const& r = records[index];
        for(std::size_t capacity{}; capacity <= r.canonical.size() + 1; ++capacity)
        {
            auto const expected = std::min(capacity, canonical.size() - r.offset);
            for(bool memory_end : {false, true})
            {
                auto const bytes = read(env, memory, capacity, index, memory_end);
                require(bytes.size() == expected, "partial entry must fill available bytes before EOF");
                require(std::equal(bytes.begin(), bytes.end(), canonical.begin() + r.offset), "partial header/name bytes");
            }
            if(capacity < r.canonical.size())
            {
                auto const retry = read(env, memory, r.canonical.size(), index);
                require(retry == r.canonical, "same last-complete cookie retries the entire truncated record");
            }
        }
    }

    // A complete entry followed by every 1..23-byte partial header exercises
    // the previously false EOF at dotdot, first normal and later normal paths.
    for(std::size_t first : {0uz, 1uz, 2uz})
    {
        auto const& r = records[first];
        for(std::size_t tail = 1; tail != 24; ++tail)
        {
            auto const bytes = read(env, memory, r.canonical.size() + tail, first);
            require(bytes.size() == r.canonical.size() + tail, "short header after complete entry");
            require(std::equal(bytes.begin(), bytes.end(), canonical.begin() + r.offset), "tail actual bytes");
            auto const complete = parse_complete(bytes);
            require(complete.size() == 1 && complete.back().next == first + 1, "partial entry does not advance consumer cookie");
            auto const retry = read(env, memory, records[first + 1].canonical.size(), complete.back().next);
            require(retry == records[first + 1].canonical, "retry from last complete cookie after partial header");
        }
    }
    for(auto cookie : {std::uint64_t{42}, std::uint64_t{43}, std::numeric_limits<std::uint64_t>::max()})
    { require(read(env, memory, 128, cookie).empty(), "EOF/beyond-end cookie must write zero bytes"); }

    std::vector<unsigned char> paginated;
    std::uint64_t cookie{};
    for(std::size_t page{}; page != 10; ++page)
    {
        auto const bytes = read(env, memory, 4096, cookie);
        auto const complete = parse_complete(bytes);
        for(auto const& r : complete)
        {
            paginated.insert(paginated.end(), r.canonical.begin(), r.canonical.end());
            cookie = r.next;
        }
        if(bytes.size() < 4096) { break; }
        require(!complete.empty(), "pagination must make progress");
    }
    require(paginated == canonical && cookie == 42, "libc-style short-page EOF pagination loses no names");
    std::printf("fd_readdir wasm%d partial-page contract: PASS checks=%zu records=42 tail=22\n", HLL_WASM64 ? 64 : 32, checks);
}
