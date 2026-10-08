// Execute exact controller source bytes with private-namespace API-result
// doubles. Real VM APIs, capsules and pause/host authority are NOT linked.
// Generate the included branch with checkpoint_controller_fragment.py.
#include <uwvm2/uwvm/debugger/wasip1_state.h>
#include <fast_io_unit/string.h>
#include <array>
#include <memory>
#include <span>
#include <cstdint>
#include <utility>
#ifndef UWVM_TEST_CHECKPOINT_CONTROLLER_FRAGMENT
#error Generate the bounded controller fragment and define UWVM_TEST_CHECKPOINT_CONTROLLER_FRAGMENT.
#endif
namespace ws=::uwvm2::uwvm::debugger::wasip1_state;
namespace checkpoint_double::runtime::lib
{
    enum class llvm_jit_wasip1_environment_capsule_status
    { captured,restored,invalid_capsule_owner,unsupported_resource,stale_environment,
      resource_limit,registry_exhausted,allocation_failed,native_operation_failed,unavailable_environment };
    using cs=llvm_jit_wasip1_environment_capsule_status;
    struct data_type { ::std::uint64_t module{},observed_runtime_epoch{},managed_resources{2u},retained_external_resources{3u}; };
    struct capsule { data_type data{};::std::uint64_t serial{}; };
    using llvm_jit_wasip1_environment_capsule_owner=::std::shared_ptr<capsule const>;
    using owner=llvm_jit_wasip1_environment_capsule_owner;
    struct llvm_jit_wasip1_environment_capsule_request { ::std::uint64_t module{};::std::array<::std::byte,16u> recording_label{}; };
    struct restore_request { ::std::uint64_t module{};bool require_managed_resources{}; };
    struct capture_result { cs status{};owner capsule{}; };
    struct copy_result { cs status{};data_type data{}; };
    inline cs capture_status{cs::captured},copy_status{cs::captured},restore_status{cs::restored};
    inline unsigned capture_calls{},copy_calls{},restore_calls{},fail_copy_call{};
    inline ::std::uint64_t fresh_epoch{7u},restored_serial{},expected_old_serial{};
    inline owner* observed_slot{};
    inline bool copied_before_publication{},restored_strict{};
    inline llvm_jit_wasip1_environment_capsule_request capture_request{};
    inline ::std::weak_ptr<capsule const> last_capture{};
    inline void reset() noexcept
    {
        capture_status=copy_status=cs::captured;restore_status=cs::restored;
        capture_calls=copy_calls=restore_calls=fail_copy_call=0u;
        fresh_epoch=7u;restored_serial=expected_old_serial=0u;observed_slot=nullptr;
        copied_before_publication=restored_strict=false;capture_request={};last_capture.reset();
    }
    inline capture_result llvm_jit_checkpoint_capture_wasip1_environment_host_api(int,::std::span<int const>,
        llvm_jit_wasip1_environment_capsule_request const& requested)
    {
        ++capture_calls;capture_request=requested;if(capture_status!=cs::captured) { return {capture_status,{}}; }
        auto saved=::std::make_shared<struct capsule>();saved->serial=99u;
        saved->data.module=requested.module;saved->data.observed_runtime_epoch=fresh_epoch;
        last_capture=saved;return {cs::captured,::std::move(saved)};
    }
    inline copy_result llvm_jit_checkpoint_copy_wasip1_capsule_data_host_api(owner const& saved)
    {
        ++copy_calls;
        if(saved && saved->serial==99u)
        { copied_before_publication=observed_slot && *observed_slot && (*observed_slot)->serial==expected_old_serial; }
        if(!saved) { return {cs::invalid_capsule_owner,{}}; }
        if(fail_copy_call==copy_calls) { return {copy_status,{}}; }
        return {cs::captured,saved->data};
    }
    inline cs llvm_jit_checkpoint_restore_wasip1_environment_host_api(int,::std::span<int const>,owner const& saved,
        restore_request const& requested)
    { ++restore_calls;restored_serial=saved?saved->serial:0u;restored_strict=requested.require_managed_resources;return restore_status; }
}
namespace api=::checkpoint_double::runtime::lib;
namespace
{
    unsigned checks{};
    void require(bool value,::fast_io::string_view why)
    { ++checks;if(!value) { ::fast_io::io::perrln("checkpoint controller: ",why);::fast_io::fast_terminate(); } }
    struct controller_fixture
    {
        ::std::array<api::owner,8u> wasip1_checkpoints_{};
        ws::view output{};
        void execute(ws::request const& query)
        {
            require(ws::valid(query),"requests obey original admission bounds");
            output={};output.module=query.module;output.operation=query.operation;
            struct reply { ws::view& wasip1_state_values; } result{output};
            int pause_{1};::std::array<int,1u> captures{7};::std::size_t count{1u};
            namespace wasip1_state=::uwvm2::uwvm::debugger::wasip1_state;
            // Only the included branch's runtime namespace changes. The
            // original branch bytes and real WASIp1 view/formatter are used.
#define uwvm2 checkpoint_double
#include UWVM_TEST_CHECKPOINT_CONTROLLER_FRAGMENT
#undef uwvm2
        }
    };
    auto seeded(::std::uint64_t epoch)
    {
        controller_fixture fixture{};
        for(unsigned slot{};slot!=8u;++slot)
        { auto value=::std::make_shared<api::capsule>();value->serial=10u+slot;value->data.observed_runtime_epoch=epoch;fixture.wasip1_checkpoints_[slot]=::std::move(value); }
        return fixture;
    }
    auto request(unsigned operation,unsigned slot)
    {
        ws::request query{};query.index=slot;query.strict_resources=(slot&1u)!=0u;
        query.operation=operation==0u?ws::action::checkpoint_save:operation==1u?ws::action::checkpoint_restore:ws::action::checkpoint_drop;
        return query;
    }
    bool others_unchanged(controller_fixture const& fixture,unsigned selected)
    { for(unsigned slot{};slot!=8u;++slot) { if(slot!=selected && (!fixture.wasip1_checkpoints_[slot] || fixture.wasip1_checkpoints_[slot]->serial!=10u+slot)) { return false; } }return true; }
    void observe(controller_fixture& fixture,unsigned slot)
    { api::observed_slot=::std::addressof(fixture.wasip1_checkpoints_[slot]);api::expected_old_serial=10u+slot; }
    void packet(ws::view const& value,unsigned operation,unsigned slot,unsigned epoch_index,::std::uint64_t epoch)
    {
        ::fast_io::string out{};ws::print(::fast_io::ostring_ref_fast_io{::std::addressof(out)},value);
        ::fast_io::string hex{};hex.reserve(out.size()*2u);constexpr char digits[]{"0123456789abcdef"};
        for(char c:out) { auto b=static_cast<unsigned char>(c);hex.push_back(digits[b>>4u]);hex.push_back(digits[b&15u]); }
        ::fast_io::io::println("PACKET operation=",operation," slot=",slot," epoch-index=",epoch_index," epoch=",epoch," hex=",hex);
    }
    ws::status mapped(api::cs status)
    {
        switch(status)
        {
            case api::cs::restored:case api::cs::captured:return ws::status::ok;
            case api::cs::invalid_capsule_owner:return ws::status::entry_not_found;
            case api::cs::unsupported_resource:return ws::status::unavailable_resource_rollback;
            case api::cs::stale_environment:return ws::status::stale_stop_or_generation;
            case api::cs::resource_limit:case api::cs::registry_exhausted:return ws::status::resource_limit;
            case api::cs::allocation_failed:return ws::status::allocation_failed;
            case api::cs::native_operation_failed:return ws::status::native_operation_failed;
            default:return ws::status::unavailable_environment;
        }
    }
}
int main(int argc,char const** argv)
{
    if(argc!=2 && argc!=3) { return 91; }
    bool const before=argc==3 && ::fast_io::string_view{argv[2],::fast_io::cstr_len(argv[2])}=="before";
    if(argc==3 && !before) { return 92; }
    constexpr ::std::uint64_t epochs[]{1u,7u,UINT64_MAX};
    for(unsigned slot{};slot!=8u;++slot)
    {
        for(unsigned e{};e!=3u;++e) for(unsigned operation{};operation!=3u;++operation)
        {
            api::reset();api::fresh_epoch=epochs[e];auto fixture=seeded(epochs[e]);observe(fixture,slot);
            ::std::weak_ptr<api::capsule const> old=fixture.wasip1_checkpoints_[slot];fixture.execute(request(operation,slot));
            auto const& out=fixture.output;
            require(out.result==ws::status::ok && out.mutation_applied,"normal operation commits");
            require(out.observed_runtime_epoch==(before && operation==2u?0u:epochs[e]),"recorded epoch survives drop");
            require(out.managed_resources==2u && out.retained_external_resources==3u,"resource summary preserved");
            require(operation==2u?!fixture.wasip1_checkpoints_[slot]:fixture.wasip1_checkpoints_[slot] &&
                fixture.wasip1_checkpoints_[slot]->serial==(operation==0u?99u:10u+slot),"only selected slot changes as requested");
            require(others_unchanged(fixture,slot),"other seven slots unchanged");
            require(api::copy_calls==(before && operation==1u?2u:1u),"one metadata read before operation");
            require(api::capture_calls==(operation==0u) && api::restore_calls==(operation==1u),"only intended host API called");
            require(old.expired()==(operation!=1u),"old detached owner retained or retired correctly");
            if(operation==0u)
            {
                require(api::copied_before_publication!=before,"metadata validated before old slot is replaced");
                require(api::capture_request.recording_label[0]==::std::byte{0x57u} &&
                    api::capture_request.recording_label[1]==static_cast<::std::byte>(slot+1u),"capture label remains slot-specific");
            }
            packet(out,operation,slot,e,epochs[e]);
        }
        for(unsigned operation{};operation!=3u;++operation)
        {
            for(auto failure:{api::cs::allocation_failed,api::cs::invalid_capsule_owner})
            {
                api::reset();api::fail_copy_call=1u;api::copy_status=failure;auto fixture=seeded(7u);observe(fixture,slot);
                ::std::weak_ptr<api::capsule const> old=fixture.wasip1_checkpoints_[slot];fixture.execute(request(operation,slot));
                bool const old_save=before && operation==0u;auto const& out=fixture.output;
                auto expected=old_save?ws::status::ok:failure==api::cs::allocation_failed && !before?ws::status::allocation_failed:ws::status::entry_not_found;
                require(out.result==expected && out.mutation_applied==old_save,"metadata failure is reported without a false commit");
                require(fixture.wasip1_checkpoints_[slot] && fixture.wasip1_checkpoints_[slot]->serial==(old_save?99u:10u+slot),"failed save retains original checkpoint");
                require(others_unchanged(fixture,slot),"metadata failure leaves other slots intact");
                require(api::restore_calls==0u,"failed metadata never reaches restore");
                require(api::copy_calls==1u,"failure metadata read is not retried");
                require(out.observed_runtime_epoch==0u && out.managed_resources==0u && out.retained_external_resources==0u,"failure does not publish partial metadata");
                require(old.expired()==old_save,"old owner survives refused edit");
                require(operation!=0u || api::last_capture.expired()!=old_save,"unpublished new owner is retired");
            }
        }
        for(unsigned operation{1u};operation!=3u;++operation)
        {
            api::reset();auto fixture=seeded(7u);auto wrong=::std::make_shared<api::capsule>();wrong->serial=10u+slot;wrong->data.module=1u;
            fixture.wasip1_checkpoints_[slot]=::std::move(wrong);fixture.execute(request(operation,slot));
            require(fixture.output.result==ws::status::entry_not_found && !fixture.output.mutation_applied,"another module cannot use slot");
            require(fixture.wasip1_checkpoints_[slot] && fixture.wasip1_checkpoints_[slot]->data.module==1u,"mismatched module retains owner");
            require(others_unchanged(fixture,slot),"module mismatch leaves other slots intact");
            require(api::restore_calls==0u && api::capture_calls==0u && api::copy_calls==1u,"module mismatch performs no host mutation");
        }
        for(auto failure:{api::cs::resource_limit,api::cs::registry_exhausted,api::cs::allocation_failed,
            api::cs::native_operation_failed,api::cs::unavailable_environment})
        {
            api::reset();api::capture_status=failure;auto fixture=seeded(7u);fixture.execute(request(0u,slot));
            require(fixture.output.result==mapped(failure) && !fixture.output.mutation_applied,"capture failure status preserved");
            require(fixture.wasip1_checkpoints_[slot] && fixture.wasip1_checkpoints_[slot]->serial==10u+slot,"capture failure retains old slot");
            require(others_unchanged(fixture,slot),"capture failure retains other slots");
            require(api::capture_calls==1u && api::copy_calls==(before?1u:0u) && api::restore_calls==0u,"capture failure makes no secondary call");
            require(fixture.output.observed_runtime_epoch==(before?7u:0u),"capture failure does not expose an unrelated old recording");
        }
        for(auto outcome:{api::cs::restored,api::cs::invalid_capsule_owner,api::cs::unsupported_resource,api::cs::stale_environment,
            api::cs::resource_limit,api::cs::registry_exhausted,api::cs::allocation_failed,api::cs::native_operation_failed})
        {
            api::reset();api::restore_status=outcome;auto fixture=seeded(7u);fixture.execute(request(1u,slot));auto const& out=fixture.output;
            require(out.result==mapped(outcome) && out.mutation_applied==(outcome==api::cs::restored),"restore outcome is faithfully reported");
            require(out.observed_runtime_epoch==7u && out.managed_resources==2u && out.retained_external_resources==3u,"restore reply uses checked saved metadata");
            require(fixture.wasip1_checkpoints_[slot] && fixture.wasip1_checkpoints_[slot]->serial==10u+slot && others_unchanged(fixture,slot),"restore never replaces checkpoint owners");
            require(api::copy_calls==(before?2u:1u) && api::capture_calls==0u && api::restore_calls==1u,"no fallible metadata copy after restore");
            require(api::restored_serial==10u+slot && api::restored_strict==((slot&1u)!=0u),"restore receives exact owner and strict selection");
        }
        api::reset();api::fail_copy_call=2u;api::copy_status=api::cs::allocation_failed;auto fixture=seeded(UINT64_MAX);fixture.execute(request(1u,slot));
        require(fixture.output.result==ws::status::ok && fixture.output.mutation_applied,"committed restore remains acknowledged");
        require(fixture.output.observed_runtime_epoch==(before?0u:UINT64_MAX),"post-restore allocation cannot erase epoch");
        require(api::copy_calls==(before?2u:1u) && api::restore_calls==1u,"second metadata read removed");
        require(fixture.wasip1_checkpoints_[slot] && fixture.wasip1_checkpoints_[slot]->serial==10u+slot && others_unchanged(fixture,slot),"committed restore retains saved owners");
        for(unsigned operation{1u};operation!=3u;++operation)
        {
            api::reset();controller_fixture empty{};empty.execute(request(operation,slot));
            require(empty.output.result==ws::status::entry_not_found && !empty.output.mutation_applied,"empty slot is refused");
            require(api::copy_calls==0u && api::capture_calls==0u && api::restore_calls==0u,"empty slot makes no API call");
        }
    }
    ::fast_io::io::println("wasip1_checkpoint_controller ",checks," checks passed cases=1 unsupported=0 phase=",::fast_io::mnp::os_c_str(before?"before":"post"));
}
