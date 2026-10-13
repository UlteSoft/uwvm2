#include "../0003.utils/control/buffer_helpers.h"
using namespace control_test;
#include <uwvm2/utils/control/impl.h>
#include <array>
#include <cstdlib>
#include <vector>
namespace ctl=uwvm2::utils::control;
// Independent wire bytes catch an accidental switch from fixed-width LE to LEB128.
static constexpr bool fixed_width_wire_layout()
{
 constexpr unsigned char expected[]{
  'U','W','C','1', 1,0, 1,0, 0,0,0,0, 0,0,0,0,
  0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
  0x10,0x32,0x54,0x76,0x98,0xba,0xdc,0xfe,
  0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88};
 ctl::frame_header header{ctl::operation::status,0,{},0xfedcba9876543210ull,0x8877665544332211ull};
 for(std::size_t i{};i!=header.instance.size();++i)header.instance[i]=ctl::wire_byte(i);
 std::array<ctl::wire_byte,ctl::header_bytes> output{};
 ctl::output_buffer destination{output};
 auto encoded=ctl::encode_frame(header,{},destination);
 if(encoded.status!=ctl::error::none||encoded.written!=sizeof(expected))return false;
 for(std::size_t i{};i!=sizeof(expected);++i)if(output[i]!=ctl::wire_byte(expected[i]))return false;
 ctl::frame_header decoded{};
 return ctl::details::decode_header(input(output),decoded)==ctl::error::none&&decoded.command==header.command&&
  decoded.payload_bytes==0&&decoded.instance==header.instance&&decoded.generation==header.generation&&decoded.request_id==header.request_id;
}
static_assert(fixed_width_wire_layout());
static unsigned checks{};
#define CHECK(x) do { ++checks; if(!(x)){fast_io::io::perrln("FAIL ",__LINE__,": ",fast_io::mnp::os_c_str(#x));fast_io::fast_terminate();} }while(false)
static ctl::launch_config config(){ctl::launch_config c;c.debug_enabled=true;c.compiler=ctl::backend::llvm;c.origin=ctl::launch_origin::console;c.instance[0]=1u;c.vm_process=1;return c;}
static std::vector<ctl::wire_byte> frame(ctl::operation op,ctl::input_buffer payload={},std::uint64_t id=1)
{
 std::vector<ctl::wire_byte> bytes(ctl::header_bytes+ctl::remaining_bytes(payload));auto c=config();
 ctl::output_buffer output{bytes};
 CHECK(ctl::encode_frame({op,static_cast<std::uint32_t>(ctl::remaining_bytes(payload)),c.instance,1,id},payload,output).status==ctl::error::none);return bytes;
}
// Views may start in the middle of a caller-owned buffer. Failed encoding
// must leave both bytes and the output cursor untouched.
static void buffer_cursor_checks()
{
 std::array<ctl::wire_byte,8> body{0,0x80,0xff,0,4,5,6,7};
 std::array<ctl::wire_byte,2*(ctl::header_bytes+8)+2> storage{};
 storage.fill(0xa5);
 ctl::output_buffer output{storage.data()+1,storage.data()+storage.size()-1};
 auto original=output.curr_ptr;
 auto payload=input(body);
 auto first=ctl::encode_frame({ctl::operation::step,8,config().instance,1,1},payload,output);
 CHECK(first.status==ctl::error::none&&output.curr_ptr==original+first.written);
 CHECK(payload.curr_ptr==body.data());
 auto second=ctl::encode_frame({ctl::operation::step,8,config().instance,1,2},payload,output);
 CHECK(second.status==ctl::error::none&&output.curr_ptr==output.end_ptr);
 CHECK(storage.front()==0xa5&&storage.back()==0xa5);
 ctl::frame_decoder decoder;
 auto source=ctl::input_buffer{original,output.curr_ptr};
 auto one=decoder.feed(source);CHECK(one.complete&&one.consumed==first.written);
 CHECK(source.curr_ptr==original&&decoder.header().request_id==1);
 auto decoded=decoder.payload();CHECK(ctl::remaining_bytes(decoded)==body.size());
 for(std::size_t i{};i!=body.size();++i)CHECK(decoded.curr_ptr[i]==body[i]);
 CHECK(decoder.feed({}).status==ctl::error::busy);
 decoder.reset();auto two=decoder.feed({original+one.consumed,output.curr_ptr});
 CHECK(two.complete&&two.consumed==second.written&&decoder.header().request_id==2);
 decoder.reset();CHECK(decoder.feed({}).consumed==0&&!decoder.ready());
 for(std::size_t available{};available!=first.written;++available)
 {
  std::array<ctl::wire_byte,ctl::header_bytes+8> short_bytes{};short_bytes.fill(0x5a);
  ctl::output_buffer short_output{short_bytes.data(),short_bytes.data()+available};
  auto failed=ctl::encode_frame({ctl::operation::step,8,config().instance,1,1},payload,short_output);
  CHECK(failed.status==ctl::error::truncated&&failed.written==0&&short_output.curr_ptr==short_bytes.data());
  for(auto byte:short_bytes)CHECK(byte==0x5a);
 }
 ctl::output_buffer empty;
 CHECK(ctl::encode_frame({ctl::operation::status,0,config().instance,1,1},{},empty).status==ctl::error::truncated);
}
int main()
{
 CHECK(fixed_width_wire_layout());
 buffer_cursor_checks();
 std::array<ctl::wire_byte,24> payload{};put_le<std::uint64_t>(payload,0,1);
 for(auto const op:{ctl::operation::breakpoint_set,ctl::operation::breakpoint_clear,ctl::operation::breakpoint_list,ctl::operation::backtrace,ctl::operation::locals})
 {
  auto const length=op==ctl::operation::breakpoint_set?24u:op==ctl::operation::breakpoint_list?0u:8u;
  auto const bytes=frame(op,prefix(payload,length));
  for(std::size_t split{};split<=bytes.size();++split)
  {
   ctl::launch_authority authority(config());ctl::control_session session;
   CHECK(session.attach_console(authority.issue_permit())==ctl::error::none);
   if(op==ctl::operation::locals)CHECK(session.confirm_execution_state(ctl::execution_state::stopped)==ctl::error::none);
   auto part=session.receive(input(prefix(bytes,split)));CHECK(part.status==ctl::error::none);
   if(split<bytes.size()){CHECK(!part.request);part=session.receive(input(suffix(bytes,split)));}
   CHECK(part.status==ctl::error::none && part.request);
   CHECK(session.confirm_execution_state(ctl::execution_state::stopped)==ctl::error::busy);
   auto const complete=(op==ctl::operation::breakpoint_set||op==ctl::operation::breakpoint_clear)?ctl::host_completion::configured:ctl::host_completion::inspected;
   CHECK(session.complete(*part.request,ctl::host_completion::replaced)==ctl::error::wrong_completion);
   CHECK(session.complete(*part.request,complete)==ctl::error::none);
   CHECK(session.confirm_execution_state(ctl::execution_state::stopped)==ctl::error::none);
   auto step=session.receive(input(frame(ctl::operation::step,prefix(payload,8),2)));CHECK(step.request);
   CHECK(session.complete(*step.request,ctl::host_completion::stepped)==ctl::error::none);
   CHECK(session.receive(input(bytes)).status==ctl::error::replay);
  }
  {ctl::control_session s;CHECK(s.receive(input(bytes)).status==ctl::error::unauthorized);}
  {auto c=config();c.debug_enabled=false;c.replacement_enabled=true;ctl::launch_authority a(c);ctl::control_session s;
   CHECK(s.attach_console(a.issue_permit())==ctl::error::none);CHECK(s.receive(input(bytes)).status==ctl::error::unavailable_capability);}
  if(op==ctl::operation::locals)
  {ctl::launch_authority a(config());ctl::control_session s;CHECK(s.attach_console(a.issue_permit())==ctl::error::none);
   CHECK(s.receive(input(bytes)).status==ctl::error::invalid_state);}
  for(std::size_t size{};size<=24;++size)if(size!=length)
  {
   ctl::launch_authority a(config());ctl::control_session s;CHECK(s.attach_console(a.issue_permit())==ctl::error::none);
   if(op==ctl::operation::locals)CHECK(s.confirm_execution_state(ctl::execution_state::stopped)==ctl::error::none);
   CHECK(s.receive(input(frame(op,prefix(payload,size)))).status==ctl::error::malformed);
  }
  for(std::size_t size{};size<bytes.size();++size)
  {
   ctl::launch_authority a(config());ctl::control_session s;CHECK(s.attach_console(a.issue_permit())==ctl::error::none);
   if(op==ctl::operation::locals)CHECK(s.confirm_execution_state(ctl::execution_state::stopped)==ctl::error::none);
   CHECK(s.receive(input(prefix(bytes,size))).status==ctl::error::none);
   CHECK(s.finish_stream()==(size==0?ctl::error::none:ctl::error::truncated));
  }
 }
 fast_io::io::println("PASS debugger protocol: ",checks," checks");
}
