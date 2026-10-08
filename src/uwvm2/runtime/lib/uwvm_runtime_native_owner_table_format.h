// Pure owned object-record DATA. This parser never authenticates a VM capture,
// code owner, native address, execution lease, loaded relocation or permission.
#pragma once
#ifndef UWVM_MODULE
#include <fast_io.h>
#include <fast_io_unit/string.h>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#endif
namespace uwvm2::runtime::lib::details::native_owner_table_format
{
    inline constexpr ::std::size_t fixed_bytes{32u}, max_name_bytes{65535u};
    struct record
    {
        ::std::string original_ir_name{}, entry_object_name{}, local_entry_object_name{};
        ::std::uint64_t unrelocated_begin_bits{}, unrelocated_end_bits{};
        ::std::uint32_t bytes{};
        unsigned pointer_bytes{}, byte_order{}, continuous_shape{}, role{};
        // Offsets only, NOT executable addresses or pointer permissions.
        ::std::size_t begin_relocation_offset{}, end_relocation_offset{};
    };
    template<unsigned Bits, typename T>
    [[nodiscard]] inline bool get(::std::span<unsigned char const> input,
        ::std::size_t& offset, unsigned order, T& value) noexcept
    {
        static_assert(Bits == 32u || Bits == 64u);
        constexpr ::std::size_t width{Bits / 8u};
        if(input.size() > PTRDIFF_MAX || offset > input.size() ||
           input.size() - offset < width || (order != 1u && order != 2u)) { return false; }
        T parsed{};
        // [one live span ... offset][complete width field][remaining span]
        // [safe] offset<=size and width<=remaining BEFORE either pointer forms.
        auto const* first{input.data() + offset};
        auto const* last{first + width};
        auto const result{order == 1u ?
            ::fast_io::parse_by_scan(first, last, ::fast_io::mnp::le_get<Bits>(parsed)) :
            ::fast_io::parse_by_scan(first, last, ::fast_io::mnp::be_get<Bits>(parsed))};
        if(result.code != ::fast_io::parse_code::ok || result.iter != last) { return false; }
        // [consumed exactly width bytes][remaining same borrowed object span]
        // [safe] full successful field scan and width<=remaining precede advance.
        offset += width; value = parsed; return true;
    }
    [[nodiscard]] inline bool decode(::std::span<unsigned char const> input,
        unsigned actual_pointer_bytes, bool actual_little_endian, record& out)
    {
        out = {};
        if(input.size() < fixed_bytes || input.size() > PTRDIFF_MAX) { return false; }
        constexpr unsigned char magic[8u]{'U','W','V','M','N','E','2',0u};
        for(::std::size_t i{}; i != 8u; ++i) { if(input[i] != magic[i]) { return false; } }
        auto const pointer{input[9u]}, order{input[10u]}, shape{input[11u]}, role{input[12u]};
        if(input[8u] != 2u || (pointer != 4u && pointer != 8u) || pointer != actual_pointer_bytes ||
           order != (actual_little_endian ? 1u : 2u) || shape > 1u || role > 3u ||
           input[13u] != 0u || input[14u] != 0u || input[15u] != 0u) { return false; }
        ::std::size_t offset{16u};
        ::std::uint32_t ir_size{}, entry_size{}, local_size{}, row_size{};
        if(!get<32u>(input,offset,order,ir_size) || !get<32u>(input,offset,order,entry_size) ||
           !get<32u>(input,offset,order,local_size) || !get<32u>(input,offset,order,row_size) ||
           ir_size > max_name_bytes || entry_size > max_name_bytes || local_size > max_name_bytes || row_size > input.size()) { return false; }
        // Three independent bounded16-bit counts sum to <=196605, widths<=8;
        // the constant-size addition/rounding cannot overflow size_t or u32.
        auto const raw_size{fixed_bytes + 2u * pointer + ir_size + entry_size + local_size};
        auto const expected{(raw_size + pointer - 1u) & ~static_cast<::std::size_t>(pointer - 1u)};
        if(row_size != expected) { return false; }
        bool const unnamed{ir_size==0u || entry_size==0u};
        if(unnamed && (ir_size!=0u || entry_size!=0u || local_size!=0u || shape!=0u || role!=0u)) { return false; }
        record value{};
        value.bytes = row_size; value.pointer_bytes = pointer; value.byte_order = order;
        value.continuous_shape = shape; value.role = role;
        if(pointer == 4u)
        {
            ::std::uint32_t begin{}, finish{};
            if(!get<32u>(input,offset,order,begin) || !get<32u>(input,offset,order,finish)) { return false; }
            value.unrelocated_begin_bits=begin; value.unrelocated_end_bits=finish;
        }
        else if(!get<64u>(input,offset,order,value.unrelocated_begin_bits) ||
                !get<64u>(input,offset,order,value.unrelocated_end_bits)) { return false; }
        auto copy_name=[&](::std::uint32_t count,::std::string& name)
        {
            if(offset>row_size || count>row_size-offset) { return false; }
            // [complete bounded name within this same input][row padding]
            // [safe] count<=row_size-offset BEFORE indexed reads/scatter pointer.
            for(::std::size_t i{};i<count;++i) { if(input[offset+i]==0u) { return false; } }
            if(count!=0u)
            {
                auto const* first{input.data()+offset};
                name=::fast_io::concat_std(::fast_io::basic_io_scatter_t<char>{reinterpret_cast<char const*>(first),count});
            }
            // [consumed exact owned name][remaining same bounded record]
            // [safe] complete count<=remaining precedes the numeric cursor move.
            offset+=count;return true;
        };
        if(!copy_name(ir_size,value.original_ir_name) || !copy_name(entry_size,value.entry_object_name) ||
           !copy_name(local_size,value.local_entry_object_name) || offset!=raw_size ||
           (!value.local_entry_object_name.empty() && value.local_entry_object_name==value.entry_object_name)) { return false; }
        // [actual record content][exact zero alignment padding] row_size<=input
        // [safe] padded size is already proved before these indexed reads.
        for(::std::size_t i{raw_size}; i != row_size; ++i) { if(input[i] != 0u) { return false; } }
        value.begin_relocation_offset = fixed_bytes;
        value.end_relocation_offset = fixed_bytes + pointer;
        // Names/roles/unrelocated integers are DATA, never loaded/native rights.
        out = ::std::move(value); return true;
    }
}
