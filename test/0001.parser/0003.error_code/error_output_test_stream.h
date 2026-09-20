#pragma once
#include <fast_io.h>

// The isolated character matrix can use direct file observers to avoid
// multiplying fast_io's buffered planning instantiations. This changes only
// the test sink, not the production error formatter or its character type.
#if defined(UWVM_TEST_ERROR_DIRECT_IO)
template <typename Char> using error_test_output_file = ::fast_io::basic_native_file<Char>;
inline auto error_test_u8err() { return ::fast_io::u8err(); }
#else
template <typename Char> using error_test_output_file = ::fast_io::basic_obuf_file<Char>;
inline auto error_test_u8err()
{ return ::fast_io::basic_obuf<::fast_io::u8native_io_observer>{::fast_io::u8err()}; }
#endif
