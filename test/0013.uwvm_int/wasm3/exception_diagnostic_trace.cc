// Real throw cold-hook ownership, pre-unwind capture and concurrent immutable diagnostics.
#include <uwvm2/runtime/compiler/uwvm_int/optable/exception_throw.h>
#include <array>
#include <atomic>
#include <barrier>
#include <fast_io.h>
#include <cstring>
#include <new>
#include <thread>
#include <type_traits>
#include <vector>

namespace exn = uwvm2::runtime::exception;
namespace op = uwvm2::runtime::compiler::uwvm_int::optable;
namespace
{
    std::atomic<unsigned> checks{}, captures{};
#define CHECK(condition) do { ++checks; if(!(condition)) { ::fast_io::io::perrln("FAIL ", __LINE__, ": ", #condition); ::fast_io::fast_terminate(); } } while(false)
    thread_local std::vector<exn::diagnostic_frame> live_frames;

    exn::diagnostic_trace_ref capture()
    {
        ++captures;
        CHECK(!live_frames.empty());
        return exn::diagnostic_trace::make(live_frames);
    }

    struct clear_frames_on_unwind
    {
        ~clear_frames_on_unwind() { live_frames.clear(); }
    };

    exn::diagnostic_trace_ref fail_capture() { throw std::bad_alloc{}; }

    void value_ownership()
    {
        static_assert(std::is_same_v<decltype(std::declval<exn::diagnostic_trace const&>().frames()),
                                     std::span<exn::diagnostic_frame const>>);
        auto tag = std::make_shared<int>(5);
        auto plain = exn::value::make(tag, {});
        CHECK(plain && !plain->diagnostic()); // old two-argument callers remain valid.
        std::vector<exn::diagnostic_frame> builder{
            {3, 9, u8"provider", u8"thrower"}, {2, 7, u8"caller", u8"wrapper"}};
        auto trace = exn::diagnostic_trace::make(builder);
        CHECK(trace && trace->frames().size() == 2);
        builder[0].module_name.assign(u8"destroyed");
        builder[0].function_name.clear();
        builder.clear();
        CHECK(trace->frames()[0].module_name == u8"provider");
        CHECK(trace->frames()[0].function_name == u8"thrower");
        CHECK(trace->frames()[0].module_id == 3 && trace->frames()[0].function_index == 9);
        std::weak_ptr<exn::diagnostic_trace const> weak = trace;
        auto value = exn::value::make(tag, {}, trace);
        CHECK(value && value->diagnostic().get() == trace.get());
        trace.reset();
        CHECK(!weak.expired());
        exn::guest_exception original{value};
        exn::guest_exception moved{std::move(original)};
        CHECK(original.instance()->diagnostic().get() == moved.instance()->diagnostic().get());
        auto expected = value->diagnostic().get();
        try { exn::throw_value(value); }
        catch(exn::guest_exception const& caught)
        {
            CHECK(caught.instance().get() == value.get());
            CHECK(caught.instance()->diagnostic().get() == expected);
            try { exn::throw_value(caught.instance()); }
            catch(exn::guest_exception const& rethrown)
            {
                CHECK(&caught != &rethrown);
                CHECK(caught.instance()->diagnostic().get() == rethrown.instance()->diagnostic().get());
            }
        }
    }

    void real_throw_capture()
    {
        auto site = op::exception_throw_site::make_numeric(std::make_shared<int>(11), {});
        op::exception_diagnostic_capture_func.store(capture, std::memory_order_release);
        live_frames = {{4, 12, u8"source", u8"original_throw"}, {1, 3, u8"main", u8"entry"}};
        exn::value_ref retained;
        auto before = captures.load();
        try
        {
            clear_frames_on_unwind guard;
            op::raise_numeric_tuple(*site, {});
        }
        catch(exn::guest_exception const& caught)
        {
            CHECK(live_frames.empty());
            retained = caught.instance();
            CHECK(retained->diagnostic() && retained->diagnostic()->frames().size() == 2);
            CHECK(retained->diagnostic()->frames()[0].function_name == u8"original_throw");
            CHECK(retained->diagnostic()->frames()[1].function_index == 3);
        }
        CHECK(captures.load() == before + 1);
        live_frames = {{8, 15, u8"later_handler", u8"different_stack"}};
        try { exn::throw_value(retained); }
        catch(exn::guest_exception const& caught)
        {
            CHECK(captures.load() == before + 1); // throw_ref/rethrow does not overwrite original provenance.
            CHECK(caught.instance()->diagnostic()->frames()[0].module_name == u8"source");
        }
        live_frames.clear();
        op::exception_diagnostic_capture_func.store(fail_capture, std::memory_order_release);
        bool host_failure{};
        try { op::raise_numeric_tuple(*site, {}); }
        catch(std::bad_alloc const&) { host_failure = true; }
        catch(exn::guest_exception const&) { CHECK(false); }
        CHECK(host_failure); // a diagnostic allocation failure is never converted into a guest exception/trap.
        op::exception_diagnostic_capture_func.store(nullptr, std::memory_order_release);
        try { op::raise_numeric_tuple(*site, {}); }
        catch(exn::guest_exception const& caught) { CHECK(!caught.instance()->diagnostic()); }
    }

    void concurrent_capture()
    {
        auto site = op::exception_throw_site::make_numeric(std::make_shared<int>(17), {});
        op::exception_diagnostic_capture_func.store(capture, std::memory_order_release);
        std::array<exn::diagnostic_trace const*, 4> traces{};
        std::array<std::thread, 4> workers;
        std::barrier rendezvous{4};
        auto before = captures.load();
        for(unsigned worker{}; worker != workers.size(); ++worker)
        {
            workers[worker] = std::thread([&, worker]
            {
                for(unsigned round{}; round != 16; ++round)
                {
                    live_frames = {{worker, round, u8"worker", u8"throw"}, {worker, 100, u8"worker", u8"entry"}};
                    try
                    {
                        clear_frames_on_unwind guard;
                        op::raise_numeric_tuple(*site, {});
                    }
                    catch(exn::guest_exception const& caught)
                    {
                        auto const& trace = caught.instance()->diagnostic();
                        CHECK(live_frames.empty() && trace && trace->frames().size() == 2);
                        CHECK(trace->frames()[0].module_id == worker && trace->frames()[0].function_index == round);
                        CHECK(trace->frames()[1].function_index == 100);
                        traces[worker] = trace.get();
                        rendezvous.arrive_and_wait();
                        for(unsigned other{}; other != workers.size(); ++other)
                        { if(worker != other) { CHECK(traces[worker] != traces[other]); } }
                        rendezvous.arrive_and_wait();
                    }
                }
            });
        }
        for(auto& worker : workers) { worker.join(); }
        CHECK(captures.load() == before + 64);
        op::exception_diagnostic_capture_func.store(nullptr, std::memory_order_release);
    }
}

int main()
{
    value_ownership();
    real_throw_capture();
    concurrent_capture();
    ::fast_io::io::println("PASS exception diagnostic trace: 64 concurrent captures, ", checks.load(), " checks");
}
