// Actual parser + initializer tag identity, retained by immutable exception values across module teardown.
#define UWVM2TEST_STRICT_NO_INTERPRETER
#include "../strict/uwvm_int_translate_strict_common.h"
#include <uwvm2/runtime/exception/impl.h>
namespace test = uwvm2test::uwvm_int_strict;
namespace exn = uwvm2::runtime::exception;
namespace
{
    unsigned checks{};
#define CHECK(condition) do { ++checks; if(!(condition)) { std::fprintf(stderr, "FAIL %u: %s\n", __LINE__, #condition); std::abort(); } } while(false)
    test::byte_vec bytes(std::initializer_list<unsigned> input)
    {
        test::byte_vec result;
        for(auto value : input) { result.push_back(static_cast<std::byte>(value)); }
        return result;
    }
    test::byte_vec provider()
    {
        // Two ()->() tags with one identical signature. Export tag 0 as "a".
        return bytes({0,97,115,109,1,0,0,0, 1,4,1,0x60,0,0, 13,5,2,0,0,0,0, 7,5,1,1,'a',4,0});
    }
    test::byte_vec importer()
    {
        // Two imports alias provider.a; the local tag has the same signature but a distinct instance.
        return bytes({0,97,115,109,1,0,0,0, 1,4,1,0x60,0,0,
            2,15,2, 1,'p',1,'a',4,0,0, 1,'p',1,'a',4,0,0,
            13,3,1,0,0});
    }
}
int main()
{
    auto feature = test::wasm_feature_parameter_t{};
    uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_wasm1p1_parameter(feature).disable_exceptions = false;
    auto p = provider();
    auto m = importer();
    auto first = test::prepare_runtime_from_wasm(m, u8"consumer", {{&p, u8"p", &feature}}, feature);
    auto const& imports = first.mod->imported_tag_vec_storage;
    CHECK(imports.size() == 2);
    auto const* a = imports.index_unchecked(0).resolved_tag;
    auto const* b = imports.index_unchecked(1).resolved_tag;
    auto const& local = first.mod->local_defined_tag_vec_storage.index_unchecked(0);
    CHECK(a && a == b);
    CHECK(a->exception_identity && local.exception_identity);
    CHECK(a->exception_identity.get() != local.exception_identity.get());
    auto retained = exn::value::make(a->exception_identity, {});
    auto retained_alias = exn::value::make(b->exception_identity, {});
    auto retained_local = exn::value::make(local.exception_identity, {});
    std::weak_ptr<void const> old_tag = a->exception_identity;
    std::weak_ptr<void const> old_local = local.exception_identity;
    CHECK(retained->tag_identity() == retained_alias->tag_identity());
    CHECK(retained->tag_identity() != retained_local->tag_identity());
    // This helper clears and rebuilds BOTH parser and runtime module registries. Never dereference
    // a/b/local/first.mod again: only the independently owned exception tokens survive this boundary.
    auto second = test::prepare_runtime_from_wasm(m, u8"consumer", {{&p, u8"p", &feature}}, feature);
    auto const* next = second.mod->imported_tag_vec_storage.index_unchecked(0).resolved_tag;
    CHECK(!old_tag.expired() && !old_local.expired());
    CHECK(next->exception_identity.get() != retained->tag_identity());
    CHECK(next->exception_identity.get() == second.mod->imported_tag_vec_storage.index_unchecked(1).resolved_tag->exception_identity.get());
    retained.reset();
    CHECK(!old_tag.expired());
    retained_alias.reset();
    CHECK(old_tag.expired());
    retained_local.reset();
    CHECK(old_local.expired());
    std::printf("PASS %u initialized tag alias/lifetime checks\n", checks);
}
