// Import only the fixture's exported aliases/observations. No textual product
// header or fabricated native friend definition can hide module attachment bugs.
#include <memory>
#include <type_traits>
import uwvm2test.checkpoint_module_topology;
namespace topology = ::uwvm2test::checkpoint_module_topology;
static_assert(topology::actual_owner_complete && topology::actual_owner_copyable);
static_assert(topology::source_nonmoving && topology::restore_private && topology::stage_private);
static_assert(::std::is_same_v<topology::source::owner, ::std::shared_ptr<topology::source const>>);
static_assert(sizeof(topology::packet) != 0u);
// Syntax/object compilation only. No synthetic stack, paused execution, typed
// plan or native-entry address is built, and this source has no runtime PASS.
