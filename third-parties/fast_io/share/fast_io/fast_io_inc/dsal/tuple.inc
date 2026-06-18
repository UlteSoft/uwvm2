export namespace fast_io
{
namespace containers
{
using ::fast_io::containers::tuple;
using ::fast_io::containers::get;
using ::fast_io::containers::is_tuple;
using ::fast_io::containers::forward_as_tuple;

namespace details
{
#if __cpp_pack_indexing < 202311L
using ::fast_io::containers::details::pack_indexing_before_cxx26_;
#endif
using ::fast_io::containers::details::pack_indexing_;
using ::fast_io::containers::details::pack_indexing_t_;
using ::fast_io::containers::details::tuple_element_impl_;
using ::fast_io::containers::details::pass_type_;
using ::fast_io::containers::details::tuple_impl_;
using ::fast_io::containers::details::tuple_storage_;
using ::fast_io::containers::details::get_tuple_element_by_type_;
using ::fast_io::containers::details::get_tuple_element_index_by_type_;
using ::fast_io::containers::details::apply_impl;
using ::fast_io::containers::details::tuple_size;
using ::fast_io::containers::details::is_tuple_;
} // namespace details

} // namespace containers

using ::fast_io::tuple;
using ::fast_io::get;
using ::fast_io::is_tuple;
using ::fast_io::forward_as_tuple;
using ::fast_io::tuple_size;
} // namespace fast_io
