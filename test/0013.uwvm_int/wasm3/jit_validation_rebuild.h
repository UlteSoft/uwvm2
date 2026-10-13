// Included inside declaration_policy.cc's test namespace, only for the LLVM JIT fixture.
// Rebuild the real finalized runtime module; never substitute the original parsed module.
namespace jit_rebuild = uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
using jit_rebuild_traits = jit_rebuild::validation_module_traits_t;

template<typename Section>
auto const& jit_rebuild_section(auto const& module)
{
    return uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<Section>(module.sections);
}

void check_jit_rebuild_metadata(auto const& runtime, auto const& rebuilt)
{
    auto const& types = jit_rebuild_section<jit_rebuild_traits::type_section_storage_t>(rebuilt);
    auto const& tables = jit_rebuild_section<jit_rebuild_traits::table_section_storage_t>(rebuilt);
    auto const& globals = jit_rebuild_section<jit_rebuild_traits::global_section_storage_t>(rebuilt);
    auto const& elements = jit_rebuild_section<jit_rebuild_traits::element_section_storage_t>(rebuilt);
    auto const& tags = jit_rebuild_section<jit_rebuild_traits::tag_section_storage_t>(rebuilt);
    UWVM2TEST_REQUIRE(types.requires_function_references == runtime.type_section_storage.requires_function_references);
    UWVM2TEST_REQUIRE(tables.requires_function_references == runtime.table_declarations_require_function_references);
    UWVM2TEST_REQUIRE(globals.requires_function_references == runtime.global_declarations_require_function_references);
    UWVM2TEST_REQUIRE(elements.requires_function_references == runtime.element_declarations_require_function_references);
    UWVM2TEST_REQUIRE(tags.present == runtime.tag_section_present);
    UWVM2TEST_REQUIRE(tags.type_indices.size() == runtime.local_defined_tag_vec_storage.size());
    for(std::size_t i{}; i != tags.type_indices.size(); ++i)
    {
        UWVM2TEST_REQUIRE(tags.type_indices.index_unchecked(i) == runtime.local_defined_tag_vec_storage.index_unchecked(i).type_index);
    }
}

void check_jit_declaration_rebuild(auto const& runtime, auto const& policy, bool valid)
{
    auto rebuilt = jit_rebuild::build_runtime_validation_module(runtime);
    check_jit_rebuild_metadata(runtime, rebuilt);
    auto const& body = jit_rebuild_section<jit_rebuild_traits::code_section_storage_t>(rebuilt).codes.index_unchecked(0).body;
    error_t error{};
    try
    {
        v3::validate_code(v3::wasm3_code_version{}, rebuilt, runtime.imported_function_vec_storage.size(),
                         reinterpret_cast<std::byte const*>(body.expr_begin), reinterpret_cast<std::byte const*>(body.code_end), error, policy);
    }
    catch(fast_io::error const&) {}
    UWVM2TEST_REQUIRE((error.err_code == error_code::ok) == valid);
    if(!valid) { UWVM2TEST_REQUIRE(error.err_code == error_code::wasm1p1_feature_required); }
}

void jit_rebuild_append_section(byte_vec& output, unsigned id, byte_vec const& payload)
{
    append_u8(output, id);
    append_u32_leb(output, static_cast<std::uint32_t>(payload.size()));
    output.insert(output.end(), payload.begin(), payload.end());
}

byte_vec jit_rebuild_module_prefix()
{
    byte_vec output;
    bytes(output, {0, 0x61, 0x73, 0x6d, 1, 0, 0, 0});
    byte_vec types;
    // Function () -> (), tag (i32), tag (i64): distinct type indices exercise tag payload selection.
    bytes(types, {3, 0x60, 0, 0, 0x60, 1, 0x7f, 0, 0x60, 1, 0x7e, 0});
    jit_rebuild_append_section(output, 1, types);
    return output;
}

byte_vec jit_rebuild_provider()
{
    auto output = jit_rebuild_module_prefix();
    auto section = [&](unsigned id, std::initializer_list<unsigned> data)
    {
        byte_vec payload;
        bytes(payload, data);
        jit_rebuild_append_section(output, id, payload);
    };
    section(3, {1, 0});
    section(4, {1, 0x70, 0, 1});
    section(5, {1, 0, 0});
    section(13, {2, 0, 1, 0, 2});
    section(6, {1, 0x7f, 0, 0x41, 0, 11});
    section(7, {6, 1, 'f', 0, 0, 1, 't', 1, 0, 1, 'm', 2, 0,
               1, 'g', 3, 0, 1, 'i', 4, 0, 1, 'j', 4, 1});
    section(10, {1, 2, 0, 11});
    return output;
}

byte_vec jit_rebuild_consumer(unsigned imported_tags, byte_vec const& body)
{
    auto output = jit_rebuild_module_prefix();
    byte_vec imports;
    append_u32_leb(imports, imported_tags + 4);
    // Deliberately put tags before the other four kinds. The rebuilt storage groups kinds differently.
    for(unsigned i{}; i != imported_tags; ++i)
    {
        bytes(imports, {1, 'p', 1, (i & 1u) ? unsigned('j') : unsigned('i'), 4, 0, 1 + (i & 1u)});
    }
    bytes(imports, {1, 'p', 1, 'g', 3, 0x7f, 0, 1, 'p', 1, 'm', 2, 0, 0,
                    1, 'p', 1, 't', 1, 0x70, 0, 1, 1, 'p', 1, 'f', 0, 0});
    jit_rebuild_append_section(output, 2, imports);
    byte_vec functions;
    bytes(functions, {1, 0});
    jit_rebuild_append_section(output, 3, functions);
    byte_vec tags;
    bytes(tags, {2, 0, 2, 0, 1});
    jit_rebuild_append_section(output, 13, tags);
    byte_vec code;
    append_u8(code, 1);
    append_u32_leb(code, static_cast<std::uint32_t>(body.size()));
    code.insert(code.end(), body.begin(), body.end());
    jit_rebuild_append_section(output, 10, code);
    return output;
}

void check_jit_rebuild_import_pointers(auto const& rebuilt, unsigned imported_tags)
{
    auto const& imports = jit_rebuild_section<jit_rebuild_traits::import_section_storage_t>(rebuilt);
    auto const& types = jit_rebuild_section<jit_rebuild_traits::type_section_storage_t>(rebuilt);
    UWVM2TEST_REQUIRE(imports.imports.size() == imported_tags + 4);
    std::size_t copied_index{};
    for(unsigned kind{}; kind != 5; ++kind)
    {
        auto const& descriptors = imports.importdesc.index_unchecked(kind);
        UWVM2TEST_REQUIRE(descriptors.size() == (kind == 4 ? imported_tags : 1));
        for(auto const* descriptor : descriptors)
        {
            // Establish ownership before dereferencing: appending tags must not invalidate earlier descriptor pointers.
            UWVM2TEST_REQUIRE(descriptor == std::addressof(imports.imports.index_unchecked(copied_index)));
            ++copied_index;
            UWVM2TEST_REQUIRE(static_cast<unsigned>(descriptor->imports.type) == kind);
            if(kind == 0)
            {
                UWVM2TEST_REQUIRE(descriptor->imports.storage.function == std::addressof(types.types.index_unchecked(0)));
            }
            if(kind == 4)
            {
                UWVM2TEST_REQUIRE(descriptor->imports.storage.tag_type_index == 1 + ((copied_index - 5) & 1u));
            }
        }
    }
    UWVM2TEST_REQUIRE(copied_index == imports.imports.size());
}

void check_jit_tag_rebuild()
{
    unsigned focused_cases{};
    for(unsigned imported_tags : {1u, 2u, 128u})
    {
        // Exercise both ends of the imported index space and both local tag signatures.
        for(unsigned tag_index : {0u, imported_tags - 1u, imported_tags, imported_tags + 1u})
        {
            unsigned const payload_type = tag_index < imported_tags ? ((tag_index & 1u) ? 0x7e : 0x7f)
                                                                    : (tag_index == imported_tags ? 0x7e : 0x7f);
            for(bool table : {false, true})
            {
                ++case_id;
                byte_vec body;
                append_u8(body, 0); // Empty local declaration vector.
                if(table)
                {
                    bytes(body, {2, payload_type, 0x1f, payload_type, 1, 0});
                    append_u32_leb(body, tag_index);
                    append_u8(body, 0); // Catch targets the enclosing block, outside try_table.
                }
                bytes(body, {payload_type == 0x7f ? 0x41u : 0x42u, 7, 0x08});
                append_u32_leb(body, tag_index);
                if(table) { bytes(body, {11, 11, 0x1a}); }
                append_u8(body, 11);
                auto provider = jit_rebuild_provider();
                auto consumer = jit_rebuild_consumer(imported_tags, body);
                auto policy = features();
                auto prepared = prepare_runtime_from_wasm(consumer, u8"jit-tag-rebuild", {{&provider, u8"p", &policy}}, policy);
                auto rebuilt = jit_rebuild::build_runtime_validation_module(*prepared.mod);
                check_jit_rebuild_metadata(*prepared.mod, rebuilt);
                check_jit_rebuild_import_pointers(rebuilt, imported_tags);
                auto const& code = jit_rebuild_section<jit_rebuild_traits::code_section_storage_t>(rebuilt).codes.index_unchecked(0).body;
                for(unsigned mode{}; mode != 3; ++mode)
                {
                    ++focused_cases;
                    error_t error{};
                    auto selected = policy;
                    if(mode == 2) { uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(selected).disable_exceptions = true; }
                    try
                    {
                        if(mode == 1)
                        {
                            v3::validate_code(v3::wasm3_code_version{}, rebuilt, 1, reinterpret_cast<std::byte const*>(code.expr_begin),
                                             reinterpret_cast<std::byte const*>(code.code_end), error);
                        }
                        else
                        {
                            v3::validate_code(v3::wasm3_code_version{}, rebuilt, 1, reinterpret_cast<std::byte const*>(code.expr_begin),
                                             reinterpret_cast<std::byte const*>(code.code_end), error, selected);
                        }
                    }
                    catch(fast_io::error const&) {}
                    UWVM2TEST_REQUIRE((error.err_code == error_code::ok) == (mode != 2));
                    if(mode == 2) { UWVM2TEST_REQUIRE(error.err_code == error_code::wasm1p1_feature_required); }
                }
            }
        }
    }
    std::printf("PASS %u JIT rebuilt-module tag throw/try_table policy cases; all five import kinds retain owned descriptor pointers\n", focused_cases);
}
