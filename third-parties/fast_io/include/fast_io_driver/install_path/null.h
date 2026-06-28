#pragma once

namespace fast_io::details
{
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
[[noreturn]]
inline void get_module_install_path()
{
	throw_posix_error(ENOTSUP);
}
} // namespace fast_io::details
