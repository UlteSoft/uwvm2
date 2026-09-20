#pragma once

/*
 * Buffer-adapter admission probes (CPO level).
 *
 * These concepts determine whether a handle can first be normalized to an
 * input and/or output observer and therefore wrapped by the corresponding
 * buffering adapter. They are construction prerequisites only; ibuffer and
 * obuffer cursor protocols are recognized in the directional stream headers.
 */

namespace fast_io
{

namespace operations::defines
{
template <typename handletype>
concept available_add_ibuf = requires {
	typename ::std::remove_cvref_t<decltype(
		::fast_io::operations::input_stream_ref(*static_cast<handletype *>(nullptr)))>::input_char_type;
};

template <typename handletype>
concept available_add_obuf = requires {
	typename ::std::remove_cvref_t<decltype(
		::fast_io::operations::output_stream_ref(*static_cast<handletype *>(nullptr)))>::output_char_type;
};

template <typename handletype>
concept available_add_iobuf = available_add_ibuf<handletype> || available_add_obuf<handletype>;

} // namespace operations::defines
} // namespace fast_io
