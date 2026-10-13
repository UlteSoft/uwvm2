// Owning Core 3 exception values and independent native activations, not a CLI EH coverage claim.
#include <uwvm2/runtime/exception/impl.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <barrier>
#include <bit>
#include <cstring>
#include <exception>
#include <span>
#include <thread>
#include <type_traits>
#include <vector>

namespace exn = uwvm2::runtime::exception;
namespace
{
    std::atomic<unsigned> checks{};
#define CHECK(condition) do { ++checks; if(!(condition)) { ::fast_io::io::perrln("FAIL ", __LINE__, ": ", #condition); ::fast_io::fast_terminate(); } } while(false)

    struct tracked
    {
        std::atomic<unsigned>& live;
        explicit tracked(std::atomic<unsigned>& counter) : live{counter} { ++live; }
        ~tracked() { --live; }
    };

    exn::payload_field integer(std::uint64_t bits)
    {
        auto bytes = std::bit_cast<std::array<std::byte, 8>>(bits);
        auto field = exn::payload_field::numeric(exn::payload_kind::i64, bytes);
        CHECK(field.has_value());
        return *field;
    }

    void representation()
    {
        std::array<std::byte, 17> bytes{};
        for(std::size_t i{}; i != bytes.size(); ++i) { bytes[i] = static_cast<std::byte>(i * 19u + 3u); }
        for(auto kind : {exn::payload_kind::i32, exn::payload_kind::i64, exn::payload_kind::f32,
                         exn::payload_kind::f64, exn::payload_kind::v128})
        {
            auto width = exn::payload_width(kind);
            for(std::size_t size{}; size != bytes.size(); ++size)
            {
                // Intentionally unaligned source, with a separately bounded 0..16 byte range.
                auto field = exn::payload_field::numeric(kind, {bytes.data() + 1, size});
                CHECK(field.has_value() == (size == width));
                if(field) { CHECK(std::memcmp(field->bits().data(), bytes.data() + 1, width) == 0); }
            }
        }
        CHECK(!exn::payload_field::numeric(exn::payload_kind::reference, {}));
        CHECK(!exn::payload_field::numeric(static_cast<exn::payload_kind>(255), {}));
        for(auto bits : {0x7f800001u, 0xff800123u, 0x80000000u, 0xffffffffu})
        {
            auto raw = std::bit_cast<std::array<std::byte, 4>>(bits);
            auto field = exn::payload_field::numeric(exn::payload_kind::f32, raw);
            CHECK(field && std::memcmp(field->bits().data(), raw.data(), raw.size()) == 0);
        }
        for(auto bits : {0x7ff0000000000001ull, 0xfff0123456789abcull, 0x8000000000000000ull})
        {
            auto raw = std::bit_cast<std::array<std::byte, 8>>(bits);
            auto field = exn::payload_field::numeric(exn::payload_kind::f64, raw);
            CHECK(field && std::memcmp(field->bits().data(), raw.data(), raw.size()) == 0);
        }
        void const* null{};
        auto field = exn::payload_field::null_reference();
        CHECK(!field.root());
        CHECK(std::memcmp(field.bits().data(), &null, sizeof(null)) == 0);
    }

    void roots_and_snapshot()
    {
        std::atomic<unsigned> live{};
        auto tag = std::make_shared<tracked>(live);
        auto referent = std::make_shared<tracked>(live);
        auto field = exn::payload_field::rooted_reference(referent);
        CHECK(field && live == 2);
        std::vector<exn::payload_field> fields(160, integer(0x8000000000000001ull));
        fields[31] = *field;
        auto instance = exn::value::make(tag, fields);
        auto tag_identity = tag.get();
        auto reference_identity = referent.get();
        fields.clear();
        field.reset();
        tag.reset();
        referent.reset();
        CHECK(live == 2);
        CHECK(instance->tag_identity() == tag_identity);
        CHECK(instance->fields().size() == 160);
        CHECK(instance->fields()[31].root().get() == reference_identity);
        for(std::size_t i{}; i != instance->fields().size(); ++i)
        {
            if(i == 31) { continue; }
            std::uint64_t bits{};
            std::memcpy(&bits, instance->fields()[i].bits().data(), sizeof(bits));
            CHECK(bits == 0x8000000000000001ull);
        }
        auto retained = instance;
        instance.reset();
        CHECK(live == 2);
        retained.reset();
        CHECK(live == 0);
        CHECK(!exn::value::make({}, {}));
        CHECK(!exn::payload_field::rooted_reference({}));
        int borrowed{};
        exn::instance_root ownerless{exn::instance_root{}, &borrowed};
        CHECK(ownerless && ownerless.use_count() == 0);
        CHECK(!exn::value::make(ownerless, {}));
        CHECK(!exn::payload_field::rooted_reference(ownerless));
    }

    void owned_builder_and_native_retention()
    {
        std::atomic<unsigned> live{};
        auto tag = std::make_shared<tracked>(live);
        auto referent = std::make_shared<tracked>(live);
        auto const tag_identity = tag.get();
        auto const reference_identity = referent.get();
        std::weak_ptr<tracked> tag_weak = tag, referent_weak = referent;
        auto const i32_bits = std::bit_cast<std::array<std::byte, 4>>(std::uint32_t{0x80000001u});
        auto const i64_bits = std::bit_cast<std::array<std::byte, 8>>(std::uint64_t{0xfedcba9876543210ull});
        auto const f32_bits = std::bit_cast<std::array<std::byte, 4>>(std::uint32_t{0xff800123u});
        auto const f64_bits = std::bit_cast<std::array<std::byte, 8>>(std::uint64_t{0x7ff0000000000001ull});
        std::array<std::byte, 17> vector_bits{};
        for(std::size_t i{}; i != vector_bits.size(); ++i) { vector_bits[i] = static_cast<std::byte>(i * 23u + 7u); }
        std::vector<exn::payload_field> fields{};
        auto const append_numeric = [&](exn::payload_kind kind, std::span<std::byte const> bits)
        {
            auto field = exn::payload_field::numeric(kind, bits);
            CHECK(field);
            fields.push_back(std::move(*field));
        };
        append_numeric(exn::payload_kind::i32, i32_bits);
        append_numeric(exn::payload_kind::i64, i64_bits);
        append_numeric(exn::payload_kind::f32, f32_bits);
        append_numeric(exn::payload_kind::f64, f64_bits);
        // [owned vector_bits: 17 bytes] end
        // [safe                    ] the unaligned data()+1 view contains exactly 16 bytes.
        append_numeric(exn::payload_kind::v128, {vector_bits.data() + 1, 16});
        auto rooted = exn::payload_field::rooted_reference(referent);
        CHECK(rooted);
        fields.push_back(std::move(*rooted));
        rooted.reset();
        for(std::uint64_t i{}; i != 160; ++i) { fields.push_back(integer(0xabcdef0123456789ull ^ i)); }
        std::vector<exn::diagnostic_frame> frames{};
        frames.push_back({.module_id = 7, .function_index = 11,
                          .module_name = u8"owned payload module", .function_name = u8"original throw"});
        auto trace = exn::diagnostic_trace::make(std::move(frames));
        auto const trace_identity = trace.get();
        std::weak_ptr<exn::diagnostic_trace const> trace_weak = trace;
        // No iterator, span, pointer or mutable element alias into fields survives publication.
        auto instance = exn::value::make_owned(tag, std::move(fields), std::move(trace));
        CHECK(instance && !trace);
        auto const value_identity = instance.get();
        // A moved-from builder is reusable; these writes must not alter the owned exception.
        fields.clear();
        fields.push_back(integer(0));
        auto const inspect = [&](exn::value_ref const& value)
        {
            CHECK(value && value.get() == value_identity && value->tag_identity() == tag_identity);
            auto const payload = value->fields();
            CHECK(payload.size() == 166);
            CHECK(payload[0].kind() == exn::payload_kind::i32 &&
                  std::memcmp(payload[0].bits().data(), i32_bits.data(), i32_bits.size()) == 0);
            CHECK(payload[1].kind() == exn::payload_kind::i64 &&
                  std::memcmp(payload[1].bits().data(), i64_bits.data(), i64_bits.size()) == 0);
            CHECK(payload[2].kind() == exn::payload_kind::f32 &&
                  std::memcmp(payload[2].bits().data(), f32_bits.data(), f32_bits.size()) == 0);
            CHECK(payload[3].kind() == exn::payload_kind::f64 &&
                  std::memcmp(payload[3].bits().data(), f64_bits.data(), f64_bits.size()) == 0);
            CHECK(payload[4].kind() == exn::payload_kind::v128 &&
                  std::memcmp(payload[4].bits().data(), vector_bits.data() + 1, 16) == 0);
            CHECK(payload[5].root().get() == reference_identity);
            for(std::uint64_t i{}; i != 160; ++i)
            {
                std::uint64_t bits{};
                std::memcpy(&bits, payload[6 + i].bits().data(), sizeof(bits));
                CHECK(bits == (0xabcdef0123456789ull ^ i));
            }
            CHECK(value->diagnostic().get() == trace_identity && !value->diagnostic()->truncated());
            CHECK(value->diagnostic()->frames().size() == 1);
            auto const& frame = value->diagnostic()->frames()[0];
            CHECK(frame.module_id == 7 && frame.function_index == 11 &&
                  frame.module_name == u8"owned payload module" && frame.function_name == u8"original throw");
        };
        inspect(instance);
        std::exception_ptr native{};
        try { exn::throw_value(instance); }
        catch(exn::guest_exception const& outer)
        {
            inspect(outer.instance());
            native = std::current_exception();
            exn::guest_exception source{outer.instance()};
            auto moved = std::move(source);
            inspect(source.instance());
            inspect(moved.instance());
            tag.reset();
            referent.reset();
            instance.reset();
            CHECK(live == 2 && !tag_weak.expired() && !referent_weak.expired() && !trace_weak.expired());
            try { throw; }
            catch(exn::guest_exception const& rethrown) { inspect(rethrown.instance()); }
            try { exn::throw_value(moved.instance()); }
            catch(exn::guest_exception const& fresh)
            {
                CHECK(&fresh != &outer);
                inspect(fresh.instance());
            }
        }
        CHECK(live == 2);
        try { std::rethrow_exception(native); }
        catch(exn::guest_exception const& held) { inspect(held.instance()); }
        native = {};
        CHECK(live == 0 && tag_weak.expired() && referent_weak.expired() && trace_weak.expired());
    }

    void owned_builder_invalid_tags_and_empty_tuple()
    {
        std::vector<exn::payload_field> fields{integer(0x76543210abcdef98ull)};
        CHECK(!exn::value::make_owned({}, std::move(fields)));
        CHECK(fields.size() == 1);
        int borrowed{};
        exn::instance_root ownerless{exn::instance_root{}, &borrowed};
        CHECK(ownerless && ownerless.use_count() == 0);
        CHECK(!exn::value::make_owned(ownerless, std::move(fields)));
        CHECK(fields.size() == 1);
        std::uint64_t bits{};
        std::memcpy(&bits, fields[0].bits().data(), sizeof(bits));
        CHECK(bits == 0x76543210abcdef98ull);
        auto tag = std::make_shared<int>(42);
        auto empty = exn::value::make_owned(tag, {});
        CHECK(empty && empty->tag_identity() == tag.get() && empty->fields().empty() && !empty->diagnostic());
    }

    void moved_fields_and_source_reuse()
    {
        std::atomic<unsigned> live{};
        auto owner = std::make_shared<tracked>(live);
        std::weak_ptr<tracked> weak = owner;
        auto source = *exn::payload_field::rooted_reference(owner);
        auto target = std::move(source);
        void const* null{};
        CHECK(!source.root());
        CHECK(source.kind() == exn::payload_kind::reference);
        CHECK(std::memcmp(source.bits().data(), &null, sizeof(null)) == 0);
        CHECK(target.root().get() == owner.get());
        auto tag = std::make_shared<int>(42);
        std::array reused_fields{source};
        auto reused = exn::value::make(tag, reused_fields);
        CHECK(reused && !reused->fields()[0].root());
        CHECK(std::memcmp(reused->fields()[0].bits().data(), &null, sizeof(null)) == 0);
        owner.reset();
        CHECK(!weak.expired());
        auto self = std::addressof(target);
        target = std::move(*self);
        CHECK(target.root() && !weak.expired());
        target = exn::payload_field::null_reference();
        CHECK(weak.expired() && live == 0);
        CHECK(std::memcmp(reused->fields()[0].bits().data(), &null, sizeof(null)) == 0);

        auto first = std::make_shared<tracked>(live);
        auto second = std::make_shared<tracked>(live);
        std::weak_ptr<tracked> old_destination = second;
        source = *exn::payload_field::rooted_reference(first);
        target = *exn::payload_field::rooted_reference(second);
        second.reset();
        CHECK(live == 2);
        target = std::move(source);
        CHECK(old_destination.expired() && live == 1);
        CHECK(target.root().get() == first.get());
        CHECK(!source.root() && std::memcmp(source.bits().data(), &null, sizeof(null)) == 0);
        reused_fields[0] = source;
        reused = exn::value::make(tag, reused_fields);
        first.reset();
        target = exn::payload_field::null_reference();
        CHECK(live == 0);
        CHECK(!reused->fields()[0].root());
        CHECK(std::memcmp(reused->fields()[0].bits().data(), &null, sizeof(null)) == 0);

        // These are host-supplied carrier patterns, not a claim about a finished
        // Wasm i31/reference encoder or collector integration.
        for(std::uintptr_t bits : {std::uintptr_t{1}, std::uintptr_t{0x80000003u}})
        {
            auto unboxed = exn::payload_field::unboxed_i31(bits);
            CHECK(!unboxed.root());
            CHECK(std::memcmp(unboxed.bits().data(), &bits, sizeof(bits)) == 0);
            auto moved = std::move(unboxed);
            CHECK(!moved.root());
            CHECK(std::memcmp(moved.bits().data(), &bits, sizeof(bits)) == 0);
            CHECK(std::memcmp(unboxed.bits().data(), &null, sizeof(null)) == 0);
        }
    }

    void activation_moves_and_native_roots()
    {
        std::atomic<unsigned> live{};
        auto tag = std::make_shared<tracked>(live);
        auto referent = std::make_shared<tracked>(live);
        std::array fields{*exn::payload_field::rooted_reference(referent)};
        auto instance = exn::value::make(tag, fields);
        std::exception_ptr retained_native{};
        {
            exn::guest_exception source{instance};
            auto target = std::move(source);
            CHECK(source.instance().get() == instance.get());
            CHECK(target.instance().get() == instance.get());
            exn::guest_exception assigned{exn::value::make(std::make_shared<int>(7), {})};
            assigned = std::move(source);
            CHECK(source.instance().get() == instance.get());
            CHECK(assigned.instance().get() == instance.get());
            auto self = std::addressof(assigned);
            assigned = std::move(*self);
            CHECK(assigned.instance().get() == instance.get());
            try { throw std::move(source); }
            catch(exn::guest_exception const& active)
            {
                CHECK(source.instance().get() == instance.get());
                CHECK(active.instance().get() == instance.get());
                retained_native = std::current_exception();
            }
        }
        fields[0] = exn::payload_field::null_reference();
        instance.reset();
        tag.reset();
        referent.reset();
        CHECK(live == 2);
        try { std::rethrow_exception(retained_native); }
        catch(exn::guest_exception const& active)
        {
            CHECK(active.instance()->tag_identity() != nullptr);
            CHECK(active.instance()->fields()[0].root());
        }
        retained_native = {};
        CHECK(live == 0);
    }

    void retained_rethrow()
    {
        auto tag = std::make_shared<int>(17);
        auto different_tag = std::make_shared<int>(17);
        std::array fields{integer(0x9876543210abcdefull)};
        auto first = exn::value::make(tag, fields);
        auto alias = exn::value::make(exn::instance_root{tag, tag.get()}, fields);
        auto distinct = exn::value::make(different_tag, fields);
        CHECK(first.get() != alias.get());
        CHECK(first->tag_identity() == alias->tag_identity());
        CHECK(first->tag_identity() != distinct->tag_identity());
        exn::value_ref captured{};
        std::exception_ptr native{};
        try { exn::throw_value(first); }
        catch(exn::guest_exception const& outer)
        {
            captured = outer.instance();
            native = std::current_exception();
            for(unsigned i{}; i != 64; ++i)
            {
                // Throw the SAME immutable Wasm instance while its previous native activation is caught.
                try { exn::throw_value(captured); }
                catch(exn::guest_exception const& inner)
                {
                    CHECK(&inner != &outer);
                    CHECK(inner.instance().get() == outer.instance().get());
                    try { exn::throw_value(distinct); }
                    catch(exn::guest_exception const& third)
                    {
                        CHECK(&third != &inner && &third != &outer);
                        CHECK(third.instance().get() == distinct.get());
                        CHECK(inner.instance().get() == first.get());
                    }
                }
            }
        }
        CHECK(captured.get() == first.get());
        first.reset();
        try { std::rethrow_exception(native); }
        catch(exn::guest_exception const& held) { CHECK(held.instance().get() == captured.get()); }
        native = {};
        try { exn::throw_value(captured); }
        catch(exn::guest_exception const& later) { CHECK(later.instance().get() == captured.get()); }
        // A foreign/host exception must not be swallowed by the guest exception catch type.
        bool foreign{};
        try { throw 42; }
        catch(exn::guest_exception const&) { CHECK(false); }
        catch(int n) { foreign = n == 42; }
        CHECK(foreign);
    }

    void parallel_activations()
    {
        auto instance = exn::value::make(std::make_shared<int>(42), {});
        std::barrier simultaneous{4};
        std::array<exn::guest_exception const*, 4> active{};
        std::vector<std::jthread> threads{};
        for(unsigned t{}; t != 4; ++t)
        {
            threads.emplace_back([&, t, instance]
            {
                for(unsigned iteration{}; iteration != 32; ++iteration)
                {
                    try { exn::throw_value(instance); }
                    catch(exn::guest_exception const& current)
                    {
                        active[t] = &current;
                        simultaneous.arrive_and_wait();
                        for(unsigned other{}; other != 4; ++other)
                        {
                            CHECK(active[other]->instance().get() == instance.get());
                            if(other != t) { CHECK(active[other] != &current); }
                        }
                        simultaneous.arrive_and_wait();
                        // No thread dereferences these borrowed activation pointers after this barrier.
                    }
                }
            });
        }
    }
}

int main()
{
    static_assert(std::is_final_v<exn::guest_exception>);
    static_assert(std::is_nothrow_copy_constructible_v<exn::guest_exception>);
    static_assert(std::is_nothrow_destructible_v<exn::guest_exception>);
    representation();
    roots_and_snapshot();
    owned_builder_and_native_retention();
    owned_builder_invalid_tags_and_empty_tuple();
    moved_fields_and_source_reuse();
    activation_moves_and_native_roots();
    retained_rethrow();
    parallel_activations();
    ::fast_io::io::println("PASS exception value ownership: ", checks.load(), " checks");
}
