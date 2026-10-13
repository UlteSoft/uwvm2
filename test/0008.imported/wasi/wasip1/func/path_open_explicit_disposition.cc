// Real WASI path_open regression: guest access bits must not create/truncate.
#include <uwvm2/imported/wasi/wasip1/func/path_open.h>
#include <uwvm2/imported/wasi/wasip1/func/path_open_wasm64.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_close.h>
#include <uwvm2/imported/wasi/wasip1/func/fd_fdstat_set_flags.h>
#include <fast_io.h>
namespace w=::uwvm2::imported::wasi::wasip1;
static unsigned checks{};
static void require(bool good,char const* why)
{++checks;if(!good){::fast_io::io::perrln("path_open_explicit_disposition: ",::fast_io::mnp::os_c_str(why));::fast_io::fast_terminate();}}
int main(int argc,char** argv)
{
 if(argc!=2)return 64;
 ::uwvm2::object::memory::linear::native_memory_t memory{};memory.init_by_page_count(1u);
 w::environment::wasip1_environment<::uwvm2::object::memory::linear::native_memory_t> env{};
 env.wasip1_memory=&memory;env.fd_storage.fd_limit=16u;env.fd_storage.opens.resize(4u);
 auto& dir=*env.fd_storage.opens.index_unchecked(3u).fd_p;
 dir.rights_base=dir.rights_inherit=static_cast<w::abi::rights_t>(0x3fffffffu);
 dir.wasi_fd.ptr->wasi_fd_storage.reset_type(w::fd_manager::wasi_fd_type_e::dir);
 auto& entry=dir.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack.dir_stack.emplace_back().ptr->dir_stack;
 entry.name=u8"regression-root";entry.storage.file=::fast_io::dir_file{::fast_io::mnp::os_c_str(argv[1])};
 w::fd_manager::wasi_fd_rc_t provenance{};auto& chain=dir.wasi_fd.ptr->wasi_fd_storage.storage.dir_stack.dir_stack;
 w::fd_manager::record_checkpoint_path(provenance,chain,u8"existing.bin",true);
 require(provenance.checkpoint_reopenable && provenance.checkpoint_mount==u8"regression-root" && provenance.checkpoint_path==u8"existing.bin" && provenance.checkpoint_follow,"canonical path recorded before moving native owners");
 w::fd_manager::record_checkpoint_path(provenance,chain,u8"./existing.bin",false);
 require(!provenance.checkpoint_reopenable && provenance.checkpoint_path.empty() && provenance.checkpoint_mount.empty(),"ambiguous provenance requires rebinding without retaining paths");
 ::uwvm2::utils::container::u8string long_path{};long_path.resize(9000u);for(auto& c:long_path) { c=u8'x'; }
 w::fd_manager::record_checkpoint_path(provenance,chain,::uwvm2::utils::container::u8string_view{long_path.data(),long_path.size()},false);
 require(!provenance.checkpoint_reopenable && provenance.checkpoint_path.empty() && provenance.checkpoint_mount.empty(),"oversized guest path never duplicated into debug metadata");
 auto old_name=entry.name;entry.name=long_path;
 w::fd_manager::record_checkpoint_path(provenance,chain,u8"existing.bin",false);
 require(!provenance.checkpoint_reopenable && provenance.checkpoint_mount.empty(),"oversized guest mount never duplicated into debug metadata");entry.name=::std::move(old_name);
 chain.emplace_back().ptr->dir_stack.name=long_path;
 w::fd_manager::record_checkpoint_path(provenance,chain,u8"existing.bin",false);
 require(!provenance.checkpoint_reopenable && provenance.checkpoint_path.empty(),"oversized ancestor path remains untracked");chain.pop_back();
 auto path=::fast_io::u8concat_fast_io(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])),u8"/existing.bin");
 auto missing=::fast_io::u8concat_fast_io(::fast_io::mnp::code_cvt(::fast_io::mnp::os_c_str(argv[1])),u8"/missing.bin");
 auto open=[&](bool wide,char8_t const* name,unsigned oflags,unsigned flags)
 {
  auto n=::fast_io::cstr_len(name);auto bytes=reinterpret_cast<::std::byte const*>(name);
  w::memory::write_all_to_memory(memory,32u,bytes,bytes+n);
  auto rights=static_cast<w::abi::rights_t>(0x60006cu); // write, seek, tell; NO READ.
  if(wide)return static_cast<w::abi::errno_t>(w::func::path_open_wasm64(env,3,w::abi::lookupflags_wasm64_t{},32u,n,static_cast<w::abi::oflags_wasm64_t>(oflags),static_cast<w::abi::rights_wasm64_t>(rights),w::abi::rights_wasm64_t{},static_cast<w::abi::fdflags_wasm64_t>(flags),16u));
  return w::func::path_open(env,3,w::abi::lookupflags_t{},32u,static_cast<w::abi::wasi_size_t>(n),static_cast<w::abi::oflags_t>(oflags),rights,w::abi::rights_t{},static_cast<w::abi::fdflags_t>(flags),16u);
 };
 auto close=[&]{auto fd=w::memory::get_basic_wasm_type_from_memory<::std::uint32_t>(memory,16u);require(w::func::fd_close_base(env,static_cast<::std::int32_t>(fd))==w::abi::errno_t::esuccess,"close actual guest FD");};
 auto content=[&](::fast_io::u8string const& file){::fast_io::native_file input{file,::fast_io::open_mode::in};char data[32]{};auto end=::fast_io::operations::read_some(input,data,data+32u);return ::fast_io::string_view{data,static_cast<::std::size_t>(end-data)}=="PRESERVE";};
 auto reset=[&]{::fast_io::native_file file{path,::fast_io::open_mode::out};::fast_io::io::print(file,"PRESERVE");};
 for(bool wide:{false,true})
 {
  reset();require(open(wide,u8"existing.bin",0u,0u)==w::abi::errno_t::esuccess,"write-only existing open");close();require(content(path),"no implicit truncation");
  require(open(wide,u8"missing.bin",0u,0u)==w::abi::errno_t::enoent,"write-only missing requires CREAT");
  require(open(wide,u8"missing.bin",0u,1u)==w::abi::errno_t::enoent,"append missing requires CREAT");
  require(open(wide,u8"existing.bin",0u,1u)==w::abi::errno_t::esuccess,"append existing open");
  auto appended=w::memory::get_basic_wasm_type_from_memory<::std::uint32_t>(memory,16u);
  require(w::func::fd_fdstat_set_flags_base(env,static_cast<::std::int32_t>(appended),w::abi::fdflags_t::fdflag_append)==w::abi::errno_t::esuccess,"idempotent flags preserve state");
#if defined(_WIN32) && !defined(__CYGWIN__)
  require(w::func::fd_fdstat_set_flags_base(env,static_cast<::std::int32_t>(appended),w::abi::fdflags_t{})==w::abi::errno_t::enotsup,"cannot report append cleared while native access remains append-only");
#else
  require(w::func::fd_fdstat_set_flags_base(env,static_cast<::std::int32_t>(appended),w::abi::fdflags_t{})==w::abi::errno_t::esuccess,"actual native APPEND clear");
#endif
  close();require(content(path),"append did not truncate");
  require(open(wide,u8"existing.bin",1u,0u)==w::abi::errno_t::esuccess,"CREAT existing preserves");close();require(content(path),"CREAT without TRUNC preserves");
  require(open(wide,u8"existing.bin",5u,0u)==w::abi::errno_t::eexist,"CREAT EXCL cannot replace");require(content(path),"EXCL preserves");
  require(open(wide,u8"existing.bin",8u,0u)==w::abi::errno_t::esuccess,"explicit TRUNC existing");close();{::fast_io::native_file file{path,::fast_io::open_mode::in};require(::fast_io::status(file).size==0u,"explicit TRUNC honored");}
  require(open(wide,u8"missing.bin",8u,0u)==w::abi::errno_t::enoent,"TRUNC without CREAT cannot create");
  require(open(wide,u8"missing.bin",9u,0u)==w::abi::errno_t::esuccess,"CREAT TRUNC creates");close();
  ::fast_io::native_unlinkat(::fast_io::at_fdcwd(),missing,{});
  reset();
  // Keep child rights in the parent base as well: this exposes missing
  // operation checks independently of the inherited-rights regression.
  auto child=static_cast<w::abi::rights_t>(0x60006cu);
  dir.rights_base=w::abi::rights_t::right_path_open|child;
  require(open(wide,u8"missing.bin",1u,0u)==w::abi::errno_t::enotcapable,"CREAT requires parent PATH_CREATE_FILE");
  try { ::fast_io::native_file file{missing,::fast_io::open_mode::in};require(false,"denied CREAT must not create"); }
  catch(::fast_io::error const&) { require(true,"denied CREAT preserves missing file"); }
  require(open(wide,u8"existing.bin",8u,0u)==w::abi::errno_t::enotcapable,"TRUNC requires parent PATH_FILESTAT_SET_SIZE");
  require(content(path),"denied TRUNC preserves content");
  dir.rights_base|=w::abi::rights_t::right_path_create_file;
  require(open(wide,u8"missing.bin",9u,0u)==w::abi::errno_t::enotcapable,"CREAT TRUNC needs both parent operations");
  dir.rights_base=w::abi::rights_t::right_path_open|w::abi::rights_t::right_path_filestat_set_size;
  require(open(wide,u8"missing.bin",9u,0u)==w::abi::errno_t::enotcapable,"TRUNC capability cannot replace CREATE capability");
  require(open(wide,u8"existing.bin",8u,0u)==w::abi::errno_t::esuccess,"parent operation and inheriting rights authorize TRUNC");close();
  { ::fast_io::native_file file{path,::fast_io::open_mode::in};require(::fast_io::status(file).size==0u,"authorized restricted-parent TRUNC honored"); }
  reset();dir.rights_base=w::abi::rights_t::right_path_open|w::abi::rights_t::right_path_create_file;
  require(open(wide,u8"missing.bin",1u,0u)==w::abi::errno_t::esuccess,"parent operation and inheriting rights authorize CREATE");close();
  ::fast_io::native_unlinkat(::fast_io::at_fdcwd(),missing,{});
  dir.rights_base=w::abi::rights_t::right_path_open;
  for(unsigned flag:{2u,8u,16u})
  { require(open(wide,u8"existing.bin",0u,flag)==w::abi::errno_t::enotcapable,"synchronized open requires parent synchronization capability"); }
  dir.rights_base|=w::abi::rights_t::right_fd_datasync;
  for(unsigned flag:{8u,16u})
  { require(open(wide,u8"existing.bin",0u,flag)==w::abi::errno_t::enotcapable,"DATASYNC cannot grant RSYNC or SYNC"); }
  // Native DSYNC/RSYNC availability differs by OS; admission succeeds or
  // reports unsupported rather than inventing a successful native flag.
  auto sync_open=[&](unsigned flag)
  { auto status=open(wide,u8"existing.bin",0u,flag);require(status==w::abi::errno_t::esuccess || status==w::abi::errno_t::enotsup,"authorized native synchronized open");if(status==w::abi::errno_t::esuccess) { close(); } };
  sync_open(2u);dir.rights_base=w::abi::rights_t::right_path_open|w::abi::rights_t::right_fd_sync;
  sync_open(2u);sync_open(8u);
#if defined(_WIN32) && !defined(__CYGWIN__)
  require(open(wide,u8"existing.bin",0u,16u)==w::abi::errno_t::enotsup,"Windows refuses unrepresentable SYNC");
#else
  sync_open(16u);
#endif
  require(content(path),"synchronized opens preserve content");
  reset();dir.rights_base=w::abi::rights_t::right_path_open;
  require(open(wide,u8"existing.bin",0u,0u)==w::abi::errno_t::esuccess,"child rights come from directory inheriting rights, not its base rights");close();require(content(path),"inherited write access never implicitly truncates");
  dir.rights_inherit=w::abi::rights_t{};
  require(open(wide,u8"existing.bin",0u,0u)==w::abi::errno_t::enotcapable,"child rights cannot exceed directory inheriting rights");
  dir.rights_base=w::abi::rights_t{};dir.rights_inherit=static_cast<w::abi::rights_t>(0x3fffffffu);
  require(open(wide,u8"existing.bin",0u,0u)==w::abi::errno_t::enotcapable,"inheriting rights cannot replace directory PATH_OPEN capability");
  dir.rights_base=dir.rights_inherit=static_cast<w::abi::rights_t>(0x3fffffffu);
 }
 ::fast_io::io::println("path_open_explicit_disposition PASS checks=",checks);
}
