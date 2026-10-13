// Cold numeric handler metadata qualification, independent of translator publication.
#include <uwvm2/runtime/compiler/uwvm_int/optable/exception_metadata.h>
#include <array>
#include <atomic>
#include <barrier>
#include <cfenv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
namespace op = uwvm2::runtime::compiler::uwvm_int::optable;
namespace exn = uwvm2::runtime::exception;
namespace
{
    std::atomic<unsigned> checks{};
#define CHECK(expr) do { checks.fetch_add(1,std::memory_order_relaxed); if(!(expr)) { std::fprintf(stderr,"FAIL %u: %s\n",__LINE__,#expr); std::abort(); } } while(false)
    using kind = exn::payload_kind;
    constexpr std::size_t opcode_width{sizeof(op::uwvm_interpreter_opfunc_t<std::byte const*,std::byte*,std::byte*>) };
    std::array<std::byte,256> code{};
    struct tuple
    {
        std::vector<kind> kinds;
        std::vector<exn::payload_field> fields;
        std::vector<std::byte> bytes;
        template<class T> void append(kind k,T value)
        {
            std::span bits{reinterpret_cast<std::byte const*>(&value),sizeof(value)};
            auto field=exn::payload_field::numeric(k,bits);CHECK(field);
            kinds.push_back(k);fields.push_back(*field);bytes.insert(bytes.end(),bits.begin(),bits.end());
        }
        tuple()
        {
            for(unsigned n{};n!=40;++n)
            {
                append(kind::i32,std::uint32_t{0x80000000u+n});
                append(kind::i64,std::uint64_t{0xff00000000000000ULL+n});
                append(kind::f32,std::uint32_t{0x7f800001u+n});
                append(kind::f64,std::uint64_t{0xfff0000000000001ULL+n});
                std::array<std::byte,16> lanes{};
                for(unsigned i{};i!=16;++i)lanes[i]=std::byte(n+i*17u);
                append(kind::v128,lanes);
            }
        }
    };
    void invalid_descriptors()
    {
        auto tag=std::make_shared<int>(1);
        op::exception_numeric_call_spec call{.stack_bytes_at_call=16,.handlers={{.tag=tag,.target_ip_offset=1,.prefix_bytes=3,.parameter_kinds={kind::i32}}}};
        auto make=[&](std::size_t capacity=32){return op::exception_function_metadata::make_numeric(code,opcode_width,capacity,{&call,1});};
        CHECK(make());CHECK(!op::exception_function_metadata::make_numeric({},opcode_width,32,{&call,1}));
        CHECK(!op::exception_function_metadata::make_numeric(code,0,32,{&call,1}));
        CHECK(!op::exception_function_metadata::make_numeric(code,code.size()+1,32,{&call,1}));
        CHECK(!make(15));
        call.handlers[0].target_ip_offset=code.size()-opcode_width+1;CHECK(!make());call.handlers[0].target_ip_offset=1;
        call.handlers[0].prefix_bytes=17;CHECK(!make());call.handlers[0].prefix_bytes=3;
        call.handlers[0].parameter_kinds={kind::v128,kind::v128};CHECK(!make());
        call.handlers[0].parameter_kinds={kind::reference};CHECK(!make());
        call.handlers[0].parameter_kinds={static_cast<kind>(255)};CHECK(!make());call.handlers[0].parameter_kinds={kind::i32};
        call.handlers[0].tag={};CHECK(!make());
        int borrowed{};call.handlers[0].tag={exn::instance_root{},&borrowed};CHECK(!make());
        call.handlers[0].tag=tag;call.handlers[0].catch_all=true;CHECK(!make());
        call.handlers[0].tag={};CHECK(!make());call.handlers[0].parameter_kinds.clear();CHECK(make());
        std::array<std::shared_ptr<op::exception_throw_site const>,1> missing{};
        CHECK(!op::exception_function_metadata::make_numeric(code,opcode_width,32,{&call,1},missing));
    }
    void stable_ownership_and_order()
    {
        tuple expected;
        auto tag=std::make_shared<int>(8);auto other=std::make_shared<int>(8);
        auto thrown=exn::value::make(tag,expected.fields);auto different=exn::value::make(other,expected.fields);
        std::weak_ptr<int> tag_weak=tag;
        constexpr std::size_t prefix=7,depth=19;
        auto const capacity=prefix+expected.bytes.size();
        op::exception_numeric_handler_spec typed{.tag=tag,.target_ip_offset=17,.prefix_bytes=prefix,.parameter_kinds=expected.kinds};
        auto first=typed;first.tag=other;first.target_ip_offset=33;
        auto later=typed;later.target_ip_offset=49;
        std::vector<op::exception_numeric_call_spec> calls(128);
        for(auto& call:calls)call={depth,{first,typed,later,{.catch_all=true,.target_ip_offset=65,.prefix_bytes=2}}};
        auto throw_tag=std::make_shared<int>(9);std::weak_ptr<int> throw_weak=throw_tag;
        auto throw_site=op::exception_throw_site::make_numeric(throw_tag,{});
        std::weak_ptr<op::exception_throw_site const> site_weak=throw_site;
        std::array throws{throw_site};
        auto owner=op::exception_function_metadata::make_numeric(code,opcode_width,capacity,calls,throws);
        CHECK(owner && owner->call_count()==128 && owner->operand_capacity()==capacity);
        CHECK(owner->call_site(128)==nullptr);
        auto site=owner->call_site(127);CHECK(site && site->context && site->dispatch);
        calls.clear();typed={};first={};later={};tag.reset();other.reset();throw_tag.reset();throw_site.reset();throws[0].reset();
        CHECK(!tag_weak.expired() && !throw_weak.expired() && !site_weak.expired());
        std::vector<op::local_func_storage_t> storage;
        storage.emplace_back();storage[0].exception_metadata=std::move(owner);
        for(unsigned n{};n!=64;++n)storage.emplace_back(); // moving function owners must not move descriptors.
        CHECK(storage[0].exception_metadata->call_site(127)==site);
        auto inspect=[&](exn::value_ref const& value,std::size_t target)
        {
            std::vector<std::byte> frame(capacity+2,std::byte{0xa5});
            auto base=frame.data()+1;
            std::feclearexcept(FE_ALL_EXCEPT);
            auto resumed=site->dispatch(site->context,value,nullptr,base+depth);
            CHECK(resumed.ip==code.data()+target);
            CHECK(resumed.top==base+prefix+expected.bytes.size());
            CHECK(std::memcmp(base+prefix,expected.bytes.data(),expected.bytes.size())==0);
            CHECK(frame.front()==std::byte{0xa5} && frame.back()==std::byte{0xa5});
            for(std::size_t i{};i!=prefix;++i)CHECK(base[i]==std::byte{0xa5});
            CHECK((std::fetestexcept(FE_INVALID)&FE_INVALID)==0);
        };
        inspect(thrown,17);inspect(different,33); // first matching tag; same signature does not alias identity.
        auto unknown_tag=std::make_shared<int>(8);
        auto ref=exn::payload_field::rooted_reference(std::make_shared<int>(44));CHECK(ref);
        std::array ref_field{*ref};auto unknown=exn::value::make(unknown_tag,ref_field);
        std::array<std::byte,32> frame;frame.fill(std::byte{0xb6});
        auto all=site->dispatch(site->context,unknown,nullptr,frame.data()+depth);
        CHECK(all.ip==code.data()+65 && all.top==frame.data()+2);
        for(auto b:frame)CHECK(b==std::byte{0xb6}); // catch_all discards even reference payloads without borrowing them.
        // Concurrent calls share immutable descriptors; each activation gets a distinct operand buffer.
        std::barrier start{5};std::vector<std::thread> workers;
        for(unsigned i{};i!=4;++i)workers.emplace_back([&]{start.arrive_and_wait();for(unsigned n{};n!=16;++n)inspect(thrown,17);});
        start.arrive_and_wait();for(auto& worker:workers)worker.join();
        thrown.reset();different.reset();storage.clear();
        CHECK(tag_weak.expired() && throw_weak.expired() && site_weak.expired());
    }
    void empty_and_unmatched()
    {
        auto tag=std::make_shared<int>(1);auto other=std::make_shared<int>(1);
        auto value=exn::value::make(tag,{});auto miss=exn::value::make(other,{});
        op::exception_numeric_call_spec call{.handlers={{.tag=tag,.target_ip_offset=1}}};
        auto owner=op::exception_function_metadata::make_numeric(code,opcode_width,0,{&call,1});CHECK(owner);
        auto site=owner->call_site(0);
        auto matched=site->dispatch(site->context,value,nullptr,nullptr);CHECK(matched.ip==code.data()+1 && matched.top==nullptr);
        auto rejected=site->dispatch(site->context,miss,nullptr,nullptr);CHECK(rejected.ip==nullptr && rejected.top==nullptr);
    }
}
int main()
{
    invalid_descriptors();stable_ownership_and_order();empty_and_unmatched();
    std::printf("PASS numeric exception metadata: %u checks, 200-field payloads, stable owners, lexical order, concurrent cold dispatch\n",checks.load());
}
