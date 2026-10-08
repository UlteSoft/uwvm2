// Read actual LLVM-emitted Mach-O objects; mutations affect owned object DATA.
// No loaded addresses, executable views, host VM inspection or owner grants.
#define UWVM_RUNTIME_LLVM_JIT 1
#include <uwvm2/runtime/lib/uwvm_runtime_native_owner_function_claims.h>
#include <fast_io.h>
#include <fast_io_dsal/string.h>
#include <llvm/Support/MemoryBufferRef.h>
#include <cstddef>
#include <cstring>

namespace ledger = uwvm2::runtime::lib::details::native_owner_function_claims;
#define CHECK(x) do { ++checks; if(!(x)) { fast_io::io::perrln("Mach-O section anchor FAIL line=", __LINE__); return 1; } } while(false)
int main(int argc, char** argv)
{
    if(argc != 3) { return 64; }
    unsigned checks{};
    auto input_path{fast_io::concat_fast_io(fast_io::mnp::os_c_str(argv[1]))};
    auto alias_path{fast_io::concat_fast_io(fast_io::mnp::os_c_str(argv[2]))};
    fast_io::native_file_loader mapped{input_path, fast_io::open_mode::in};
    fast_io::native_file_loader alias{alias_path, fast_io::open_mode::in};
    CHECK(mapped.size() != 0u && mapped.size() <= 64u * 1024u * 1024u && alias.size() != 0u);
    auto parse = [](char const* data, std::size_t size)
    {
        return llvm::object::ObjectFile::createObjectFile(llvm::MemoryBufferRef{
            llvm::StringRef{data, size}, "actual-Mach-O-anchor-regression"});
    };
    auto original{parse(mapped.data(), mapped.size())};
    if(!original) { llvm::consumeError(original.takeError()); return 2; }
    auto const* macho{llvm::dyn_cast<llvm::object::MachOObjectFile>(original->get())};
    CHECK(macho != nullptr && macho->is64Bit() && macho->isLittleEndian() && macho->getArch() == llvm::Triple::aarch64);
    ledger::image positive{};
    CHECK(ledger::collect(*macho, positive) && positive.endpoints_unambiguous() && positive.bodies.size() == 4u);
    auto extra{parse(alias.data(), alias.size())};
    if(!extra) { llvm::consumeError(extra.takeError()); return 3; }
    ledger::image negative{};
    CHECK(ledger::collect(**extra, negative) && negative.complete && !negative.endpoints_unambiguous() &&
          (negative.blockers & static_cast<unsigned>(ledger::blocker::unexpected_alias)) != 0u);
    auto const table{macho->getSymtabLoadCommand()};
    CHECK(table.symoff <= mapped.size() && table.nsyms <= (mapped.size() - table.symoff) / sizeof(llvm::MachO::nlist_64));
    std::size_t anchor{}, other{}, anchor_name{};
    bool found{}, found_other{};
    for(auto const& symbol : macho->symbols())
    {
        auto name{symbol.getName()};
        if(!name) { llvm::consumeError(name.takeError()); return 4; }
        auto const index{macho->getSymbolIndex(symbol.getRawDataRefImpl())};
        CHECK(index < table.nsyms);
        auto const row{macho->getSymbol64TableEntry(symbol.getRawDataRefImpl())};
        auto const where{static_cast<std::size_t>(table.symoff) + static_cast<std::size_t>(index) * sizeof(row)};
        if(*name == "ltmp0")
        {
            CHECK(!found && row.n_type == llvm::MachO::N_SECT && row.n_desc == 0u && row.n_sect == 1u && row.n_value == 0u);
            anchor = where; found = true;
            CHECK(table.stroff <= mapped.size() && row.n_strx < table.strsize && row.n_strx < mapped.size() - table.stroff);
            anchor_name = static_cast<std::size_t>(table.stroff) + row.n_strx;
        }
        else if(*name == "ltmp1") { other = where; found_other = true; }
    }
    CHECK(found && found_other);
    auto declined = [&](auto mutate)
    {
        std::vector<unsigned char> bytes(mapped.size());
        std::memcpy(bytes.data(), mapped.data(), mapped.size());
        mutate(bytes);
        auto object{parse(reinterpret_cast<char const*>(bytes.data()), bytes.size())};
        if(!object) { llvm::consumeError(object.takeError()); return false; }
        ledger::image observed{};
        return ledger::collect(**object, observed) && observed.complete && !observed.endpoints_unambiguous() &&
            (observed.blockers & static_cast<unsigned>(ledger::blocker::unexpected_alias)) != 0u;
    };
    // Proven table extent/index precede every fixed-width mutation below.
    CHECK(declined([&](auto& b) { b[anchor + offsetof(llvm::MachO::nlist_64, n_type)] |= llvm::MachO::N_EXT; }));
    CHECK(declined([&](auto& b) {
        std::uint16_t value{llvm::MachO::N_WEAK_DEF};
        std::memcpy(b.data() + anchor + offsetof(llvm::MachO::nlist_64, n_desc), &value, sizeof(value));
    }));
    CHECK(declined([&](auto& b) {
        std::uint64_t value{4u};
        std::memcpy(b.data() + anchor + offsetof(llvm::MachO::nlist_64, n_value), &value, sizeof(value));
    }));
    CHECK(declined([&](auto& b) { b[anchor_name] = 'x'; }));
    CHECK(declined([&](auto& b) {
        llvm::MachO::nlist_64 value{};
        std::memcpy(&value, b.data() + other, sizeof(value));
        value.n_type = llvm::MachO::N_SECT; value.n_sect = 1u; value.n_desc = 0u; value.n_value = 0u;
        std::memcpy(b.data() + other, &value, sizeof(value));
    }));
    fast_io::io::println("Mach-O section anchor PASS checks=", checks, " execution-ownership=false");
}
#undef CHECK
