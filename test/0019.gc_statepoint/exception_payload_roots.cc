// Actual immutable exception values and native C++ propagation. The optional
// exclusive sweep overlay is a prototype, not a product collector or guest VM.
#include <uwvm2/runtime/exception/roots.h>
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <span>
#include <vector>
#include <fast_io.h>

namespace eh = ::uwvm2::runtime::exception;
namespace gc = ::uwvm2::uwvm::runtime::storage;
namespace types = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace global = ::uwvm2::object::global;
static ::std::size_t checks{}, actual_collections{}, actual_reclaimed{}, unwind_collections{};

static void check(bool condition, char const* label) noexcept
{
    ++checks;
    if(!condition)
    {
        ::fast_io::io::perrln("exception payload roots failed: ", ::fast_io::mnp::os_c_str(label));
        ::fast_io::fast_terminate();
    }
}

static types::recursive_type_section declarations()
{
    types::recursive_type_section section{};
    types::recursive_group group{};
    for(auto kind : {types::composite_kind::struct_, types::composite_kind::array})
    {
        types::sub_type type{};
        type.kind = kind;
        types::field_type field{};
        field.storage.value.kind = types::value_kind::i32;
        field.mutable_ = true;
        type.fields.push_back(field);
        group.types.push_back(::std::move(type));
    }
    section.groups.push_back(::std::move(group));
    section.type_count = 2u;
    return section;
}

static gc::gc_reference make_struct(gc::gc_object_store& store, ::std::uint32_t key) noexcept
{
    auto const input{gc::gc_object_value::i32(key)};
    gc::gc_reference result{};
    check(store.struct_new(0u, ::std::addressof(input), 1uz, result) == gc::gc_object_status::ok, "real struct allocation");
    return result;
}

static eh::payload_field wasm_field(gc::gc_reference reference)
{
    auto field{eh::payload_field::wasm_reference(::std::as_bytes(::std::span{::std::addressof(reference), 1uz}))};
    check(field.has_value(), "complete owned Wasm carrier");
    return ::std::move(*field);
}

struct roots
{
    ::std::array<gc::gc_reference, 8uz> values{};
    ::std::size_t size{};
    bool operator()(gc::gc_reference reference) noexcept
    {
        check(size < values.size(), "native root buffer capacity");
        // [0,size) published values][one free slot] ... buffer end
        // [safe                   ] copy the complete carrier before advancing.
        values[size++] = reference;
        return true;
    }
};

static void inspect_and_collect(gc::gc_object_store& store, eh::value const& instance,
                                ::std::size_t expected_reclaimed, bool unwinding) noexcept
{
    roots snapshot{};
    auto const result{eh::visit_immutable_exception_wasm_roots(instance, snapshot)};
    check(result.status == eh::payload_root_status::ok && result.visited == 4uz && snapshot.size == 4uz,
          "only four typed complete carriers enumerated");
#if defined(UWVM_EXCEPTION_ROOT_SWEEP_PROBE)
    ::std::array stores{store.shared_from_this()};
    ::std::size_t reclaimed{};
    check(gc::gc_object_store::collect_exclusive_aggregate_domain(stores.data(), stores.size(),
          snapshot.values.data(), snapshot.size, reclaimed) == gc::gc_object_status::ok, "exclusive complete-domain collection");
    check(reclaimed == expected_reclaimed, "exact per-collection reclamation count");
    ++actual_collections;
    actual_reclaimed += reclaimed;
    if(unwinding)
    {
        check(::std::uncaught_exceptions() != 0, "collection while native exception actually unwinds");
        ++unwind_collections;
    }
#else
    static_cast<void>(expected_reclaimed);
    static_cast<void>(unwinding);
#endif
    gc::gc_object_value observed{};
    check(store.struct_get(snapshot.values[0], 0u, false, observed) == gc::gc_object_status::ok &&
          observed.as<::std::uint32_t>() == 37u, "payload-only struct identity and field survive");
    ::std::size_t length{};
    check(store.array_length(snapshot.values[1], length) == gc::gc_object_status::ok && length == 7uz,
          "payload-only array survives");
    check(store.array_get(snapshot.values[1], 6uz, false, observed) == gc::gc_object_status::ok &&
          observed.as<::std::uint32_t>() == 41u, "payload array data survives");
    check(snapshot.values[2].kind == global::wasm_ref_kind::wasm_i31 &&
          snapshot.values[2].storage.wasm_i31.get_s() == -19, "complete signed i31 retained");
    check(snapshot.values[3].kind == global::wasm_ref_kind::wasm_null, "complete null retained");
}

struct native_unwind_probe
{
    gc::gc_object_store& store;
    eh::value_ref instance;
    ~native_unwind_probe()
    {
        // This explicit native owner makes the fixture safe even if the C++
        // ABI fails to allocate an exception activation. It does not claim
        // automatic discovery of a guest's in-flight exception root.
        if(::std::uncaught_exceptions() == 0) { return; }
        static_cast<void>(make_struct(store, 999u));
        inspect_and_collect(store, *instance, 1uz, true);
    }
};

#if defined(__GNUC__) || defined(__clang__)
[[gnu::noinline]]
#endif
static void throw_nested(gc::gc_object_store& store, eh::value_ref instance, unsigned depth)
{
    native_unwind_probe guard{store, instance};
    if(depth != 0u) { throw_nested(store, instance, depth - 1u); }
    else { eh::throw_value(::std::move(instance)); }
}

int main()
{
    auto const layout{declarations()};
    auto leases{::std::make_shared<gc::gc_lease_owner>()};
    auto store{::std::make_shared<gc::gc_object_store>(layout, leases)};
    check(store->valid(), "actual store types");
    auto const structure{make_struct(*store, 37u)};
    auto const numeric_token{make_struct(*store, 888u)};
    gc::gc_reference array{};
    check(store->array_new(1u, gc::gc_object_value::i32(41u), 7uz, array) == gc::gc_object_status::ok, "real array allocation");
    auto const tag{::std::static_pointer_cast<void const>(::std::make_shared<unsigned char>())};
    ::std::vector<eh::payload_field> fields{};
    fields.push_back(wasm_field(structure));
    fields.push_back(wasm_field(array));
    fields.push_back(wasm_field(global::make_wasm_i31_reference(-19)));
    fields.push_back(wasm_field({}));
    ::std::array<::std::byte, 16uz> numeric_bits{};
    ::std::memcpy(numeric_bits.data(), ::std::addressof(numeric_token), sizeof(numeric_token));
    auto const numeric{eh::payload_field::numeric(eh::payload_kind::v128, numeric_bits)};
    check(numeric.has_value(), "numeric carrier-shaped vector");
    fields.push_back(*numeric);
    fields.push_back(eh::payload_field::null_reference());
    auto instance{eh::value::make_owned(tag, ::std::move(fields))};
    check(static_cast<bool>(instance), "actual immutable owning exception value");
    inspect_and_collect(*store, *instance, 1uz, false);
#if defined(UWVM_EXCEPTION_ROOT_SWEEP_PROBE)
    gc::gc_object_value observed{};
    check(store->struct_get(numeric_token, 0u, false, observed) == gc::gc_object_status::invalid_reference,
          "numeric token-shaped bytes do not retain object");
#endif
    eh::value_ref caught{};
    try
    {
        try { throw_nested(*store, instance, 2u); }
        catch(eh::guest_exception const& exception)
        {
            check(exception.instance().get() == instance.get(), "typed catch keeps exact exception identity");
            static_cast<void>(make_struct(*store, 1001u));
            inspect_and_collect(*store, *exception.instance(), 1uz, false);
            throw;
        }
    }
    catch(eh::guest_exception const& exception)
    {
        caught = exception.instance();
        check(caught.get() == instance.get(), "native rethrow preserves immutable identity");
    }
    check(caught && caught->fields()[4].bits().size() == numeric_bits.size() &&
          ::std::memcmp(caught->fields()[4].bits().data(), numeric_bits.data(), numeric_bits.size()) == 0,
          "numeric vector bits unchanged after catch and rethrow");

    // These intentionally invalid native values do not coexist with a sweep.
    // Retire every value owner containing a live aggregate before the empty
    // root-set collection below, including the rejected mixed/host payloads.
    {
    gc::gc_reference forged{};
    forged.kind = global::wasm_ref_kind::wasm_struct;
    forged.storage.ptr = reinterpret_cast<void*>(::std::uintptr_t{0x123u});
    ::std::array forged_fields{wasm_field(forged)};
    auto const forged_instance{eh::value::make(tag, forged_fields)};
    auto const checked_struct{[&](gc::gc_reference reference) noexcept
    {
        gc::gc_object_value result{};
        return store->struct_get(reference, 0u, false, result) == gc::gc_object_status::ok;
    }};
    check(eh::visit_immutable_exception_wasm_roots(*forged_instance, checked_struct).status ==
          eh::payload_root_status::rejected_reference, "forged legal-kind token rejected by real store");
    forged.kind = static_cast<global::wasm_ref_kind>(255u);
    ::std::array invalid_fields{wasm_field(structure), wasm_field(forged)};
    auto const invalid_instance{eh::value::make(tag, invalid_fields)};
    roots invalid_roots{};
    auto const invalid_result{eh::visit_immutable_exception_wasm_roots(*invalid_instance, invalid_roots)};
    check(invalid_result.status == eh::payload_root_status::invalid_payload && invalid_roots.size == 0uz,
          "entire payload validated before any roots published");
    forged.kind = global::wasm_ref_kind::wasm_func;
    ::std::array parser_fields{wasm_field(forged)};
    auto const parser_instance{eh::value::make(tag, parser_fields)};
    check(eh::visit_immutable_exception_wasm_roots(*parser_instance, invalid_roots).status ==
          eh::payload_root_status::invalid_payload, "parser-only function index rejected");
    auto const host_owner{::std::static_pointer_cast<void const>(::std::make_shared<unsigned char>())};
    auto const host_field{eh::payload_field::rooted_reference(host_owner)};
    check(host_field.has_value(), "canonical native rooted host identity");
    ::std::array host_fields{wasm_field(structure), *host_field};
    auto const host_instance{eh::value::make(tag, host_fields)};
    check(eh::visit_immutable_exception_wasm_roots(*host_instance, invalid_roots).status ==
          eh::payload_root_status::host_codec_required && invalid_roots.size == 0uz,
          "untyped native host handle cannot authorize collection");
    ::std::array encoded_fields{eh::payload_field::unboxed_i31(19u)};
    auto const encoded_instance{eh::value::make(tag, encoded_fields)};
    check(eh::visit_immutable_exception_wasm_roots(*encoded_instance, invalid_roots).status ==
          eh::payload_root_status::host_codec_required, "opaque i31 encoding requires actual host codec");
    }

#if defined(UWVM_EXCEPTION_ROOT_SWEEP_PROBE)
    // None of these native carrier copies is a registered implicit root. End
    // every explicit exception owner before collecting an empty root set.
    instance.reset();
    caught.reset();
    ::std::array stores{store};
    ::std::size_t reclaimed{};
    check(gc::gc_object_store::collect_exclusive_aggregate_domain(stores.data(), stores.size(), nullptr, 0uz, reclaimed) ==
          gc::gc_object_status::ok && reclaimed == 2uz, "retired payload roots reclaim both aggregates");
    ++actual_collections;
    actual_reclaimed += reclaimed;
    check(actual_collections == 6uz && actual_reclaimed == 7uz && unwind_collections == 3uz,
          "actual collection counts including three true native unwind cleanups");
    ::std::array stale_fields{wasm_field(structure)};
    auto const stale_instance{eh::value::make(tag, stale_fields)};
    auto const reject_stale{[&](gc::gc_reference reference) noexcept
    {
        gc::gc_object_value result{};
        return store->struct_get(reference, 0u, false, result) == gc::gc_object_status::ok;
    }};
    check(eh::visit_immutable_exception_wasm_roots(*stale_instance, reject_stale).status ==
          eh::payload_root_status::rejected_reference, "retired valid token cannot commit a later collection");
#endif
    // Actual exn token tested AFTER aggregate-only sweeps. That prototype
    // deliberately rejects a live process exn registry instead of dropping it.
    auto const empty_value{eh::value::make(tag, {})};
    gc::gc_reference exception_ref{};
    check(store->make_exn_reference(empty_value, exception_ref) == gc::gc_object_status::ok, "real exception registry token");
    unsigned char host_payload{};
    gc::gc_reference host_reference{};
    host_reference.kind = global::wasm_ref_kind::wasm_extern;
    host_reference.storage.ptr = ::std::addressof(host_payload);
    ::std::array other_fields{wasm_field(exception_ref), wasm_field(host_reference)};
    auto const other_instance{eh::value::make(tag, other_fields)};
    roots other_roots{};
    check(eh::visit_immutable_exception_wasm_roots(*other_instance, other_roots).status == eh::payload_root_status::ok &&
          other_roots.size == 2uz && other_roots.values[0].kind == global::wasm_ref_kind::wasm_exn &&
          other_roots.values[0].storage.ptr == exception_ref.storage.ptr &&
          other_roots.values[1].kind == global::wasm_ref_kind::wasm_extern &&
          other_roots.values[1].storage.ptr == ::std::addressof(host_payload), "typed exn/opaque extern carrier identities preserved");
#if defined(UWVM_EXCEPTION_ROOT_SWEEP_PROBE)
    ::fast_io::io::println("exception payload roots PASS checks=", checks,
                          " actual_collections=6 reclaimed=7 unwind_collections=3");
#else
    ::fast_io::io::println("exception payload roots PASS checks=", checks, " collector_activation=absent");
#endif
}
