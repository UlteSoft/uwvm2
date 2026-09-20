#pragma once

/*
 * Seek and buffer-flush capability probes (CPO level).
 *
 * Directional and bidirectional observers may expose element-based or
 * byte-based seek operations plus explicit buffer-flush commands. This file
 * checks the exact result contracts and combines compatible directional forms.
 * It does not choose fallback direction or change a file position; read/write
 * operation algorithms compose these capabilities when required.
 */

namespace fast_io
{

namespace operations::decay::defines
{

/// Buffer-flush customizations are commands, not queries. Requiring an exact `void` result keeps a same-named status
/// query from entering a dispatcher whose public and recursive contracts intentionally discard no value.
template <typename T>
concept has_input_stream_buffer_flush_define = requires(T t) {
	{ input_stream_buffer_flush_define(t) } -> ::std::same_as<void>;
};

template <typename T>
concept has_output_stream_buffer_flush_define = requires(T t) {
	{ output_stream_buffer_flush_define(t) } -> ::std::same_as<void>;
};

template <typename T>
concept has_io_stream_buffer_flush_define = requires(T t) {
	{ io_stream_buffer_flush_define(t) } -> ::std::same_as<void>;
};

template <typename T>
concept has_input_or_io_stream_buffer_flush_define =
	::fast_io::operations::decay::defines::has_io_stream_buffer_flush_define<T> ||
	::fast_io::operations::decay::defines::has_input_stream_buffer_flush_define<T>;

template <typename T>
concept has_output_or_io_stream_buffer_flush_define =
	::fast_io::operations::decay::defines::has_io_stream_buffer_flush_define<T> ||
	::fast_io::operations::decay::defines::has_output_stream_buffer_flush_define<T>;

template <typename T>
concept has_input_stream_seek_define = requires(T instm, ::fast_io::intfpos_t pos, ::fast_io::seekdir sdir) {
	{ input_stream_seek_define(instm, pos, sdir) } -> ::std::same_as<::fast_io::intfpos_t>;
};

template <typename T>
concept has_input_stream_seek_bytes_define = requires(T instm, ::fast_io::intfpos_t pos, ::fast_io::seekdir sdir) {
	{ input_stream_seek_bytes_define(instm, pos, sdir) } -> ::std::same_as<::fast_io::intfpos_t>;
};

template <typename T>
concept has_output_stream_seek_define = requires(T instm, ::fast_io::intfpos_t pos, ::fast_io::seekdir sdir) {
	{ output_stream_seek_define(instm, pos, sdir) } -> ::std::same_as<::fast_io::intfpos_t>;
};

template <typename T>
concept has_output_stream_seek_bytes_define = requires(T instm, ::fast_io::intfpos_t pos, ::fast_io::seekdir sdir) {
	{ output_stream_seek_bytes_define(instm, pos, sdir) } -> ::std::same_as<::fast_io::intfpos_t>;
};

template <typename T>
concept has_io_stream_seek_define = requires(T instm, ::fast_io::intfpos_t pos, ::fast_io::seekdir sdir) {
	{ io_stream_seek_define(instm, pos, sdir) } -> ::std::same_as<::fast_io::intfpos_t>;
};

template <typename T>
concept has_io_stream_seek_bytes_define = requires(T instm, ::fast_io::intfpos_t pos, ::fast_io::seekdir sdir) {
	{ io_stream_seek_bytes_define(instm, pos, sdir) } -> ::std::same_as<::fast_io::intfpos_t>;
};

template <typename T>
concept has_input_or_io_stream_seek_define = ::fast_io::operations::decay::defines::has_io_stream_seek_define<T> ||
											 ::fast_io::operations::decay::defines::has_input_stream_seek_define<T>;

template <typename T>
concept has_output_or_io_stream_seek_define = ::fast_io::operations::decay::defines::has_io_stream_seek_define<T> ||
											  ::fast_io::operations::decay::defines::has_output_stream_seek_define<T>;

template <typename T>
concept has_input_or_io_stream_seek_bytes_define =
	::fast_io::operations::decay::defines::has_io_stream_seek_bytes_define<T> ||
	::fast_io::operations::decay::defines::has_input_stream_seek_bytes_define<T>;

template <typename T>
concept has_output_or_io_stream_seek_bytes_define =
	::fast_io::operations::decay::defines::has_io_stream_seek_bytes_define<T> ||
	::fast_io::operations::decay::defines::has_output_stream_seek_bytes_define<T>;

} // namespace operations::decay::defines

} // namespace fast_io
