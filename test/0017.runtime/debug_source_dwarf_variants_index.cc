// Genuine producer/link relocation metadata evidence. This executable grants no
// runtime frame authority and does not claim actual guest object-value copying.
#include <uwvm2/uwvm/debugger/source_dwarf_index.h>
#include <uwvm2/uwvm/debugger/source_dwarf_objects.h>
#include <fast_io.h>
using namespace uwvm2::uwvm::debugger::source_dwarf;
using metadata_index = uwvm2::uwvm::debugger::source_dwarf::index;
static void check(bool ok, char const* message)
{ if(!ok) { ::fast_io::io::perrln("debug_source_dwarf_variants_index: ", ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); } }
int main(int argc, char const* const* argv)
{
    check(argc == 3, "require actual linked Wasm fixture and producer label");
    ::std::string_view const label{argv[2]};
    check(label == "rust-variants" || label == "c-globals" || label == "cpp-globals" || label == "rust-globals", "known real producer label");
    ::fast_io::native_file_loader file{::fast_io::mnp::os_c_str(argv[1]), ::fast_io::open_mode::in};
    check(file.size() >= 8u, "Wasm header bounds");
    // [RAII-owned bytes ... Wasm header (8) ... module_end]
    // [safe                                             ] unsafe (one-past)
    //  ^^ minimum checked before pointer/span derivation or any header read.
    ::std::span<::std::byte const> const bytes{reinterpret_cast<::std::byte const*>(file.data()), file.size()};
    ::std::array<unsigned char, 8u> const magic{0u, 0x61u, 0x73u, 0x6du, 1u, 0u, 0u, 0u};
    for(::std::size_t i{}; i != magic.size(); ++i) { check(::std::to_integer<unsigned char>(bytes[i]) == magic[i], "actual Wasm header"); }
    details::reader module{bytes, 8u}; ::std::vector<section> sections{}; ::std::uint64_t code_size{};
    while(module.cursor != bytes.size())
    {
        ::std::uint8_t id{}; ::std::uint32_t size{};
        check(module.byte(id) && module.leb(size) && size <= bytes.size() - module.cursor, "bounded actual section payload");
        // [safe] checked section length BEFORE subspan and scalar position advance.
        auto const payload{bytes.subspan(module.cursor, size)}; module.cursor += size;
        if(id == 10u) { check(code_size == 0u, "unique code section"); code_size = size; }
        if(id != 0u) { continue; }
        details::reader custom{payload}; ::std::uint32_t length{};
        check(custom.leb(length) && length <= payload.size() - custom.cursor, "bounded custom name");
        // [custom payload ... cursor ... cursor+length ... end]
        // [safe                                              ] unsafe (one-past)
        //                    ^^ checked length precedes string-view pointer derivation.
        ::std::string_view const name{reinterpret_cast<char const*>(payload.data() + custom.cursor), length};
        custom.cursor += length; // checked scalar advance may reach payload_end.
        if(name.starts_with(".debug_") || name == "external_debug_info") { sections.push_back({name, payload.subspan(custom.cursor)}); }
    }
    check(code_size != 0u, "actual executable code present"); ::std::unique_ptr<metadata_index> parsed{};
    auto const status{metadata_index::parse({sections, code_size, 4u}, parsed)};
    if(status != error::none) { ::fast_io::io::perrln("actual parse status=", ::fast_io::mnp::dec(static_cast<unsigned>(status))); }
    check(status == error::none && parsed, "actual final producer embedded DWARF accepted");
    if(label == "rust-variants")
    {
        variable_record const* choice{}; variable_record const* niche{};
        for(auto const& variable : parsed->variables())
        { if(variable.name == "choice") { check(choice == nullptr, "one actual choice variable"); choice = ::std::addressof(variable); }
          if(variable.name == "niche") { check(niche == nullptr, "one actual niche variable"); niche = ::std::addressof(variable); } }
        check(choice && choice->type < parsed->types().size() && niche && niche->type < parsed->types().size(), "actual Rust aggregate variables retained");
        auto const& type{parsed->types()[choice->type]};
        check(type.variant_parts.size() == 1u, "real Rust payload enum has a variant part"); auto const& part{type.variant_parts.front()};
        check(part.has_discriminant && part.discriminant_supported && part.discriminant_signed && part.discriminant_bytes == 1u, "real signed i8 discriminator metadata");
        bool negative{}, positive{}, empty{};
        for(auto const& variant : part.variants)
        {
            check(variant.selector_kind == variant_selector_kind::intervals && variant.selectors.size() == 1u, "actual tagged variants have one explicit label");
            auto const bits{variant.selectors.front().low};
            negative |= bits == static_cast<::std::uint64_t>(-3); positive |= bits == 7u; empty |= bits == 11u;
        }
        check(negative && positive && empty, "actual signed and positive discriminator labels preserved exactly");
        auto const& niche_type{parsed->types()[niche->type]}; check(niche_type.variant_parts.size() == 1u, "actual Option NonZero has a variant part");
        auto const& niche_part{niche_type.variant_parts.front()};
        check(niche_part.has_discriminant && niche_part.discriminant_supported && !niche_part.discriminant_signed && niche_part.discriminant_bytes == 4u,
              "actual Rust niche discriminator unsigned width");
        bool has_default{}, has_zero{};
        for(auto const& variant : niche_part.variants)
        { has_default |= variant.selector_kind == variant_selector_kind::default_case;
          for(auto const& selector : variant.selectors) { has_zero |= selector.low == 0u && selector.high == 0u; } }
        check(has_default && has_zero, "real niche default and explicit zero selector retained without a heuristic");
        ::std::vector<object_node> layout{};
        check(query_type_layout(parsed->types(), choice->type, layout) == inline_query_error::none, "actual variant type graph bounded expansion");
        for(auto const& node : layout) { ::fast_io::io::println(object_details_of(node)); }
        ::fast_io::io::println("PASS actual Rust signed payload and niche variant metadata"); return 0;
    }
    auto const public_name{label == "rust-globals" ? ::std::string_view{"PUBLIC_COUNTER"} : ::std::string_view{"public_counter"}};
    variable_record const* global{}; bool file_static{}, class_static{}, function_static{}, lexical_local{};
    for(auto const& variable : parsed->variables())
    {
        if(variable.name == public_name && variable.global && !variable.declaration)
        { check(global == nullptr, "unique actual public global definition"); global = ::std::addressof(variable); }
        if(variable.global && (variable.name == "file_counter" || variable.name == "FILE_COUNTER") && !variable.declaration) { file_static = true; }
        if(variable.name == "class_counter" && variable.global && !variable.declaration) { class_static = variable.qualified_name.find("Counter::class_counter") != ::std::string::npos; }
        if(variable.name == "function_counter" && !variable.global && variable.static_storage) { function_static = true; }
        if(variable.name == "public_counter" && !variable.global) { lexical_local = true; }
    }
    check(global && global->static_storage && file_static && lexical_local, "actual globals/file-static metadata and local shadowing retained");
    if(label != "rust-globals") { check(function_static, "actual function-local static has static storage"); }
    if(label == "cpp-globals") { check(class_static, "actual out-of-class C++ static definition qualified through specification"); }
    bool address{}; for(auto const& location : global->locations) { address |= location.plan.kind == plan_kind::absolute_guest_offset; }
    check(address, "actual final linked DW_OP_addr/addrx resolves as guest offset only");
    bool queried{};
    for(auto const& scope : parsed->scopes())
    {
        if(scope.kind != scope_kind::subprogram || !scope.concrete || scope.ranges.empty()) { continue; }
        variable_selection selected{};
        auto const query{query_named_variable(parsed->scopes(), parsed->types(), parsed->variables(), scope.ranges.front().begin,
            global->qualified_name, selected)};
        if(query == inline_query_error::none && selected.identity == global->identity && selected.global) { queried = true; break; }
    }
    check(queried, "actual global metadata lookup resolves under a concrete physical source scope");
    if(label != "rust-globals")
    {
        variable_record const* function_static{};
        for(auto const& variable : parsed->variables())
        { if(variable.name == "function_counter" && !variable.global && variable.static_storage) { function_static = ::std::addressof(variable); } }
        check(function_static != nullptr && !function_static->qualified_name.empty(), "real function static keeps actual qualified defining scope");
        bool available_in_owner{}, denied_outside_owner{};
        for(auto const& scope : parsed->scopes())
        {
            if(scope.kind != scope_kind::subprogram || !scope.concrete || scope.ranges.empty()) { continue; }
            variable_selection selected{};
            auto const query{query_named_variable(parsed->scopes(), parsed->types(), parsed->variables(), scope.ranges.front().begin,
                function_static->qualified_name, selected)};
            if(query == inline_query_error::none)
            { check(selected.identity == function_static->identity && selected.physical_scope < parsed->scopes().size() &&
                    parsed->scopes()[selected.physical_scope].identity == scope.identity, "qualified function static does not fabricate another function activation"); available_in_owner = true; }
            else if(query == inline_query_error::unavailable) { denied_outside_owner = true; }
        }
        check(available_in_owner && denied_outside_owner, "real qualified function static available in defining activation and explicit unavailable across physical functions");
    }
    ::fast_io::io::println("PASS actual ", escaped_metadata_text{label}, " global/static metadata, names and linked guest offsets");
}
