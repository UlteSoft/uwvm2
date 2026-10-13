export namespace fast_io
{

#ifndef FAST_IO_FREESTANDING
// The hosted header already supplies these standard-string front doors.
// Re-export their existing global owners, matching the native-string bridge.
using ::fast_io::io_alias_t;
using ::fast_io::io_alias;
using ::fast_io::io_strlike_type_t;
using ::fast_io::basic_ostring_ref_std;
using ::fast_io::ostring_ref_std;
using ::fast_io::u8ostring_ref_std;
using ::fast_io::u16ostring_ref_std;
using ::fast_io::u32ostring_ref_std;
using ::fast_io::io_strlike_ref;
using ::fast_io::strlike_construct_define;
using ::fast_io::strlike_construct_single_character_define;
using ::fast_io::concat_std;
using ::fast_io::u8concat_std;
using ::fast_io::u16concat_std;
using ::fast_io::u32concat_std;
using ::fast_io::concatln_std;
using ::fast_io::u8concatln_std;
using ::fast_io::u16concatln_std;
using ::fast_io::u32concatln_std;
#if (!defined(_LIBCPP_VERSION)) || _LIBCPP_HAS_WIDE_CHARACTERS
using ::fast_io::wostring_ref_std;
using ::fast_io::wconcat_std;
using ::fast_io::wconcatln_std;
#endif
#endif

inline namespace io
{
using ::fast_io::io::print;
using ::fast_io::io::println;
using ::fast_io::io::perr;
using ::fast_io::io::perrln;
using ::fast_io::io::panic;
using ::fast_io::io::panicln;
using ::fast_io::io::scan;

#ifndef FAST_IO_DISABLE_DEBUG_PRINT
using ::fast_io::io::debug_print;
using ::fast_io::io::debug_println;
using ::fast_io::io::debug_perr;
using ::fast_io::io::debug_perrln;
#endif
} // namespace io

} // namespace fast_io
