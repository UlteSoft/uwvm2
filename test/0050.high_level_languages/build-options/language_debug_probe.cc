#include "uwvm2/uwvm/debugger/source_scalar_expression.h"
#include "uwvm2/uwvm/debugger/source_language_expression.h"
#include "uwvm2/uwvm/debugger/source_pretty_coroutine.h"
#include "uwvm2/uwvm/debugger/source_pretty_sequences.h"
#include "uwvm2/uwvm/debugger/source_pretty_libcxx_nodes.h"
#include "uwvm2/uwvm/debugger/source_pretty_values.h"
#include "uwvm2/uwvm/debugger/source_pretty_array.h"
#include <array>
#include <cstdio>

#ifdef UWVM_TEST_EXPECT_MASK
static_assert(UWVM_DEBUG_LANGUAGE_MASK == UWVM_TEST_EXPECT_MASK);
#endif

int main()
{
    namespace scalar = uwvm2::uwvm::debugger::source_scalar_expression;
    namespace language = uwvm2::uwvm::debugger::source_language_expression;
    namespace pretty = uwvm2::uwvm::debugger::source_pretty_printer;
    namespace dwarf = uwvm2::uwvm::debugger::source_dwarf;
    scalar::program code{};
    if(scalar::parse("1+2",code) != scalar::error::none) { return 1; }
    auto check = [&](scalar::language_semantics selected, bool enabled)
    {
        auto const status{scalar::parse("1",code,selected)};
        return enabled ? status == scalar::error::none : status == scalar::error::unsupported;
    };
    if(!check(scalar::language_semantics::c,UWVM_DEBUG_LANGUAGE_C) ||
       !check(scalar::language_semantics::cpp,UWVM_DEBUG_LANGUAGE_CPP) ||
       !check(scalar::language_semantics::rust,UWVM_DEBUG_LANGUAGE_RUST) ||
       !check(scalar::language_semantics::go,UWVM_DEBUG_LANGUAGE_GO_FAMILY) ||
       !check(scalar::language_semantics::zig,UWVM_DEBUG_LANGUAGE_ZIG)) { return 2; }
    dwarf::type_record go{},tinygo{};
    go.language=0x16u; tinygo.language=0x0cu; tinygo.tinygo_producer=true;
    if(language::details::go_language(go) != bool(UWVM_DEBUG_LANGUAGE_GO) ||
       language::details::go_language(tinygo) != bool(UWVM_DEBUG_LANGUAGE_TINYGO)) { return 3; }
    unsigned reads{};
    auto reader = [&](::std::uint64_t, ::std::size_t, ::std::vector<::std::byte>&) { ++reads; return false; };
    auto format = [&](::std::uint64_t id, char const* name)
    {
        ::std::vector<dwarf::type_record> types(1);
        types[0].kind=dwarf::type_kind::structure; types[0].language=id;
        types[0].name=name; types[0].size_known=true; types[0].byte_size=24;
        ::std::vector<dwarf::object_node> tree(1);
        tree[0].kind=dwarf::type_kind::structure; tree[0].type=0;
        ::std::array<::std::byte,24> bytes{};
        return pretty::format(types,0,bytes,{}, {},4,reader,tree);
    };
    auto const cpp{format(0x04u,"std::vector<unsigned int>")};
    auto const rust{format(0x1cu,"Vec<u32>")};
    // A name-only container has no layout proof. Enabled adapters may decline
    // its schema or report it unavailable; neither result may read the guest.
    // A disabled adapter must decline it. Scalar parsing above independently
    // checks that the enabled language paths are present.
    auto refuses_incomplete = [](pretty::outcome status, bool enabled)
    {
        return enabled ? status == pretty::outcome::unavailable || status == pretty::outcome::not_matched
                       : status == pretty::outcome::not_matched;
    };
    if(!refuses_incomplete(cpp,UWVM_DEBUG_LANGUAGE_CPP) ||
       !refuses_incomplete(rust,UWVM_DEBUG_LANGUAGE_RUST) || reads)
    {
        ::std::fprintf(stderr,"LANGUAGE_BUILD_SELECTION_FAILURE cpp=%u rust=%u reads=%u\n",
            unsigned(cpp),unsigned(rust),reads);
        return 4;
    }
    ::std::printf("LANGUAGE_BUILD_SELECTION_OK %u\n",unsigned(UWVM_DEBUG_LANGUAGE_MASK));
}
