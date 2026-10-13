// Actual emitted ELF claim-ledger regression; DATA only, no loaded addresses.
#define UWVM_RUNTIME_LLVM_JIT 1
#include <uwvm2/runtime/lib/uwvm_runtime_native_owner_function_claims.h>
#include <uwvm2/runtime/lib/uwvm_runtime_native_provenance_rows.h>
#include <llvm/Support/MemoryBufferRef.h>
#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/DebugInfo/DWARF/DWARFDie.h>
#include <llvm/DebugInfo/DWARF/DWARFFormValue.h>
#include <fast_io.h>
#include <limits>
#include <array>
namespace ledger=uwvm2::runtime::lib::details::native_owner_function_claims;
#define CHECK(x) do { if(!(x)) { fast_io::io::perrln("native mapping metadata FAIL line=",__LINE__);return 1; } } while(false)
// Fixed relocation DATA for this offline actual-object parser test only.
// These integers are never dereferenced and issue no stopped-frame credential.
struct offline_sections final : llvm::LoadedObjectInfo
{
    std::unique_ptr<llvm::LoadedObjectInfo> clone() const override { return std::make_unique<offline_sections>(); }
    std::uint64_t getSectionLoadAddress(llvm::object::SectionRef const& section) const override
    { return section.isText() && section.getIndex()<64u ? 0x10000000u+section.getIndex()*0x1000000u:0u; }
};
int main(int argc,char** argv)
{
    CHECK(argc==2);
    fast_io::native_file file{fast_io::mnp::os_c_str(argv[1]),fast_io::open_mode::in};
    auto const size{fast_io::file_size(file)};
    CHECK(size!=0u && size<=64u*1024u*1024u && size<=PTRDIFF_MAX);
    fast_io::native_file_loader mapped{fast_io::at(file)};CHECK(mapped.size()==size);
    auto object{llvm::object::ObjectFile::createObjectFile(llvm::MemoryBufferRef{
        llvm::StringRef{mapped.data(),mapped.size()},"actual-native-mapping-metadata"})};
    if(!object) { llvm::consumeError(object.takeError());return 2; }
    // A loader-only success must not conceal rejected provenance relocations.
    // Inspect the genuine emitted object's DATA, with no fabricated load map.
    bool malformed{};
    auto error{[&](llvm::Error failure) { malformed = true; llvm::consumeError(std::move(failure)); }};
    auto dwarf{llvm::DWARFContext::create(**object,llvm::DWARFContext::ProcessDebugRelocations::Process,nullptr,"",error,error)};
    CHECK(dwarf && !malformed);
    unsigned public_rows{},numeric_variables{};
    for(auto const& unit:dwarf->compile_units())
    {
        auto root{unit->getUnitDIE(false)};
        if(llvm::dwarf::toStringRef(root.find(llvm::dwarf::DW_AT_producer)) != "uwvm debug-jit native Wasm provenance v1") { continue; }
        auto line{dwarf->getLineTableForUnit(unit.get(),error)};
        if(!line) { error(line.takeError());return 4; }
        CHECK(*line != nullptr);
        for(auto const& row:(*line)->Rows)
        { if(!row.EndSequence && row.Line != 0u && row.Column == 1u) { ++public_rows; } }
        std::vector<llvm::DWARFDie> pending{root};
        while(!pending.empty())
        {
            auto die{pending.back()};pending.pop_back();
            if(die.getTag() == llvm::dwarf::DW_TAG_variable &&
                llvm::dwarf::toStringRef(die.find(llvm::dwarf::DW_AT_name)).starts_with("uwvm.native.numeric."))
            { ++numeric_variables; }
            for(auto const child:die.children()) { pending.push_back(child); }
        }
    }
    CHECK(!malformed && public_rows != 0u && numeric_variables != 0u);
    ledger::image observed{};CHECK(ledger::collect(**object,observed));
    if(!observed.endpoints_unambiguous())
    {
        fast_io::io::perrln("actual owner blockers=",observed.blockers);
        for(auto const& body:observed.bodies)
        { fast_io::io::perrln("actual object owner section=",body.section," begin=",body.begin_offset," end=",body.end_offset," shape=",body.shape," proved=",body.endpoints_proved); }
        for(auto const& claim:observed.claims)
        {
            if(claim.text && claim.kind!=llvm::object::SymbolRef::ST_Function)
            { fast_io::io::perrln("actual object text metadata name=",fast_io::mnp::os_c_str(claim.name.c_str())," kind=",claim.kind," flags=",claim.flags," section=",claim.section," offset=",claim.offset); }
        }
    }
    CHECK(observed.endpoints_unambiguous());
    std::vector<uwvm2::runtime::lib::details::native_arm_mapping::data_range> literal_data{};
    CHECK(uwvm2::runtime::lib::details::native_arm_mapping::collect(**object,literal_data));
    if(!literal_data.empty())
    {
        namespace provenance=uwvm2::runtime::lib::details::native_loaded_provenance;
        offline_sections addresses{};provenance::image source{};
        CHECK(source.observe(1u,**object,addresses));source.bind_actual_runtime_epoch(7u);
        unsigned visible_bodies{},masked_bytes{};
        for(auto const& body:observed.bodies)
        {
            if(body.role!=1u) { continue; }
            auto const base{0x10000000u+body.section*0x1000000u};
            CHECK(base<=UINTPTR_MAX && body.end_offset<=UINTPTR_MAX-base && body.end_offset-body.begin_offset<=65536u);
            auto const begin{static_cast<std::uintptr_t>(base+body.begin_offset)},end{static_cast<std::uintptr_t>(base+body.end_offset)};
            std::array<std::uint8_t,65536u> permissions{},instruction_code{};bool visible{};
            for(auto const& unit:dwarf->compile_units())
            {
                auto table{dwarf->getLineTableForUnit(unit.get(),error)};
                if(!table) { error(table.takeError());return 4; }
                if(*table==nullptr) { continue; }
                for(auto const& row:(*table)->Rows)
                {
                    if(row.EndSequence || row.Column!=1u || row.Line==0u) { continue; }
                    std::string identity{};CHECK((*table)->getFileNameByIndex(row.File,"uwvm-native-provenance-v1",
                        llvm::DILineInfoSpecifier::FileLineInfoKind::RawValue,identity));
                    CHECK(source.code_permissions(begin,end-begin,begin,end,identity,1u<<20u,7u,permissions.data()));
                    CHECK(source.instruction_code(begin,end-begin,begin,end,identity,1u<<20u,7u,instruction_code.data()));
                    CHECK(!source.instruction_code(begin,end-begin,begin,end,identity,1u<<20u,8u,instruction_code.data()));
                    CHECK(!source.instruction_code(begin+4u,end-begin-4u,begin,end,identity,1u<<20u,7u,instruction_code.data()));
                    CHECK(!source.instruction_code(begin,end-begin,begin,end,identity,1u<<20u,7u,nullptr));
                    for(auto offset{body.begin_offset};offset<body.end_offset;++offset)
                    {
                        bool literal{};
                        for(auto const& data:literal_data)
                        { literal |= data.section==body.section && offset>=data.begin && offset<data.end; }
                        CHECK(instruction_code[offset-body.begin_offset]==(literal ? 0u:1u));
                    }
                    for(auto const& data:literal_data)
                    {
                        if(data.section!=body.section) { continue; }
                        auto const low{std::max(data.begin,body.begin_offset)},high{std::min(data.end,body.end_offset)};
                        for(auto offset{low};offset<high;++offset)
                        {
                            CHECK(permissions[offset-body.begin_offset]==0u && instruction_code[offset-body.begin_offset]==0u);
                            CHECK(source.lookup(static_cast<std::uintptr_t>(base+offset),begin,end,identity,1u<<20u,7u).state==provenance::status::unavailable);
                            ++masked_bytes;
                        }
                    }
                    for(auto i{begin};i<end;++i) { visible |= permissions[i-begin]==1u; }
                }
            }
            visible_bodies+=visible;
        }
        CHECK(visible_bodies!=0u && masked_bytes!=0u && !malformed);
        unsigned mutations{};
        for(auto const& symbol:(**object).symbols())
        {
            auto name{symbol.getName()};if(!name) { llvm::consumeError(name.takeError());return 3; }
            if(*name!="$d") { continue; }
            auto const offset{static_cast<std::size_t>(name->data()-(**object).getData().data())};
            CHECK(offset<(**object).getData().size() && (**object).getData().size()-offset>=2u);
            for(auto replacement:{'t','e'})
            {
                std::string bytes{(**object).getData().data(),(**object).getData().size()};bytes[offset+1u]=replacement;
                auto changed{llvm::object::ObjectFile::createObjectFile(llvm::MemoryBufferRef{llvm::StringRef{bytes},"actual-ARM-mapping-refusal"})};
                CHECK(changed);ledger::image rejected{};
                CHECK(!ledger::collect(**changed,rejected) || !rejected.endpoints_unambiguous());
                ++mutations;
            }
            break;
        }
        CHECK(mutations==2u);
        source.invalidate_runtime_generation();CHECK(!source.valid());
        fast_io::io::println("PASS actual ARM literal pool excluded from every public byte and numeric PC; ordinary numeric code retained; Thumb/unknown mapping refused mutations=",mutations);
    }
    if((**object).getArch()==llvm::Triple::mips64 || (**object).getArch()==llvm::Triple::mips64el)
    {
        namespace provenance=uwvm2::runtime::lib::details::native_loaded_provenance;
        offline_sections addresses{};provenance::image source{};
        CHECK(source.observe(1u,**object,addresses));source.bind_actual_runtime_epoch(7u);
        struct location { std::uint_least32_t dwarf_register{},bits{}; };
        unsigned code_bytes{},gp32{},gp64{},fp32{},fp64{};
        for(auto const& body:observed.bodies)
        {
            if(body.role!=1u) { continue; }
            auto const base{0x10000000u+body.section*0x1000000u};
            CHECK(base<=UINTPTR_MAX && body.end_offset<=UINTPTR_MAX-base && body.end_offset-body.begin_offset<=65536u);
            auto const begin{static_cast<std::uintptr_t>(base+body.begin_offset)},end{static_cast<std::uintptr_t>(base+body.end_offset)};
            std::array<std::uint8_t,65536u> permissions{};
            for(auto const& unit:dwarf->compile_units())
            {
                auto root{unit->getUnitDIE(false)};
                auto const name{llvm::dwarf::toStringRef(root.find(llvm::dwarf::DW_AT_name))};
                if(!name.starts_with("uwvm-m")) { continue; }
                std::string identity{name.data(),name.size()};
                CHECK(source.code_permissions(begin,end-begin,begin,end,identity,1u<<20u,7u,permissions.data()));
                for(auto pc{begin};pc<end;++pc)
                {
                    code_bytes+=permissions[pc-begin]!=0u;
                    location values[64u]{};
                    auto const count{source.numeric_locations(pc,begin,end,identity,1u<<20u,7u,values,64u)};
                    for(std::size_t i{};i<count;++i)
                    {
                        auto const value{values[i]};
                        if(value.dwarf_register<32u) { gp32+=value.bits==32u;gp64+=value.bits==64u; }
                        else if(value.dwarf_register<64u) { fp32+=value.bits==32u;fp64+=value.bits==64u; }
                    }
                }
            }
        }
        fast_io::io::println("actual MIPS relocated typed metadata code-bytes=",code_bytes,
            " gp32=",gp32," gp64=",gp64," fp32=",fp32," fp64=",fp64);
        CHECK(code_bytes!=0u && gp32!=0u);
        source.invalidate_runtime_generation();CHECK(!source.valid());
    }
    unsigned mappings{},temporaries{};
    for(auto const& symbol:(**object).symbols())
    {
        auto name{symbol.getName()};if(!name) { llvm::consumeError(name.takeError());return 3; }
        auto selected{symbol.getSection()};if(!selected) { llvm::consumeError(selected.takeError());return 3; }
        if(*selected==(**object).section_end()) { continue; }
        auto address{symbol.getAddress()};if(!address) { llvm::consumeError(address.takeError());return 3; }
        auto kind{symbol.getType()};if(!kind) { llvm::consumeError(kind.takeError());return 3; }
        auto flags{symbol.getFlags()};if(!flags) { llvm::consumeError(flags.takeError());return 3; }
        auto const base{(*selected)->getAddress()};CHECK(*address>=base);
        for(auto const& claim:observed.claims)
        {
            // LLVM reuses .L0 names in both text and DWARF sections. Match
            // this actual symbol's full tuple, never every equal-name claim.
            if(claim.name!=*name || claim.section!=(*selected)->getIndex() ||
               claim.offset!=*address-base || claim.kind!=*kind || claim.flags!=*flags) { continue; }
            if(ledger::detail::retained_temporary(**object,symbol,claim,observed.bodies))
            {
                ++temporaries;auto bad{claim};bad.offset=bad.section_size+1u;
                CHECK(!ledger::detail::retained_temporary(**object,symbol,bad,observed.bodies));
                bad=claim;bad.flags=llvm::object::SymbolRef::SF_Global;
                CHECK(!ledger::detail::retained_temporary(**object,symbol,bad,observed.bodies));
                bad=claim;bad.kind=llvm::object::SymbolRef::ST_Function;
                CHECK(!ledger::detail::retained_temporary(**object,symbol,bad,observed.bodies));
                for(auto invalid:{".Levil",".Ltmp",".Ltmp1evil",".LBB1_",".Luwvm_code_end",".Lfunc_begin0x","$d"})
                {
                    bad=claim;bad.name=invalid;
                    CHECK(!ledger::detail::retained_temporary(**object,symbol,bad,observed.bodies));
                }
            }
            if(!ledger::detail::instruction_mapping(**object,symbol,claim,observed.bodies)) { break; }
            ++mappings;
            auto bad{claim};bad.offset=bad.section_size;
            CHECK(!ledger::detail::instruction_mapping(**object,symbol,bad,observed.bodies));
            bad=claim;bad.section=(std::numeric_limits<std::uint64_t>::max)();
            CHECK(!ledger::detail::instruction_mapping(**object,symbol,bad,observed.bodies));
            bad=claim;bad.kind=llvm::object::SymbolRef::ST_Function;
            CHECK(!ledger::detail::instruction_mapping(**object,symbol,bad,observed.bodies));
            for(auto invalid:{"$d","$t","$aevil","$a.","$a.a","$xevil","$x.","$x.a",""})
            {
                bad=claim;bad.name=invalid;
                CHECK(!ledger::detail::instruction_mapping(**object,symbol,bad,observed.bodies));
            }
            break; // Count/test this actual symbol once, even equal tuples.
        }
    }
    auto const arch{(**object).getArch()};
    if(arch==llvm::Triple::aarch64 || arch==llvm::Triple::aarch64_be || arch==llvm::Triple::riscv32 || arch==llvm::Triple::riscv64)
    { CHECK(mappings!=0u); }
    fast_io::io::println("PASS actual ELF instruction mapping metadata retained, no new code extent, data/alias/gap claims refused mappings=",mappings," temporaries=",temporaries," public-origin-rows=",public_rows," typed-numeric-variables=",numeric_variables);
}
