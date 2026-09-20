#pragma once

namespace fast_io
{

class custom_global_allocator;

#if defined(FAST_IO_DISABLE_CUSTOM_THREAD_LOCAL_ALLOCATOR)
// Disabling the distinct thread-local backend preserves the allocator type
// contract by making it identical to the global backend. This declaration must
// remain syntactically complete even when no downstream configuration selects
// the alias, because every public-header inclusion parses this branch.
using custom_thread_local_allocator = custom_global_allocator;
#else
class custom_thread_local_allocator;
#endif

} // namespace fast_io
