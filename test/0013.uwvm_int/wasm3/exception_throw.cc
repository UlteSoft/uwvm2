// Actual numeric throw opfuncs and owning tuples. Translator/CLI emission is a separate next step.
#include <uwvm2/runtime/compiler/uwvm_int/optable/exception_throw.h>
#include <array>
#include <atomic>
#include <barrier>
#include <cfenv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <thread>
#include <type_traits>
#include <vector>

namespace op = uwvm2::runtime::compiler::uwvm_int::optable;
namespace exn = uwvm2::runtime::exception;
namespace
{
    std::atomic<unsigned> checks{};
#define CHECK(condition) do { ++checks; if(!(condition)) { std::fprintf(stderr, "FAIL %u: %s\n", __LINE__, #condition); std::abort(); } } while(false)
    using kind = exn::payload_kind;
    using slot = op::wasm_stack_top_i32_i64_f32_f64_u;

    struct tuple_fixture
    {
        std::vector<kind> kinds{};
        std::vector<std::byte> bytes{std::byte{0xc7}}; // byte zero is an unrelated caller prefix.
        explicit tuple_fixture(unsigned repetitions)
        {
            for(unsigned i{}; i != repetitions; ++i)
            {
                append(kind::i32, std::uint32_t{0x80000001u + i});
                append(kind::i64, std::uint64_t{0x8000000000000001ULL + i});
                append(kind::f32, std::uint32_t{0x7f800001u + i}); // signaling NaN remains raw bits.
                append(kind::f64, std::uint64_t{0xfff0000000000001ULL + i});
                std::array<std::byte, 16> wide{};
                for(unsigned n{}; n != wide.size(); ++n) { wide[n] = static_cast<std::byte>(19u * n + i); }
                append(kind::v128, wide);
            }
        }
        template<class T> void append(kind k, T const& value)
        {
            kinds.push_back(k);
            auto old = bytes.size();
            bytes.resize(old + sizeof(value));
            std::memcpy(bytes.data() + old, &value, sizeof(value));
        }
        std::span<std::byte const> arguments() const noexcept { return {bytes.data() + 1, bytes.size() - 1}; }
    };

    void verify(exn::value const& value, op::exception_throw_site const& site, tuple_fixture const& expected)
    {
        CHECK(value.tag_identity() == site.tag_identity().get());
        CHECK(value.fields().size() == expected.kinds.size());
        std::size_t offset{1};
        for(std::size_t i{}; i != expected.kinds.size(); ++i)
        {
            auto const& field = value.fields()[i];
            auto width = exn::payload_width(expected.kinds[i]);
            CHECK(field.kind() == expected.kinds[i]);
            CHECK(!field.root() && field.bits().size() == width);
            CHECK(std::memcmp(field.bits().data(), expected.bytes.data() + offset, width) == 0);
            offset += width;
        }
        CHECK(offset == expected.bytes.size());
    }

    void construction_and_ownership()
    {
        auto tag = std::make_shared<int>(59);
        CHECK(!op::exception_throw_site::make_numeric({}, {}));
        int borrowed{};
        exn::instance_root ownerless{exn::instance_root{}, &borrowed};
        CHECK(!op::exception_throw_site::make_numeric(ownerless, {}));
        for(auto invalid : {kind::reference, static_cast<kind>(255)})
        {
            std::array signature{kind::i32, invalid};
            CHECK(!op::exception_throw_site::make_numeric(tag, signature));
        }
        auto empty = op::exception_throw_site::make_numeric(tag, {});
        CHECK(empty && empty->argument_bytes() == 0 && empty->parameter_kinds().empty());
        tuple_fixture original{17};
        auto site = op::exception_throw_site::make_numeric(tag, original.kinds);
        CHECK(site && site->argument_bytes() == original.arguments().size());
        CHECK(site->parameter_kinds().size() == 85);
        auto input = original;
        auto signature = original.kinds;
        auto copied = op::exception_throw_site::make_numeric(tag, signature);
        signature.clear(); // the descriptor does not borrow a compiler's temporary signature.
        CHECK(copied->parameter_kinds().size() == original.kinds.size());
        std::exception_ptr previous;
        exn::guest_exception const* previous_activation{};
        exn::value_ref retained{};
        std::feclearexcept(FE_ALL_EXCEPT);
        try { op::raise_numeric_tuple(*site, input.arguments()); }
        catch(exn::guest_exception const& caught)
        {
            CHECK((std::fetestexcept(FE_INVALID) & FE_INVALID) == 0);
            previous = std::current_exception();
            previous_activation = &caught;
            retained = caught.instance();
            verify(*retained, *site, original);
        }
        CHECK(previous && retained);
        std::memset(input.bytes.data() + 1, 0, input.bytes.size() - 1);
        verify(*retained, *site, original); // every payload bit was copied before leaving its source frame.
        try { op::raise_numeric_tuple(*site, original.arguments()); }
        catch(exn::guest_exception const& caught)
        {
            CHECK(&caught != previous_activation && caught.instance().get() != retained.get());
            verify(*caught.instance(), *site, original);
            try { std::rethrow_exception(previous); }
            catch(exn::guest_exception const& old)
            { CHECK(&old == previous_activation && old.instance().get() == retained.get()); }
        }
        std::weak_ptr<int> weak = tag;
        tag.reset(); site.reset(); copied.reset(); empty.reset();
        CHECK(!weak.expired()); // retained exception owns the tag after all compiled sites retire.
        previous = {};
        CHECK(!weak.expired());
        retained.reset();
        CHECK(weak.expired());
    }

    template<unsigned Slots, bool Tail>
    consteval op::uwvm_interpreter_translate_option_t option()
    {
        op::uwvm_interpreter_translate_option_t result{.is_tail_call = Tail};
        if constexpr(Slots != 0)
        {
            static_assert(Tail);
            result.i32_stack_top_begin_pos = result.i64_stack_top_begin_pos =
                result.f32_stack_top_begin_pos = result.f64_stack_top_begin_pos = 3;
            result.i32_stack_top_end_pos = result.i64_stack_top_end_pos =
                result.f32_stack_top_end_pos = result.f64_stack_top_end_pos = 3 + Slots;
        }
        return result;
    }

    template<bool Tail, class... Cached>
    void execute(unsigned repetitions, Cached... cached)
    {
        tuple_fixture input{repetitions};
        auto tag = std::make_shared<int>(61);
        auto site = op::exception_throw_site::make_numeric(tag, input.kinds);
        constexpr auto settings = option<sizeof...(Cached), Tail>();
        auto entry = op::translate::get_uwvmint_throw_numeric_fptr_from_tuple<settings>(
            uwvm2::utils::container::tuple<std::byte const*, std::byte*, std::byte*, Cached...>{});
        std::array<std::byte, 1 + sizeof(entry) + sizeof(site.get())> code{};
        auto initial_ip = code.data() + 1;
        std::memcpy(initial_ip, &entry, sizeof(entry));
        auto site_pointer = site.get();
        std::memcpy(initial_ip + sizeof(entry), &site_pointer, sizeof(site_pointer));
        std::byte const* ip = initial_ip;
        auto top = input.bytes.data() + input.bytes.size();
        std::array<std::byte, 9> locals{};
        auto local_base = locals.data() + 1;
        auto expected_top = top;
        decltype(entry) loaded{};
        std::memcpy(&loaded, ip, sizeof(loaded));
        bool caught_exception{};
        std::feclearexcept(FE_ALL_EXCEPT);
        try { loaded(ip, top, local_base, cached...); CHECK(false); }
        catch(exn::guest_exception const& caught)
        {
            caught_exception = true;
            CHECK((std::fetestexcept(FE_INVALID) & FE_INVALID) == 0);
            CHECK(ip == initial_ip && top == expected_top && local_base == locals.data() + 1);
            CHECK(input.bytes[0] == std::byte{0xc7});
            verify(*caught.instance(), *site, input);
        }
        CHECK(caught_exception);
    }

    void simultaneous_activations()
    {
        tuple_fixture input{2};
        auto site = op::exception_throw_site::make_numeric(std::make_shared<int>(67), input.kinds);
        std::array<exn::guest_exception const*, 4> activations{};
        std::array<exn::value const*, 4> values{};
        std::barrier rendezvous{4};
        std::array<std::thread, 4> workers;
        for(unsigned thread{}; thread != workers.size(); ++thread)
        {
            workers[thread] = std::thread([&, thread]
            {
                for(unsigned round{}; round != 8; ++round)
                {
                    try { op::raise_numeric_tuple(*site, input.arguments()); }
                    catch(exn::guest_exception const& caught)
                    {
                        activations[thread] = &caught;
                        values[thread] = caught.instance().get();
                        verify(*caught.instance(), *site, input);
                        rendezvous.arrive_and_wait();
                        for(unsigned other{}; other != workers.size(); ++other)
                        {
                            if(other == thread) { continue; }
                            CHECK(activations[thread] != activations[other] && values[thread] != values[other]);
                        }
                        rendezvous.arrive_and_wait();
                    }
                }
            });
        }
        for(auto& worker : workers) { worker.join(); }
    }
}

int main()
{
    construction_and_ownership();
    for(unsigned repetitions : {0u, 1u, 17u})
    {
        execute<false>(repetitions);
        execute<true>(repetitions);
        execute<true>(repetitions, slot{.i64 = 71});
        execute<true>(repetitions, slot{.i64 = 71}, slot{.i64 = 73});
    }
    simultaneous_activations();
    std::printf("PASS numeric throw: 12 bytecode cases, 32 overlapping activations, %u checks\n", checks.load());
}
