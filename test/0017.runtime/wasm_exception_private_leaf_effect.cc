// Analysis component only: no Wasm decoder, canonical validator, source pin,
// native execution, JIT clone, bridge change or runtime publication is mocked.
#include <array>
#include <cstddef>
#include <exception>
#include <initializer_list>
#include <memory>
#include <new>
#include <span>
#include <utility>
#include <vector>
#include "../../src/uwvm2/runtime/compiler/shared/wasm_exception_private_leaf_effect.h"

namespace leaf = ::uwvm2::runtime::compiler::shared::wasm_exception_private_leaf_effect;

#if defined(UWVM2TEST_PRIVATE_LEAF_OOM) && UWVM2TEST_PRIVATE_LEAF_OOM == 1
// Isolated executable allocation fault, not an analysis/compiler API. All
// ordinary allocations use their genuine C allocator ABI; only the next
// replaceable operator-new allocation in this single-threaded test fails.
namespace { bool fail_next_allocation{}; }
# if defined(__APPLE__)
#  define UWVM2TEST_PRIVATE_LEAF_MALLOC "_malloc"
#  define UWVM2TEST_PRIVATE_LEAF_FREE "_free"
# else
#  define UWVM2TEST_PRIVATE_LEAF_MALLOC "malloc"
#  define UWVM2TEST_PRIVATE_LEAF_FREE "free"
# endif
extern "C" void* private_leaf_test_malloc(::std::size_t) noexcept __asm__(UWVM2TEST_PRIVATE_LEAF_MALLOC);
extern "C" void private_leaf_test_free(void*) noexcept __asm__(UWVM2TEST_PRIVATE_LEAF_FREE);
# undef UWVM2TEST_PRIVATE_LEAF_MALLOC
# undef UWVM2TEST_PRIVATE_LEAF_FREE
void* operator new(::std::size_t bytes)
{
    if(fail_next_allocation) { fail_next_allocation = false; throw ::std::bad_alloc{}; }
    // [actual native allocation: at least one byte][returned owning base]
    // [safe                                                          ] no
    // interior pointer or guest integer is supplied to this allocator.
    auto const allocation{private_leaf_test_malloc(bytes == 0uz ? 1uz : bytes)};
    if(allocation == nullptr) { throw ::std::bad_alloc{}; }
    return allocation;
}
void operator delete(void* allocation) noexcept
{
    // [same genuine allocation returned by replaceable new][owning base]
    // [safe                                                          ] pass
    // the base unchanged to its paired allocator; never read retired bytes.
    private_leaf_test_free(allocation);
}
void operator delete(void* allocation, ::std::size_t) noexcept { ::operator delete(allocation); }
#endif

void check(bool condition) noexcept { if(!condition) { ::std::terminate(); } }

leaf::linked_tag_identity make_tag(unsigned char value)
{
    auto root{::std::make_shared<unsigned char const>(value)};
    auto identity{leaf::linked_tag_identity::observe_actual_linked_instance(::std::move(root))};
    check(identity.has_value());
    return ::std::move(*identity);
}

leaf::function_observation make_leaf(leaf::observation_domain::owner domain, ::std::size_t index,
    ::std::span<leaf::linked_tag_identity const> tags,
    leaf::signature_class signature = leaf::signature_class::numeric,
    leaf::payload_class payload = leaf::payload_class::numeric)
{
    check(tags.size() < leaf::native_extent_limit);
    auto recorder{leaf::function_recorder::begin(::std::move(domain), index, tags.size()+1uz, signature)};
    check(recorder.has_value());
    ::std::size_t cursor{};
    for(auto const& tag: tags)
    {
        // [test-owned tag range ...] end
        // [safe                   ] borrow retained observations for one event;
        // byte offsets describe events, not addresses or validation evidence.
        check(recorder->observe_throw({cursor, cursor+1uz}, tag, payload, {}));
        ++cursor;
    }
    check(recorder->observe_outer_function_end({cursor, cursor+1uz}));
    auto observation{recorder->seal_observation()};
    check(observation.has_value());
    check(!recorder->seal_observation());
    return ::std::move(*observation);
}

leaf::function_observation make_caller(leaf::observation_domain::owner domain, ::std::size_t target,
    ::std::span<leaf::handler_observation const> handlers,
    leaf::call_route route = leaf::call_route::ordinary_local_direct)
{
    auto recorder{leaf::function_recorder::begin(::std::move(domain), 0uz, 2uz, leaf::signature_class::numeric)};
    check(recorder.has_value());
    check(recorder->observe_call({0uz, 1uz}, route, target, handlers));
    check(recorder->observe_outer_function_end({1uz, 2uz}));
    auto observation{recorder->seal_observation()};
    check(observation.has_value());
    return ::std::move(*observation);
}

int main()
{
    auto domain{leaf::observation_domain::create(0uz, 4uz)};
    check(leaf::observation_domain::has_actual_observation_owner(domain));
    check(!leaf::observation_domain::create(leaf::unknown_function, 4uz));
    check(!leaf::observation_domain::create(0uz, 0uz));
    check(!leaf::function_recorder::begin({}, 0uz, 1uz, leaf::signature_class::numeric));
    check(!leaf::function_recorder::begin(domain, 4uz, 1uz, leaf::signature_class::numeric));
    check(!leaf::function_recorder::begin(domain, 1uz, 0uz, leaf::signature_class::numeric));
    check(!leaf::function_recorder::begin(domain, 1uz, leaf::native_extent_limit+1uz, leaf::signature_class::numeric));
    auto nonowning_domain{leaf::observation_domain::owner{leaf::observation_domain::owner{}, domain.get()}};
    // [real retained domain][nonowning negative-test alias to its base]
    // [safe                                                      ] the genuine
    // owner remains live; this alias itself must not satisfy retention.
    check(!leaf::function_recorder::begin(nonowning_domain, 1uz, 1uz, leaf::signature_class::numeric));

    auto tag_root{::std::make_shared<unsigned char const>(7u)};
    auto tag_value{leaf::linked_tag_identity::observe_actual_linked_instance(tag_root)};
    auto alias_value{leaf::linked_tag_identity::observe_actual_linked_instance(
        ::std::shared_ptr<void const>{tag_root, tag_root.get()})};
    check(tag_value.has_value() && alias_value.has_value());
    auto const tag{*tag_value}, alias{*alias_value}, distinct{make_tag(7u)}, second{make_tag(9u)};
    check(tag.same_instance(alias));
    check(!tag.same_instance(distinct)); // Equal numeric payload/signature is not tag instance identity.
    check(!leaf::linked_tag_identity::observe_actual_linked_instance({}));
    check(!leaf::linked_tag_identity::observe_actual_linked_instance(
        ::std::shared_ptr<void const>{::std::shared_ptr<void const>{}, tag_root.get()}));

    auto const plain{leaf::handler_observation{leaf::catch_form::tagged, tag}};
    auto const plain_alias{leaf::handler_observation{leaf::catch_form::tagged, alias}};
    auto const retaining_alias{leaf::handler_observation{leaf::catch_form::tagged_reference, alias}};
    auto const unrelated_retaining{leaf::handler_observation{leaf::catch_form::tagged_reference, distinct}};
    auto const all{leaf::handler_observation{leaf::catch_form::all, {}}};
    auto const all_reference{leaf::handler_observation{leaf::catch_form::all_reference, {}}};
    ::std::array plain_handlers{plain};
    ::std::array alias_handlers{plain_alias};
    ::std::array alias_shadow{retaining_alias, plain};
    ::std::array all_shadow{all_reference, plain};
    ::std::array irrelevant_ref{unrelated_retaining, plain};
    ::std::array consuming_before_ref{plain, retaining_alias};
    ::std::array tagged_before_all{unrelated_retaining, all};
    ::std::array invalid_later{plain, leaf::handler_observation{}};
    ::std::array invalid_all{leaf::handler_observation{leaf::catch_form::all, tag}};
    ::std::array invalid_tag{leaf::handler_observation{leaf::catch_form::tagged, {}}};
    check(leaf::first_actual_handler(tag, alias_shadow) == leaf::handler_match::retaining);
    check(leaf::first_actual_handler(tag, all_shadow) == leaf::handler_match::retaining);
    check(leaf::first_actual_handler(tag, irrelevant_ref) == leaf::handler_match::consuming);
    check(leaf::first_actual_handler(tag, consuming_before_ref) == leaf::handler_match::consuming);
    check(leaf::first_actual_handler(tag, tagged_before_all) == leaf::handler_match::consuming);
    check(leaf::first_actual_handler(tag, {}) == leaf::handler_match::absent);
    check(leaf::first_actual_handler({}, plain_handlers) == leaf::handler_match::invalid);
    check(leaf::first_actual_handler(tag, invalid_later) == leaf::handler_match::invalid);
    check(!leaf::handlers_well_formed(invalid_all) && !leaf::handlers_well_formed(invalid_tag));

    ::std::array escaping{tag};
    auto callee{make_leaf(domain, 1uz, escaping)};
    check(callee.event_stream_complete() && callee.numeric_leaf_observed());
    auto caller{make_caller(domain, 1uz, plain_handlers)};
    check(!caller.numeric_leaf_observed()); // Caller need not itself qualify as a leaf.
    check(leaf::observe_consumed_direct_call(caller, 0uz, callee) == leaf::selection::observed_consumed);
    check(leaf::observe_consumed_direct_call(make_caller(domain, 1uz, alias_handlers), 0uz, callee) ==
        leaf::selection::observed_consumed);
    check(leaf::observe_consumed_direct_call(make_caller(domain, 1uz, alias_shadow), 0uz, callee) ==
        leaf::selection::retaining_handler);
    check(leaf::observe_consumed_direct_call(make_caller(domain, 1uz, all_shadow), 0uz, callee) ==
        leaf::selection::retaining_handler);
    check(leaf::observe_consumed_direct_call(make_caller(domain, 1uz, irrelevant_ref), 0uz, callee) ==
        leaf::selection::observed_consumed);
    check(leaf::observe_consumed_direct_call(make_caller(domain, 1uz, {}), 0uz, callee) ==
        leaf::selection::unconsumed_tag);
    ::std::array same_payload_wrong_instance{leaf::handler_observation{leaf::catch_form::tagged, distinct}};
    check(leaf::observe_consumed_direct_call(make_caller(domain, 1uz, same_payload_wrong_instance), 0uz, callee) ==
        leaf::selection::unconsumed_tag);

    ::std::array two_escaping{tag, second};
    auto two_tag_leaf{make_leaf(domain, 2uz, two_escaping)};
    check(leaf::observe_consumed_direct_call(make_caller(domain, 2uz, plain_handlers), 0uz, two_tag_leaf) ==
        leaf::selection::unconsumed_tag);
    ::std::array two_handlers{plain, leaf::handler_observation{leaf::catch_form::tagged, second}};
    check(leaf::observe_consumed_direct_call(make_caller(domain, 2uz, two_handlers), 0uz, two_tag_leaf) ==
        leaf::selection::observed_consumed);
    ::std::array duplicate_escaping{tag, alias, tag};
    check(make_leaf(domain, 2uz, duplicate_escaping).escaping_tag_count() == 1uz);

    // The caller retains its exact original ordered snapshot, never a span
    // into mutable decoder/control-stack storage after observe_call returns.
    ::std::vector<leaf::handler_observation> temporary_handlers{plain};
    auto copied_caller{make_caller(domain, 1uz, temporary_handlers)};
    temporary_handlers[0uz] = all_reference;
    temporary_handlers.clear();
    check(leaf::observe_consumed_direct_call(copied_caller, 0uz, callee) == leaf::selection::observed_consumed);

    auto other_domain{leaf::observation_domain::create(0uz, 4uz)};
    auto other_leaf{make_leaf(other_domain, 1uz, escaping)};
    check(leaf::observe_consumed_direct_call(caller, 0uz, other_leaf) == leaf::selection::different_domain);
    check(leaf::observe_consumed_direct_call(make_caller(domain, 2uz, plain_handlers), 0uz, callee) ==
        leaf::selection::wrong_target);
    check(leaf::observe_consumed_direct_call(caller, 1uz, callee) == leaf::selection::wrong_target);
    for(auto const route: {leaf::call_route::tail, leaf::call_route::indirect_or_reference,
        leaf::call_route::imported_or_host, leaf::call_route::unknown})
    {
        check(leaf::observe_consumed_direct_call(make_caller(domain, 1uz, plain_handlers, route), 0uz, callee) ==
            leaf::selection::wrong_target);
    }
    check(leaf::observe_consumed_direct_call(make_caller(domain, leaf::unknown_function, plain_handlers), 0uz, callee) ==
        leaf::selection::wrong_target);

    for(auto const signature: {leaf::signature_class::reference_or_vector, leaf::signature_class::unknown})
    {
        check(leaf::observe_consumed_direct_call(caller, 0uz, make_leaf(domain, 1uz, escaping, signature)) ==
            leaf::selection::unsupported_leaf);
    }
    for(auto const payload: {leaf::payload_class::reference_or_vector, leaf::payload_class::unknown})
    {
        check(leaf::observe_consumed_direct_call(caller, 0uz,
            make_leaf(domain, 1uz, escaping, leaf::signature_class::numeric, payload)) == leaf::selection::unsupported_leaf);
    }
    for(auto const effect: {leaf::disqualifying_effect::reference, leaf::disqualifying_effect::retaining_handler,
        leaf::disqualifying_effect::throw_reference, leaf::disqualifying_effect::tail_transfer,
        leaf::disqualifying_effect::memory_or_table_or_global, leaf::disqualifying_effect::host_or_unknown,
        leaf::disqualifying_effect::unknown_opcode})
    {
        auto recorder{leaf::function_recorder::begin(domain, 1uz, 3uz, leaf::signature_class::numeric)};
        check(recorder.has_value());
        check(recorder->observe_throw({0uz, 1uz}, tag, leaf::payload_class::numeric, {}));
        check(recorder->observe_disqualifying({1uz, 2uz}, effect));
        check(recorder->observe_outer_function_end({2uz, 3uz}));
        auto observation{recorder->seal_observation()};
        check(observation.has_value());
        check(leaf::observe_consumed_direct_call(caller, 0uz, *observation) == leaf::selection::unsupported_leaf);
    }
    {
        auto recorder{leaf::function_recorder::begin(domain, 1uz, 3uz, leaf::signature_class::numeric)};
        check(recorder.has_value());
        check(recorder->observe_throw({0uz, 1uz}, tag, leaf::payload_class::numeric, {}));
        check(recorder->observe_call({1uz, 2uz}, leaf::call_route::ordinary_local_direct, 2uz, {}));
        check(recorder->observe_outer_function_end({2uz, 3uz}));
        auto observation{recorder->seal_observation()};
        check(observation.has_value());
        check(leaf::observe_consumed_direct_call(caller, 0uz, *observation) == leaf::selection::unsupported_leaf);
    }
    {
        auto recorder{leaf::function_recorder::begin(domain, 1uz, 3uz, leaf::signature_class::numeric)};
        check(recorder.has_value());
        check(recorder->observe_handlers({0uz, 1uz}, alias_shadow));
        check(recorder->observe_throw({1uz, 2uz}, tag, leaf::payload_class::numeric, alias_shadow));
        check(recorder->observe_outer_function_end({2uz, 3uz}));
        auto observation{recorder->seal_observation()};
        check(observation.has_value() && !observation->numeric_leaf_observed());
    }
    {
        auto recorder{leaf::function_recorder::begin(domain, 1uz, 2uz, leaf::signature_class::numeric)};
        check(recorder.has_value());
        check(recorder->observe_throw({0uz, 1uz}, tag, leaf::payload_class::numeric, plain_handlers));
        check(recorder->observe_outer_function_end({1uz, 2uz}));
        auto observation{recorder->seal_observation()};
        check(observation.has_value() && observation->escaping_tag_count() == 0uz);
        check(leaf::observe_consumed_direct_call(caller, 0uz, *observation) == leaf::selection::no_escaping_tag);
    }

    // Rejected/incomplete streams remain poisoned, rather than silently
    // sealing a leaf with a missing last opcode, throw or callsite.
    {
        auto recorder{leaf::function_recorder::begin(domain, 1uz, 2uz, leaf::signature_class::numeric)};
        check(recorder.has_value());
        check(recorder->observe_scalar_or_control({0uz, 1uz}));
        check(!recorder->seal_observation());
        check(!recorder->observe_scalar_or_control({0uz, 1uz})); // overlap
        check(!recorder->observe_outer_function_end({1uz, 2uz}));
        check(!recorder->seal_observation());
    }
    for(auto const range: {leaf::validated_byte_range{1uz, 2uz}, leaf::validated_byte_range{0uz, 0uz},
        leaf::validated_byte_range{0uz, 3uz}, leaf::validated_byte_range{2uz, 1uz}})
    {
        auto recorder{leaf::function_recorder::begin(domain, 1uz, 2uz, leaf::signature_class::numeric)};
        check(recorder.has_value());
        check(!recorder->observe_scalar_or_control(range));
        check(!recorder->seal_observation());
    }
    {
        auto recorder{leaf::function_recorder::begin(domain, 1uz, 2uz, leaf::signature_class::numeric)};
        check(recorder.has_value());
        check(!recorder->observe_outer_function_end({0uz, 1uz}));
        check(!recorder->observe_outer_function_end({1uz, 2uz}));
        check(!recorder->seal_observation());
    }
    {
        auto recorder{leaf::function_recorder::begin(domain, 0uz, 2uz, leaf::signature_class::numeric)};
        check(recorder.has_value());
        check(!recorder->observe_call({0uz, 1uz}, leaf::call_route::ordinary_local_direct, 4uz, plain_handlers));
        check(!recorder->seal_observation());
    }
    {
        auto recorder{leaf::function_recorder::begin(domain, 0uz, 2uz, leaf::signature_class::numeric)};
        check(recorder.has_value());
        check(!recorder->observe_call({0uz, 1uz}, leaf::call_route::ordinary_local_direct, 1uz, invalid_later));
        check(!recorder->seal_observation());
    }
    {
        auto recorder{leaf::function_recorder::begin(domain, 1uz, 2uz, leaf::signature_class::numeric)};
        check(recorder.has_value());
        check(!recorder->observe_throw({0uz, 1uz}, {}, leaf::payload_class::numeric, {}));
        check(!recorder->seal_observation());
    }
    {
        auto recorder{leaf::function_recorder::begin(domain, 1uz, 1uz, leaf::signature_class::numeric)};
        check(recorder.has_value());
        check(recorder->observe_outer_function_end({0uz, 1uz}));
        auto observation{recorder->seal_observation()};
        check(observation.has_value());
        check(!recorder->observe_scalar_or_control({0uz, 1uz}));
        check(!recorder->seal_observation());
    }
    auto moved_callee{::std::move(callee)};
    check(leaf::observe_consumed_direct_call(caller, 0uz, callee) == leaf::selection::incomplete);
    check(leaf::observe_consumed_direct_call(caller, 0uz, moved_callee) == leaf::selection::observed_consumed);

#if defined(UWVM2TEST_PRIVATE_LEAF_OOM) && UWVM2TEST_PRIVATE_LEAF_OOM == 1
    {
        auto recorder{leaf::function_recorder::begin(domain, 1uz, 2uz, leaf::signature_class::numeric)};
        check(recorder.has_value());
        bool caught{};
        fail_next_allocation = true;
        try { static_cast<void>(recorder->observe_throw({0uz, 1uz}, tag, leaf::payload_class::numeric, {})); }
        catch(::std::bad_alloc const&) { caught = true; }
        check(caught && !fail_next_allocation);
        check(!recorder->observe_outer_function_end({1uz, 2uz}));
        check(!recorder->seal_observation());
    }
    {
        auto recorder{leaf::function_recorder::begin(domain, 0uz, 2uz, leaf::signature_class::numeric)};
        check(recorder.has_value());
        bool caught{};
        fail_next_allocation = true;
        try { static_cast<void>(recorder->observe_call({0uz, 1uz}, leaf::call_route::ordinary_local_direct, 1uz, plain_handlers)); }
        catch(::std::bad_alloc const&) { caught = true; }
        check(caught && !fail_next_allocation);
        check(!recorder->observe_outer_function_end({1uz, 2uz}));
        check(!recorder->seal_observation());
    }
#endif
}
