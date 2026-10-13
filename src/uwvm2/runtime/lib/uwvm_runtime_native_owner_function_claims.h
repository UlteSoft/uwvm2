// Actual object/symbol claim DATA. Exact backend endpoints are necessary, but
// neither this helper nor a table name issues executable read/step permission.
#pragma once
#include "uwvm_runtime_native_owner_table_object_graph.h"
#include "uwvm_runtime_native_arm_mapping.h"
#ifndef UWVM_MODULE
#include <string_view>
#endif
#if defined(UWVM_RUNTIME_LLVM_JIT)
namespace uwvm2::runtime::lib::details::native_owner_function_claims
{
    namespace graph=::uwvm2::runtime::lib::details::native_owner_object_graph;
    enum class blocker : unsigned
    {
        none=0u, unknown_body=1u, overlapping_body=2u, name_collision=4u,
        missing_entry=8u, unexpected_alias=16u, unknown_text_claim=32u,
        invalid_entry=64u, explicit_zero_size=128u, unknown_defined_claim=256u
    };
    struct claim
    {
        ::std::string name{};
        ::std::uint64_t section{}, offset{}, section_size{}, explicit_size{};
        unsigned kind{}, flags{};
        bool defined{}, text{}, located{}, explicit_size_known{};
        // Only actual symbol-table observations. Zero/unknown extents are kept;
        // no computeSymbolSizes/next-symbol/section-end estimate is used.
    };
    struct image
    {
        ::std::vector<graph::row> bodies{};
        ::std::vector<claim> claims{};
        unsigned blockers{};
        bool complete{};
        [[nodiscard]] bool endpoints_unambiguous() const noexcept
        { return complete && blockers==0u; }
    };
    namespace detail
    {
        inline constexpr ::std::size_t max_symbols{1'048'576u};
        [[nodiscard]] inline bool instruction_mapping(::llvm::object::ObjectFile const& object,
            ::llvm::object::SymbolRef const& symbol,claim const& value,::std::span<graph::row const> bodies) noexcept
        {
            // Actual ELF local NOTYPE, zero-sized ISA mapping annotations are
            // not callable aliases. Retain them in the complete ledger; they
            // grant no extent. A matching name alone never qualifies a claim.
            if(!::llvm::isa<::llvm::object::ELFObjectFileBase>(object) || !value.defined || !value.located || !value.text ||
               value.kind!=::llvm::object::SymbolRef::ST_Unknown ||
               (value.flags & ::llvm::object::SymbolRef::SF_FormatSpecific)==0u) { return false; }
            auto const actual{::llvm::object::ELFSymbolRef{symbol}};
            if(actual.getBinding()!=::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::binding_local || actual.getELFType()!=::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::symbol_notype ||
               actual.getSize()!=0u || actual.getOther()!=0u) { return false; }
            ::std::string_view const name{value.name};
            auto const arch{object.getArch()}; unsigned alignment{};
            if(arch==::llvm::Triple::aarch64 || arch==::llvm::Triple::aarch64_be) { alignment=4u; }
            else if(arch==::llvm::Triple::riscv32 || arch==::llvm::Triple::riscv64) { alignment=2u; }
            else if(arch==::llvm::Triple::arm || arch==::llvm::Triple::armeb) { alignment=4u; }
            else { return false; }
            // ARM-mode annotations are ISA metadata under the same exact
            // local/NOTYPE/zero-size/owned-body checks. Thumb/data annotations
            // remain blockers for this fixed ARM-mode native adapter.
            bool const arm{arch==::llvm::Triple::arm || arch==::llvm::Triple::armeb};
            bool accepted{name==(arm ? "$a" : "$x")};
            if(name.starts_with(arm ? "$a." : "$x.") && name.size()>3u)
            {
                accepted=true;
                for(::std::size_t i{3u};i<name.size();++i) { accepted &= name[i]>='0' && name[i]<='9'; }
            }
            if((arch==::llvm::Triple::riscv64 && name.starts_with("$xrv64")) ||
               (arch==::llvm::Triple::riscv32 && name.starts_with("$xrv32")))
            {
                accepted=name.size()>6u;
                for(::std::size_t i{6u};i<name.size();++i)
                { auto const c{name[i]}; accepted &= (c>='a' && c<='z') || (c>='0' && c<='9') || c=='_' || c=='.'; }
            }
            if(!accepted || value.offset%alignment!=0u) { return false; }
            unsigned owners{};
            for(auto const& body:bodies)
            {
                if(body.endpoints_proved && body.shape==1u && body.section==value.section && body.section_size==value.section_size &&
                   value.offset>=body.begin_offset && value.offset<body.end_offset) { ++owners; }
            }
            return owners==1u; // No unknown gap, overlap, literal data or new body.
        }
        [[nodiscard]] inline bool macho_section_anchor(::llvm::object::ObjectFile const& object,
            ::llvm::object::SymbolRef const& symbol,claim const& value,::std::span<graph::row const> bodies) noexcept
        {
            // AArch64's real Mach-O streamer labels each section with a linker-
            // private ltmpN. LLVM classifies every text N_SECT as ST_Function,
            // including this section anchor. Retain the exact local anchor as
            // DATA only: it never supplies an entry, body, size or code grant.
            auto const* macho{::llvm::dyn_cast<::llvm::object::MachOObjectFile>(::std::addressof(object))};
            if(macho==nullptr || !macho->is64Bit() || object.getArch()!=::llvm::Triple::aarch64 ||
               symbol.getObject()!=::std::addressof(object) || !value.defined || !value.located || !value.text ||
               value.kind!=::llvm::object::SymbolRef::ST_Function || value.flags!=0u || value.offset!=0u ||
               value.section>=255u || bodies.size()!=1u || bodies[0u].begin_offset!=0u ||
               !bodies[0u].endpoints_proved || bodies[0u].shape!=1u || bodies[0u].section!=value.section ||
               bodies[0u].section_size!=value.section_size) { return false; }
            ::std::string_view const name{value.name};
            if(!name.starts_with("ltmp") || name.size()==4u || (name.size()>5u && name[4u]=='0')) { return false; }
            for(::std::size_t i{4u};i<name.size();++i) { if(name[i]<'0' || name[i]>'9') { return false; } }
            // Actual enumerated symbol, not a relocation-created unchecked
            // index. Exact N_SECT/local/no-descriptor and one-based section
            // ordinal are necessary; a familiar spelling alone is insufficient.
            auto const actual{macho->getSymbol64TableEntry(symbol.getRawDataRefImpl())};
            return actual.n_type==::llvm::MachO::N_SECT && actual.n_desc==0u &&
                actual.n_sect==value.section+1u;
        }
        [[nodiscard]] inline bool retained_temporary(::llvm::object::ObjectFile const& object,
            ::llvm::object::SymbolRef const& symbol,claim const& value,::std::span<graph::row const> bodies) noexcept
        {
            auto const arch{object.getArch()};
            // These targets need retained temporary symbols for exact metadata
            // relocation resolution. No label creates a body or callable entry.
            if((arch!=::llvm::Triple::riscv32 && arch!=::llvm::Triple::riscv64 && arch!=::llvm::Triple::loongarch64) ||
               !::llvm::isa<::llvm::object::ELFObjectFileBase>(object) || !value.defined || !value.located || !value.text ||
               value.kind!=::llvm::object::SymbolRef::ST_Unknown ||
               (value.flags!=0u && !(value.flags==::llvm::object::SymbolRef::SF_FormatSpecific && value.name==".L0 "))) { return false; }
            auto const actual{::llvm::object::ELFSymbolRef{symbol}};
            if(actual.getBinding()!=::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::binding_local || actual.getELFType()!=::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::symbol_notype ||
               actual.getSize()!=0u || actual.getOther()!=0u) { return false; }
            auto digits{[](::std::string_view text) noexcept
            {
                if(text.empty()) { return false; }
                for(auto c:text) { if(c<'0' || c>'9') { return false; } }
                return true;
            }};
            ::std::string_view const name{value.name};bool end_label{};bool recognized{};
            for(auto prefix:{::std::string_view{".Ltmp"},::std::string_view{".Lfunc_begin"},::std::string_view{".Luwvm_code_end"}})
            {
                if(name.starts_with(prefix) && digits(name.substr(prefix.size())))
                { recognized=true;end_label=prefix==".Luwvm_code_end";break; }
            }
            if(name.starts_with(".LBB"))
            {
                auto const separator{name.find('_',4u)};
                recognized=separator!=::std::string_view::npos && digits(name.substr(4u,separator-4u)) && digits(name.substr(separator+1u));
            }
            // LLVM's ELF writer may name a discarded local relocation anchor
            // '.L0 '. Its actual local/NOTYPE/zero-size tuple and body point,
            // never this spelling or its non-unique name, prove admission.
            bool const relocation_anchor{name==".L0 "};
            if(relocation_anchor) { recognized=true; }
            if(!recognized) { return false; }
            unsigned owners{};
            for(auto const& body:bodies)
            {
                if(body.endpoints_proved && body.shape==1u && body.section==value.section && body.section_size==value.section_size &&
                   (end_label ? value.offset==body.end_offset :
                    (value.offset>=body.begin_offset && value.offset<body.end_offset) ||
                    (relocation_anchor && value.offset==body.end_offset))) { ++owners; }
            }
            return owners==1u;
        }
        // The complete geometric ledger is sorted and has already checked
        // overlaps. Probe only the adjacent bodies, avoiding quadratic work
        // when a target retains a temporary for every basic block.
        [[nodiscard]] inline ::std::span<graph::row const> adjacent_body(image const& candidate,
            ::std::vector<::std::size_t> const& geometric,claim const& value,bool endpoint) noexcept
        {
            if((candidate.blockers & static_cast<unsigned>(blocker::overlapping_body))!=0u) { return {}; }
            auto found{::std::upper_bound(geometric.begin(),geometric.end(),value,[&](auto const& point,auto index)
            {
                auto const& body{candidate.bodies[index]};
                return point.section!=body.section ? point.section<body.section : point.offset<body.begin_offset;
            })};
            for(unsigned checked{};found!=geometric.begin() && checked!=2u;++checked)
            {
                --found;auto const& body{candidate.bodies[*found]};
                if(body.section==value.section && body.section_size==value.section_size &&
                   (endpoint ? value.offset==body.end_offset : value.offset>=body.begin_offset && value.offset<body.end_offset))
                { return {::std::addressof(body),1u}; }
            }
            return {};
        }
        inline void deny(image& candidate,blocker reason) noexcept
        { candidate.blockers|=static_cast<unsigned>(reason); }
        struct descriptor_relocation
        {
            ::std::uint64_t offset{};
            ::llvm::object::SectionRef section{};
            ::llvm::object::RelocationRef relocation{};
        };
        // Temporary actual object borrows; this index dies inside collect(),
        // never enters an owned image or survives ObjectFile/section storage.
        struct descriptor_index
        {
            ::llvm::object::SectionRef section{};
            ::std::vector<descriptor_relocation> relocations{};
            bool present{};
            [[nodiscard]] bool build(::llvm::object::ObjectFile const& object)
            {
                auto const* elf{::llvm::dyn_cast<::llvm::object::ELFObjectFileBase>(::std::addressof(object))};
                if(elf==nullptr || object.getBytesInAddress()!=8u ||
                   (object.getArch()!=::llvm::Triple::ppc64 && object.getArch()!=::llvm::Triple::ppc64le) ||
                   (elf->getPlatformFlags() & ::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::ppc64_abi)==2u) { return true; }
                for(auto const& current:object.sections())
                {
                    auto name{current.getName()};if(!name) { ::llvm::consumeError(name.takeError());return false; }
                    if(*name!=".opd") { continue; }
                    if(present || current.isText() || ::llvm::object::ELFSectionRef{current}.getType()!=::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::section_progbits)
                    { return false; }
                    section=current;present=true;
                }
                if(!present) { return true; }
                auto const size{section.getSize()};
                if(size>graph::detail::max_object_bytes) { return false; }
                auto observe=[&](::llvm::object::SectionRef const& relsection)
                {
                    for(auto const& relocation:relsection.relocations())
                    {
                        if(relocations.size()==2u*graph::detail::max_rows) { return false; }
                        auto const offset{relocation.getOffset()};
                        if(offset>size || 8u>size-offset) { return false; }
                        relocations.push_back({offset,relsection,relocation});
                    }
                    return true;
                };
                if(!observe(section)) { return false; }
                for(auto const& relsection:object.sections())
                {
                    if(relsection.getIndex()==section.getIndex()) { continue; }
                    auto target{relsection.getRelocatedSection()};
                    if(!target) { ::llvm::consumeError(target.takeError());return false; }
                    if(*target!=object.section_end() && (*target)->getIndex()==section.getIndex() && !observe(relsection)) { return false; }
                }
                ::std::sort(relocations.begin(),relocations.end(),[](auto const& x,auto const& y){ return x.offset<y.offset; });
                return true;
            }
        };
        // Real PPC ELFv1 descriptor field0 relocation graph. Original object
        // DATA only; per descriptor consumes its indexed three fields, never
        // re-scans O(all .opd relocations) or reads relocated native memory.
        [[nodiscard]] inline bool descriptor_matches(::llvm::object::ObjectFile const& object,
            ::llvm::object::SymbolRef const& symbol,graph::row const& body,descriptor_index const& index)
        {
            if(!index.present) { return false; }
            auto selected{symbol.getSection()};
            if(!selected) { ::llvm::consumeError(selected.takeError());return false; }
            if(*selected==object.section_end() || (*selected)->getIndex()!=index.section.getIndex()) { return false; }
            auto const& section{index.section};
            auto address{symbol.getAddress()};if(!address) { ::llvm::consumeError(address.takeError());return false; }
            auto contents{section.getContents()};if(!contents) { ::llvm::consumeError(contents.takeError());return false; }
            auto const base{section.getAddress()},size{section.getSize()};
            if(*address<base || *address-base>size || size>graph::detail::max_object_bytes ||
               contents->size()!=size || contents->size()>PTRDIFF_MAX) { return false; }
            auto const offset{*address-base};
            if((offset&7u)!=0u || 24u>size-offset) { return false; }
            // [actual original .opd ... offset][complete 24-byte descriptor]
            // [safe] exact file/section extent and complete width before pointer.
            auto const* first{reinterpret_cast<unsigned char const*>(contents->data())+offset};
            ::std::span<unsigned char const> bytes{first,24u};
            ::std::size_t cursor{};::std::uint64_t raw{},toc{},environment{};
            auto const order{object.isLittleEndian() ? 1u:2u};
            if(!native_owner_table_format::get<64u>(bytes,cursor,order,raw) ||
               !native_owner_table_format::get<64u>(bytes,cursor,order,toc) ||
               !native_owner_table_format::get<64u>(bytes,cursor,order,environment) || cursor!=24u || environment!=0u)
            { return false; }
            unsigned entry_count{},toc_count{},fields{};graph::detail::point point{};
            auto found{::std::lower_bound(index.relocations.begin(),index.relocations.end(),offset,
                [](auto const& value,auto key){ return value.offset<key; })};
            while(found!=index.relocations.end() && found->offset>=offset && found->offset-offset<24u)
            {
                // [one temporary sorted actual relocation array][found != end]
                // [safe] complete descriptor width proved above; at most its
                // three pointer fields may be consumed. Extra duplicates decline.
                if(++fields>3u) { return false; }
                auto const& relocation{found->relocation};
                if(found->offset==offset)
                {
                    if(++entry_count!=1u || relocation.getType()!=::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::ppc64_addr64 ||
                       !graph::detail::endpoint(object,found->section,relocation,8u,raw,point)) { return false; }
                }
                else if(found->offset-offset==8u)
                {
                    if(++toc_count!=1u || relocation.getType()!=::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::ppc64_toc) { return false; }
                }
                else { return false; }
                // [actual currently consumed row][same array next or end]
                // [safe] found!=end precedes the one-element iterator advance.
                ++found;
            }
            // Callable descriptor remains DATA; exact same-object true code
            // point only. No descriptor word0 normalization or live dereference.
            return entry_count==1u && toc_count==1u && point.known && point.section==body.section &&
                point.offset==body.begin_offset && point.size==body.section_size;
        }
        struct named_body { ::std::string_view name{};::std::size_t body{};bool local{}; };
    }
    // Scan ALL actual symbols, including undefined/data/debug symbols in the
    // retained DATA ledger. Extra/unknown/zero function claims do not disappear.
    [[nodiscard]] inline bool collect(::llvm::object::ObjectFile const& object,image& output)
    {
        output={};image candidate{};
        if(!object.isRelocatableObject() || object.getData().size()>graph::detail::max_object_bytes) { return false; }
        if(::llvm::isa<::llvm::object::ELFObjectFileBase>(object))
        {
            unsigned ordinary_tables{},dynamic_tables{};::std::size_t section_count{};
            for(auto const& section:object.sections())
            {
                if(++section_count>graph::detail::max_sections) { return false; }
                auto const kind{::llvm::object::ELFSectionRef{section}.getType()};
                if(kind==::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::section_symtab) { ++ordinary_tables; }
                else if(kind==::uwvm2::runtime::compiler::llvm_jit::details::elf_header_constants::section_dynsym) { ++dynamic_tables; }
            }
            // ELFObjectFile::symbols() enumerates ONLY the first DotSymtabSec.
            // No complete all-symbol claim may be published if another actual
            // ordinary/dynamic symbol table would be silently omitted.
            if(ordinary_tables!=1u || dynamic_tables!=0u) { return false; }
        }
        if(!graph::collect(object,candidate.bodies)) { return false; }
        ::std::vector<native_arm_mapping::data_range> arm_data{};
        if(!native_arm_mapping::collect(object,arm_data)) { return false; }
        detail::descriptor_index descriptors{};
        if(!descriptors.build(object)) { return false; }
        ::std::vector<detail::named_body> names{};::std::vector<::std::size_t> geometric{};
        ::std::vector<::std::string_view> original_names{};original_names.reserve(candidate.bodies.size());
        ::std::vector<unsigned> entry_counts(candidate.bodies.size()),local_counts(candidate.bodies.size());
        names.reserve(candidate.bodies.size()*2u);geometric.reserve(candidate.bodies.size());
        for(::std::size_t i{};i<candidate.bodies.size();++i)
        {
            // [owned complete graph rows ... checked i] end
            // [safe] i<size before every indexed row/name borrow.
            auto const& body{candidate.bodies[i]};
            original_names.push_back(body.original_ir_name);
            if(body.shape!=1u || !body.endpoints_proved || body.original_ir_name.empty() || body.entry_object_name.empty())
            { detail::deny(candidate,blocker::unknown_body); }
            else { geometric.push_back(i); }
            if(!body.entry_object_name.empty()) { names.push_back({body.entry_object_name,i,false}); }
            if(!body.local_entry_object_name.empty()) { names.push_back({body.local_entry_object_name,i,true}); }
        }
        ::std::sort(original_names.begin(),original_names.end());
        for(::std::size_t i{1u};i<original_names.size();++i)
        { if(original_names[i-1u]==original_names[i]) { detail::deny(candidate,blocker::name_collision); } }
        ::std::sort(names.begin(),names.end(),[](auto const& a,auto const& b){ return a.name<b.name; });
        for(::std::size_t i{1u};i<names.size();++i)
        { if(names[i-1u].name==names[i].name) { detail::deny(candidate,blocker::name_collision); } }
        ::std::sort(geometric.begin(),geometric.end(),[&](auto a,auto b)
        { auto const& x{candidate.bodies[a]};auto const& y{candidate.bodies[b]};return x.section!=y.section ? x.section<y.section:x.begin_offset<y.begin_offset; });
        for(::std::size_t i{1u};i<geometric.size();++i)
        {
            auto const& left{candidate.bodies[geometric[i-1u]]};auto const& right{candidate.bodies[geometric[i]]};
            if(left.section==right.section && left.end_offset>right.begin_offset) { detail::deny(candidate,blocker::overlapping_body); }
        }
        ::std::vector<::std::uint64_t> section_anchors{};
        ::std::size_t total_name_bytes{};
        bool const elf{::llvm::isa<::llvm::object::ELFObjectFileBase>(object)};
        for(auto const& symbol:object.symbols())
        {
            if(candidate.claims.size()==detail::max_symbols || symbol.getObject()!=::std::addressof(object)) { return false; }
            auto name{symbol.getName()};if(!name) { ::llvm::consumeError(name.takeError());return false; }
            auto kind{symbol.getType()};if(!kind) { ::llvm::consumeError(kind.takeError());return false; }
            auto flags{symbol.getFlags()};if(!flags) { ::llvm::consumeError(flags.takeError());return false; }
            if(name->size()>native_owner_table_format::max_name_bytes || name->contains('\0') ||
               name->size()>graph::detail::max_object_bytes-total_name_bytes) { return false; }
            total_name_bytes+=name->size();
            claim value{};value.kind=static_cast<unsigned>(*kind);value.flags=*flags;
            if(!name->empty()) { value.name=::fast_io::concat_std(::fast_io::basic_io_scatter_t<char>{name->data(),name->size()}); }
            value.defined=(*flags & (::llvm::object::SymbolRef::SF_Undefined|::llvm::object::SymbolRef::SF_Absolute|::llvm::object::SymbolRef::SF_Common))==0u;
            auto selected{symbol.getSection()};if(!selected) { ::llvm::consumeError(selected.takeError());return false; }
            if(*selected!=object.section_end())
            {
                auto const& section{**selected};auto address{symbol.getAddress()};
                if(!address) { ::llvm::consumeError(address.takeError());return false; }
                auto const base{section.getAddress()},size{section.getSize()};
                if(*address<base || *address-base>size || size>graph::detail::max_object_bytes) { return false; }
                value.section=section.getIndex();value.offset=*address-base;value.section_size=size;
                value.located=true;value.text=section.isText();
            }
            if(elf && *kind==::llvm::object::SymbolRef::ST_Function)
            {
                // Actual enumerated in-bounds symbol; no relocation-created
                // unchecked index reaches ELF's non-Expected size accessor.
                value.explicit_size_known=true;value.explicit_size=::llvm::object::ELFSymbolRef{symbol}.getSize();
                if(value.defined && value.explicit_size==0u) { detail::deny(candidate,blocker::explicit_zero_size); }
            }
            auto found{::std::lower_bound(names.begin(),names.end(),::std::string_view{value.name},
                [](auto const& item,auto key){ return item.name<key; })};
            bool const matched{found!=names.end() && found->name==value.name};
            if(matched)
            {
                auto const index{found->body};auto const& body{candidate.bodies[index]};
                // name match is necessary, never sufficient. Real exact symbol
                // kind, code point / PPC descriptor graph and full row agree.
                bool valid{value.defined && value.located && *kind==::llvm::object::SymbolRef::ST_Function &&
                    ((*flags & ::llvm::object::SymbolRef::SF_Indirect)==0u) && body.endpoints_proved};
                if(valid && value.text) { valid=value.section==body.section && value.offset==body.begin_offset; }
                else if(valid) { valid=!found->local && detail::descriptor_matches(object,symbol,body,descriptors); }
                if(valid && value.explicit_size_known)
                { valid=value.explicit_size!=0u && value.explicit_size>=body.end_offset-body.begin_offset && value.explicit_size<=body.section_size-body.begin_offset; }
                if(!valid) { detail::deny(candidate,blocker::invalid_entry); }
                auto& count{found->local ? local_counts[index]:entry_counts[index]};
                if(++count!=1u) { detail::deny(candidate,blocker::name_collision); }
            }
            else if(detail::macho_section_anchor(object,symbol,value,detail::adjacent_body(candidate,geometric,value,false)))
            {
                // At most one actual section anchor. A second local alias at
                // this point remains a blocker even with a forged ltmpN name.
                if(::std::find(section_anchors.begin(),section_anchors.end(),value.section)!=section_anchors.end())
                { detail::deny(candidate,blocker::unexpected_alias); }
                section_anchors.push_back(value.section);
            }
            else if(value.defined && (*kind==::llvm::object::SymbolRef::ST_Function ||
                    (*flags & ::llvm::object::SymbolRef::SF_Indirect)!=0u))
            { detail::deny(candidate,blocker::unexpected_alias); }
            else if(value.defined && value.text && *kind!=::llvm::object::SymbolRef::ST_Debug && *kind!=::llvm::object::SymbolRef::ST_File)
            {
                // Metadata does not expand any proved endpoint. Preserve ALL
                // claims; actual function/global/weak aliases still decline.
                auto const inside{detail::adjacent_body(candidate,geometric,value,false)};
                bool accepted{detail::instruction_mapping(object,symbol,value,inside) ||
                    detail::retained_temporary(object,symbol,value,inside)};
                if(!accepted && inside.size()==1u && native_arm_mapping::kind(object,symbol,::llvm::StringRef{value.name})=='d')
                {
                    // Literal data is not an alias or a new callable body.
                    // Require the complete actual mapping interval to remain
                    // inside this one proved body. Provenance separately masks
                    // every one of its bytes, including conflicting DWARF rows.
                    auto const& body{inside[0u]};
                    for(auto const& data:arm_data)
                    {
                        if(data.section==value.section && data.begin==value.offset && data.section_size==value.section_size &&
                           data.begin>=body.begin_offset && data.end<=body.end_offset) { accepted=true;break; }
                    }
                }
                if(!accepted)
                {
                    auto const endpoint{detail::adjacent_body(candidate,geometric,value,true)};
                    accepted=detail::retained_temporary(object,symbol,value,endpoint);
                }
                if(!accepted) { detail::deny(candidate,blocker::unknown_text_claim); }
            }
            else if(value.defined && !value.located && *kind!=::llvm::object::SymbolRef::ST_Debug && *kind!=::llvm::object::SymbolRef::ST_File &&
                    (*flags & ::llvm::object::SymbolRef::SF_FormatSpecific)==0u)
            { detail::deny(candidate,blocker::unknown_defined_claim); }
            candidate.claims.push_back(::std::move(value));
        }
        for(::std::size_t i{};i<candidate.bodies.size();++i)
        {
            if(entry_counts[i]!=1u || local_counts[i]!=(candidate.bodies[i].local_entry_object_name.empty() ? 0u:1u))
            { detail::deny(candidate,blocker::missing_entry); }
        }
        candidate.complete=true;output=::std::move(candidate);return true;
    }
}
#endif
