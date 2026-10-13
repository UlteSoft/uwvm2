// Actual compiler object mutations; no generated function is executed.
#pragma once
#include <llvm/Support/MemoryBufferRef.h>
#include <functional>

#if defined(__linux__) && defined(__powerpc64__)
struct stackmap_loaded_sections final : ::llvm::LoadedObjectInfo
{
    ::llvm::object::ObjectFile const& original;
    ::llvm::LoadedObjectInfo const& loaded;
    stackmap_loaded_sections(::llvm::object::ObjectFile const& object,
        ::llvm::LoadedObjectInfo const& addresses) : original(object), loaded(addresses) {}
    ::std::unique_ptr<::llvm::LoadedObjectInfo> clone() const override
    { return ::std::make_unique<stackmap_loaded_sections>(original,loaded); }
    ::std::uint64_t getSectionLoadAddress(::llvm::object::SectionRef const& section) const override
    {
        for(auto const& actual:original.sections())
        { if(actual.getIndex()==section.getIndex()) { return loaded.getSectionLoadAddress(actual); } }
        return 0u;
    }
};

static bool check_actual_stackmap_boundary(::llvm::object::ObjectFile const& object,
    ::llvm::LoadedObjectInfo const& loaded, unsigned kind)
{
    if(kind!=0x7bu && kind!=0x7eu) { return true; }
    ::std::size_t table_offset{},table_size{},name_offset{};
    for(auto const& section:object.sections())
    {
        auto name{section.getName()};
        if(!name) { ::llvm::consumeError(name.takeError());return false; }
        if(*name!=".llvm_stackmaps") { continue; }
        auto content{section.getContents()};
        if(!content) { ::llvm::consumeError(content.takeError());return false; }
        table_offset=content->data()-object.getData().data();table_size=content->size();
        name_offset=name->data()-object.getData().data();
    }
    if(table_size<16u) { return false; }
    auto const read{[&](::std::size_t offset,unsigned width) -> ::std::uint64_t
    {
        if(offset>table_size || width>table_size-offset) { return UINT64_MAX; }
        ::std::uint64_t value{};
        for(unsigned i{};i!=width;++i)
        { value|=static_cast<::std::uint64_t>(static_cast<unsigned char>(object.getData()[table_offset+offset+i])) <<
            (8u*(object.isLittleEndian() ? i : width-i-1u)); }
        return value;
    }};
    auto const functions{read(4u,4u)},constants{read(8u,4u)},records{read(12u,4u)};
    if(functions!=1u || records==0u || constants>1024u) { return false; }
    ::std::vector<::std::size_t> record_offsets,location_offsets;
    ::std::size_t cursor{static_cast<::std::size_t>(16u+24u*functions+8u*constants)};
    for(::std::uint64_t i{};i!=records;++i)
    {
        if(cursor>table_size || table_size-cursor<16u) { return false; }
        record_offsets.push_back(cursor);
        auto const count{read(cursor+14u,2u)};cursor+=16u;
        if(count!=1u || table_size-cursor<12u) { return false; }
        location_offsets.push_back(cursor);cursor+=12u;cursor=(cursor+7u)&~::std::size_t{7u};
        if(cursor>table_size || table_size-cursor<4u) { return false; }
        auto const liveouts{read(cursor+2u,2u)};cursor+=4u;
        if(liveouts>(table_size-cursor)/4u) { return false; }
        cursor+=liveouts*4u;cursor=(cursor+7u)&~::std::size_t{7u};
    }
    if(cursor!=table_size) { return false; }
    using mutation=::std::function<void(::std::string&)>;
    auto const change{[&](::std::string& bytes,::std::size_t offset,unsigned width,::std::uint64_t value)
    {
        for(unsigned i{};i!=width;++i)
        { bytes[table_offset+offset+i]=static_cast<char>(value >> (8u*(object.isLittleEndian() ? i : width-i-1u))); }
    }};
    stackmap_loaded_sections addresses{object,loaded};
    auto const observe{[&](mutation const& edit,provenance::image& rows) -> bool
    {
        ::std::string bytes{object.getData().data(),object.getData().size()};edit(bytes);
        auto parsed{::llvm::object::ObjectFile::createObjectFile(::llvm::MemoryBufferRef{
            ::llvm::StringRef{bytes.data(),bytes.size()},"owned-stackmap-mutation"})};
        if(!parsed) { ::llvm::consumeError(parsed.takeError());return false; }
        return rows.observe(1u,**parsed,addresses);
    }};
    provenance::image baseline{},dwarf_only{};
    if(!observe([](::std::string&){},baseline) || !observe([&](::std::string& bytes){bytes[name_offset]='_';},dwarf_only)) { return false; }
    baseline.bind_actual_runtime_epoch(7u);dwarf_only.bind_actual_runtime_epoch(7u);
    auto const identity{::fast_io::concat_fast_io("uwvm-m2-f3-g4.wasm-native-v1")};
    ::std::string_view const file{identity.data(),identity.size()};
    ::std::uintptr_t begin{},end{};
    for(auto const& symbol:object.symbols())
    {
        auto name{symbol.getName()};if(!name) { ::llvm::consumeError(name.takeError());return false; }
        if(*name!="native_provenance_probe") { continue; }
        auto const size{::llvm::object::ELFSymbolRef{symbol}.getSize()};
        auto section{symbol.getSection()};if(!section) { ::llvm::consumeError(section.takeError());return false; }
        if(*section==object.section_end()) { return false; }
        if((**section).isText())
        {
            auto address{symbol.getAddress()};if(!address) { ::llvm::consumeError(address.takeError());return false; }
            begin=loaded.getSectionLoadAddress(**section)+*address-(**section).getAddress();end=begin+size;
        }
        else
        {
            ::uwvm2::runtime::lib::details::ppc64_elfv1_relocation_index descriptors{object};
            auto range{::uwvm2::runtime::lib::details::get_ppc64_elfv1_loaded_function_range(object,symbol,loaded,size,descriptors)};
            begin=range.begin;end=begin+range.size;
        }
    }
    if(begin==0u || end<=begin || end-begin>65536u) { return false; }
    struct location { unsigned dwarf_register{},bits{}; };
    bool positive{};
    for(auto pc{begin};pc<end;++pc)
    {
        location actual[64u]{},ordinary[64u]{};
        positive|=baseline.numeric_locations(pc,begin,end,file,3u,7u,actual,64u)>
            dwarf_only.numeric_locations(pc,begin,end,file,3u,7u,ordinary,64u);
    }
    if(!positive) { return false; }
    // Malformed encodings invalidate the complete image, including earlier
    // DWARF rows. No partially parsed authority can remain reachable.
    for(unsigned attack{};attack!=5u;++attack)
    {
        provenance::image rejected{};
        auto corrupt{[&](::std::string& bytes)
        {
            if(attack==0u) { change(bytes,0u,1u,2u); }
            if(attack==1u) { change(bytes,4u,4u,UINT32_MAX); }
            if(attack==2u) { change(bytes,32u,8u,records+1u); }
            if(attack==3u) { change(bytes,record_offsets[0]+12u,2u,1u); }
            if(attack==4u) { change(bytes,record_offsets[0]+14u,2u,129u); }
        }};
        if(observe(corrupt,rejected) || rejected.valid() || rejected.row_count()!=0u) { return false; }
        rejected.bind_actual_runtime_epoch(7u);location output[64u]{};
        if(rejected.numeric_locations(begin,begin,end,file,3u,7u,output,64u)!=0u) { return false; }
    }
    // Structurally valid non-register/address/spill/constant records and bad
    // type/identity/PC fields must add no locations beyond ordinary DWARF.
    for(unsigned attack{};attack!=10u;++attack)
    {
        provenance::image refused{};
        auto corrupt{[&](::std::string& bytes)
        {
            for(auto offset:location_offsets)
            {
                if(attack<4u) { change(bytes,offset,1u,attack+2u); }
                if(attack==4u) { change(bytes,offset+2u,2u,1u); }
                if(attack==5u) { change(bytes,offset+4u,2u,UINT16_MAX); }
                if(attack==6u) { change(bytes,offset+8u,4u,8u); }
            }
            for(auto offset:record_offsets)
            {
                if(attack==7u) { change(bytes,offset,8u,0u); }
                if(attack==8u) { change(bytes,offset+8u,4u,UINT32_MAX); }
                if(attack==9u) { auto const id{read(offset,8u)};change(bytes,offset,8u,id | (0xffull<<32u)); }
            }
        }};
        if(!observe(corrupt,refused)) { return false; }refused.bind_actual_runtime_epoch(7u);
        for(auto pc{begin};pc<end;++pc)
        {
            location output[64u]{},ordinary[64u]{};
            auto const count{refused.numeric_locations(pc,begin,end,file,3u,7u,output,64u)};
            auto const expected{dwarf_only.numeric_locations(pc,begin,end,file,3u,7u,ordinary,64u)};
            if(count!=expected) { return false; }
            for(::std::size_t i{};i!=count;++i)
            { if(output[i].dwarf_register!=ordinary[i].dwarf_register || output[i].bits!=ordinary[i].bits) { return false; } }
        }
    }
    baseline.invalidate_runtime_generation();location output[64u]{};
    if(baseline.numeric_locations(begin,begin,end,file,3u,7u,output,64u)!=0u) { return false; }
    ::fast_io::io::println("PASS actual stackmap exact-PC registers and 15 malformed/non-register mutations kind=",kind);
    return true;
}
#else
static bool check_actual_stackmap_boundary(::llvm::object::ObjectFile const&,
    ::llvm::LoadedObjectInfo const&,unsigned) { return true; }
#endif
