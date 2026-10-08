// Native ownership/concurrency/allocation proof for the actual exception
// header. This is not a whole-VM Wasm or P-core exception benchmark.
#include <uwvm2/runtime/exception/value.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <barrier>
#include <new>
#include <span>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace exn = ::uwvm2::runtime::exception;
namespace
{
    ::std::atomic<unsigned> checks{};
    thread_local bool count_allocations{};
    thread_local ::std::size_t allocations{};
    void require(bool condition, char const* message) noexcept
    {
        ++checks;
        if(!condition)
        {
            ::fast_io::io::perrln("FAIL compact exception trace: ", ::fast_io::mnp::os_c_str(message));
            ::fast_io::fast_terminate();
        }
    }
    template<class Function>
    [[nodiscard]] auto counted(Function&& function)
    {
        allocations = 0uz;
        count_allocations = true;
        auto result{function()};
        count_allocations = false;
        return result;
    }
}

// These are C++ allocation functions, which can throw bad_alloc. Their actual
// ELF aliases must retain that exception behavior; they are not POSIX C APIs.
extern "C" void* actual_cpp_new(::std::size_t) __asm__("__real__Znwm");
extern "C" void* counted_cpp_new(::std::size_t) __asm__("__wrap__Znwm");
extern "C" void* counted_cpp_new(::std::size_t bytes)
{
    if(count_allocations) { ++allocations; }
    return actual_cpp_new(bytes);
}

int main()
{
    static_assert(::std::is_same_v<decltype(::std::declval<exn::diagnostic_trace const&>().frames()),
        ::std::span<exn::diagnostic_frame const>>);
    ::std::u8string const module_name{u8"original provider module with a deliberately long owned diagnostic name"};
    ::std::u8string const function_name{u8"original throwing function with a deliberately long owned diagnostic name"};
    ::std::vector<exn::diagnostic_module_symbols> builder{};
    builder.push_back({.name = module_name, .function_count = 4uz,
        .functions = {{1uz, function_name}, {3uz, u8"caller"}}});
    auto symbols{exn::diagnostic_symbols::make(builder)};
    require(symbols && symbols->contains({0uz, 3uz}) && !symbols->contains({0uz, 4uz}) &&
        !symbols->contains({1uz, 0uz}), "complete symbol cohort validates public indices");
    builder[0].name = u8"destroyed module";
    builder[0].functions[0].name = u8"destroyed function";
    builder.clear();
    require(symbols->names({0uz, 1uz}).module_name == module_name &&
        symbols->names({0uz, 1uz}).function_name == function_name,
        "lvalue publication owns names independently of the original builder");
    require(symbols->names({0uz, 2uz}).function_name.empty(), "unnamed functions retain their real public index");
    ::std::weak_ptr<exn::diagnostic_symbols const> weak_symbols{symbols};
    ::std::vector<exn::diagnostic_frame_index> indices{{0uz, 1uz}, {0uz, 3uz}};
    auto trace{counted([&]() { return exn::diagnostic_trace::make_compact(symbols, ::std::move(indices), true); })};
    require(trace && allocations == 1uz, "compact immutable trace/control block requires one actual scalar allocation");
    auto depth{counted([&]() { return trace->frame_count(); })};
    require(depth == 2uz && allocations == 0uz && trace->truncated(), "depth queries allocate no names or frame cache");
    symbols.reset();
    require(!weak_symbols.expired(), "trace strongly retains names after the runtime symbol owner retires");

    ::std::barrier start{5};
    ::std::array<::std::thread, 4uz> workers{};
    ::std::array<exn::diagnostic_frame const*, 4uz> published{};
    for(::std::size_t index{}; index != workers.size(); ++index)
    {
        workers[index] = ::std::thread{[&, index, owner = trace]()
        {
            start.arrive_and_wait();
            for(unsigned round{}; round != 1000u; ++round)
            {
                require(owner->frame_count() == 2uz, "concurrent depth query never observes a partially populated cache");
                auto const frames{owner->frames()};
                require(frames.size() == 2uz && frames[0].module_name == module_name &&
                    frames[0].function_name == function_name && frames[1].function_name == u8"caller",
                    "all concurrent readers see complete immutable original names");
                // [trace-owned complete immutable frame array] one-past
                // [safe                                     ] this borrowed
                // pointer is compared only while the main trace owner is live.
                published[index] = frames.data();
            }
        }};
    }
    start.arrive_and_wait();
    for(auto& worker: workers) { worker.join(); }
    for(auto const address: published)
    { require(address == trace->frames().data(), "one fully initialized name cache is published to every reader"); }

    auto tag{::std::make_shared<int>(19)};
    ::std::vector<exn::payload_field> fields{exn::payload_field::null_reference()};
    auto value{counted([&]() { return exn::value::make_owned(tag, ::std::move(fields), trace); })};
    require(value && allocations == 1uz && value->fields().size() == 1uz,
        "owned exception value/control block requires one actual scalar allocation");
    auto plain{counted([&]() { return exn::value::make(tag, {}); })};
    require(plain && allocations == 1uz, "empty immutable exception value requires one allocation");
    trace.reset();
    try { exn::throw_value(value); }
    catch(exn::guest_exception const& caught)
    {
        require(caught.instance().get() == value.get() && caught.instance()->diagnostic()->frames()[0].function_name == function_name,
            "actual native throwing activation retains the original immutable instance and names");
        try { exn::throw_value(caught.instance()); }
        catch(exn::guest_exception const& rethrown)
        {
            require(rethrown.instance().get() == value.get() &&
                rethrown.instance()->diagnostic()->frame_count() == 2uz,
                "real rethrow preserves the original instance and throw-site identities");
        }
    }
    value.reset();
    require(weak_symbols.expired(), "retiring the final exception trace releases its symbol owner");

    auto valid{exn::diagnostic_symbols::make({{.function_count = 1uz}})};
    require(valid && !exn::diagnostic_trace::make_compact(valid, {{0uz, 1uz}}) &&
        !exn::diagnostic_trace::make_compact(valid, {{1uz, 0uz}}), "invalid native compact identities are rejected before publication");
    ::std::shared_ptr<exn::diagnostic_symbols const> empty{};
    ::std::shared_ptr<exn::diagnostic_symbols const> borrowed{empty, valid.get()};
    require(!exn::diagnostic_trace::make_compact(borrowed, {}), "empty-owner symbol alias cannot escape into a trace");
    auto unrelated{::std::make_shared<int>(23)};
    ::std::shared_ptr<exn::diagnostic_symbols const> foreign_owner_alias{unrelated, valid.get()};
    require(foreign_owner_alias.use_count() != 0 &&
        !exn::diagnostic_trace::make_compact(foreign_owner_alias, {{0uz, 0uz}}),
        "nonempty unrelated control block cannot claim ownership of a diagnostic table");
    ::std::shared_ptr<exn::diagnostic_symbols const> genuine_alias{valid, valid.get()};
    auto alias_trace{exn::diagnostic_trace::make_compact(genuine_alias, {{0uz, 0uz}})};
    require(alias_trace && alias_trace->frames().size() == 1uz,
        "genuine factory control-block aliases still retain their actual table");
    require(!exn::diagnostic_symbols::make({{.function_count = 4uz, .functions = {{2uz, u8"a"}, {2uz, u8"b"}}}}) &&
        !exn::diagnostic_symbols::make({{.function_count = 4uz, .functions = {{2uz, u8"a"}, {1uz, u8"b"}}}}) &&
        !exn::diagnostic_symbols::make({{.function_count = 1uz, .functions = {{1uz, u8"bad"}}}}),
        "duplicate, unsorted and out-of-range symbols never become immutable lookup tables");
    auto zero{exn::diagnostic_trace::make_compact(valid, {})};
    require(zero && zero->frame_count() == 0uz && zero->frames().empty(), "empty compact traces remain readable");
    auto legacy{exn::diagnostic_trace::make({{3uz, 9uz, u8"legacy module", u8"legacy function"}})};
    require(legacy && legacy->frame_count() == 1uz && legacy->frames()[0].function_name == u8"legacy function",
        "existing independent diagnostic builders preserve their original span interface");
    ::fast_io::io::println("PASS compact exception trace: checks=", checks.load(),
        "; actual header, allocation wrappers, concurrent materialization, native throw/rethrow; wholeVM=false");
}
