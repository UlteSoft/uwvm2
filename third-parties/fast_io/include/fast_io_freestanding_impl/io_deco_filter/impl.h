#pragma once

#include "destroy.h"
#include "io_deco_filter.h"
#include "io_deco_filter_ref.h"
#include "read.h"
#include "write.h"

namespace fast_io
{

/// @brief Owning buffered decorator layer used by file/decorator composition.
/// @details This convenience type is a storage boundary, not a view.  Normalizing the decorator template argument here
///          prevents an independently written forwarding CPO from accidentally creating a reference data member.  Views
///          remain represented by `basic_io_deco_filter_ref`; the owning layer always destroys its decorator with itself.
template <typename handletype, typename decorators, typename allocator_type = ::fast_io::native_global_allocator>
using basic_io_deco_filt = basic_io_deco_filter<
	handletype,
	basic_io_buffer_traits<::fast_io::buffer_mode::in | ::fast_io::buffer_mode::out, allocator_type,
						   typename ::std::remove_cvref_t<decltype(
							   ::fast_io::operations::input_stream_ref(
								   *static_cast<handletype *>(nullptr)))>::input_char_type,
						   typename ::std::remove_cvref_t<decltype(
							   ::fast_io::operations::output_stream_ref(
								   *static_cast<handletype *>(nullptr)))>::output_char_type>,
	::std::remove_cvref_t<decorators>>;

}
