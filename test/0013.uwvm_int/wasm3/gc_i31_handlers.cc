// Exercise real threaded interpreter handlers, including the numeric register ring.
// The independent WebAssembly text oracle is test/0017.runtime/fixtures/gc_i31_execution.wat.
#include <uwvm2/uwvm/io/impl.h>
#include <uwvm2/runtime/compiler/uwvm_int/optable/gc.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace opt = ::uwvm2::runtime::compiler::uwvm_int::optable;
using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
using ref_t = ::uwvm2::object::global::wasm_funcref_t;
using byte = ::std::byte;
using ip_t = byte const*;
using sp_t = byte*;
using local_t = byte*;
using opfunc_t = opt::uwvm_interpreter_opfunc_t<ip_t, sp_t, local_t>;
using ring_opfunc_t = opt::uwvm_interpreter_opfunc_t<ip_t, sp_t, local_t, wasm_i32>;
static opfunc_t volatile plain_handler_slot{};
static ring_opfunc_t volatile ring_handler_slot{};

static sp_t seen_sp{};
static wasm_i32 seen_ring{};
static unsigned seen_count{};
static void end_plain(ip_t, sp_t sp, local_t) noexcept { seen_sp = sp; ++seen_count; }
static void end_ring(ip_t, sp_t sp, local_t, wasm_i32 value) noexcept
{ seen_sp = sp; seen_ring = value; ++seen_count; }

template<typename T> static T read(byte const* p) noexcept
{ T result; ::std::memcpy(&result, p, sizeof(result)); return result; }
template<typename T> static void write(byte* p, T const& value) noexcept
{ ::std::memcpy(p, &value, sizeof(value)); }
static bool check(bool condition, char const* what) noexcept
{ if(condition) { return true; } ::std::fprintf(stderr, "FAIL gc i31 handlers: %s\n", what); return false; }

int main()
{
    constexpr opt::uwvm_interpreter_translate_option_t byref{.is_tail_call = false};
    constexpr opt::uwvm_interpreter_translate_option_t tail{.is_tail_call = true};
    constexpr opt::uwvm_interpreter_translate_option_t ring{
        .is_tail_call = true, .i32_stack_top_begin_pos = 3uz, .i32_stack_top_end_pos = 4uz};
    ::std::array<byte, 128> stack{};
    ::std::array<byte, 2 * sizeof(ring_opfunc_t)> code{};
    ip_t ip{code.data()};
    sp_t sp{stack.data()};
    local_t local{stack.data()};
    opt::uwvm_interpreter_stacktop_currpos_t ring_position{};
    ring_position.i32_stack_top_curr_pos = 3uz;
    opt::uwvm_interpreter_stacktop_currpos_t plain_position{};

    auto const byref_make{opt::translate::get_uwvmint_ref_i31_fptr<byref, ip_t, sp_t, local_t>(plain_position)};
    auto const byref_signed{opt::translate::get_uwvmint_i31_get_fptr<byref, true, ip_t, sp_t, local_t>(plain_position)};
    auto const byref_unsigned{opt::translate::get_uwvmint_i31_get_fptr<byref, false, ip_t, sp_t, local_t>(plain_position)};
    for(wasm_i32 input : {wasm_i32{-1}, wasm_i32{0x4000'0000}, wasm_i32{0x7fff'ffff}})
    {
        ip = code.data(); sp = stack.data(); write(sp, input); sp += sizeof(input);
        byref_make(ip, sp, local);
        if(!check(ip == code.data() + sizeof(byref_make), "byref ref.i31 instruction pointer") ||
           !check(sp == stack.data() + sizeof(ref_t), "byref ref.i31 stack height")) { return 1; }
        auto const ref{read<ref_t>(stack.data())};
        if(!check(ref.ref.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_i31, "i31 kind") ||
           !check(ref.ref.storage.wasm_i31.get_u() == (static_cast<::std::uint32_t>(input) & 0x7fff'ffffu), "i31 payload")) { return 1; }
        ip = code.data(); byref_signed(ip, sp, local);
        auto const expected_signed{::uwvm2::object::global::make_wasm_i31_reference(input).storage.wasm_i31.get_s()};
        if(!check(sp == stack.data() + sizeof(wasm_i32), "byref i31.get_s stack height") ||
           !check(read<wasm_i32>(stack.data()) == expected_signed, "byref i31.get_s value")) { return 1; }
        ip = code.data(); sp = stack.data(); write(sp, ref); sp += sizeof(ref);
        byref_unsigned(ip, sp, local);
        if(!check(sp == stack.data() + sizeof(wasm_i32), "byref i31.get_u stack height") ||
           !check(static_cast<::std::uint32_t>(read<wasm_i32>(stack.data())) == ref.ref.storage.wasm_i31.get_u(),
                  "byref i31.get_u value")) { return 1; }
    }

    auto const tail_make{opt::translate::get_uwvmint_ref_i31_fptr<tail, ip_t, sp_t, local_t>(plain_position)};
    auto const tail_signed{opt::translate::get_uwvmint_i31_get_fptr<tail, true, ip_t, sp_t, local_t>(plain_position)};
    opfunc_t const plain_end{end_plain};
    ip = code.data(); sp = stack.data(); write(sp, wasm_i32{-1}); sp += sizeof(wasm_i32);
    write(code.data() + sizeof(opfunc_t), plain_end);
    plain_handler_slot = tail_make;
    seen_count = 0; plain_handler_slot(ip, sp, local);
    if(!check(seen_count == 1u && seen_sp == stack.data() + sizeof(ref_t), "musttail ref.i31 successor")) { return 1; }
    ip = code.data(); sp = seen_sp; seen_count = 0;
    plain_handler_slot = tail_signed; plain_handler_slot(ip, sp, local);
    if(!check(seen_count == 1u && seen_sp == stack.data() + sizeof(wasm_i32) &&
              read<wasm_i32>(stack.data()) == -1, "musttail i31.get_s successor")) { return 1; }

    auto const ring_make{opt::translate::get_uwvmint_ref_i31_fptr<ring, ip_t, sp_t, local_t, wasm_i32>(ring_position)};
    auto const ring_signed{opt::translate::get_uwvmint_i31_get_fptr<ring, true, ip_t, sp_t, local_t, wasm_i32>(ring_position)};
    ring_opfunc_t const ring_end{end_ring};
    write(code.data() + sizeof(ring_opfunc_t), ring_end);
    ring_handler_slot = ring_make;
    seen_count = 0; ring_handler_slot(code.data(), stack.data(), stack.data(), -1);
    if(!check(seen_count == 1u && seen_sp == stack.data() + sizeof(ref_t), "ring ref.i31 successor")) { return 1; }
    ring_handler_slot = ring_signed;
    seen_count = 0; ring_handler_slot(code.data(), seen_sp, stack.data(), 0);
    if(!check(seen_count == 1u && seen_sp == stack.data() && seen_ring == -1, "ring i31.get_s successor")) { return 1; }
    ::std::puts("PASS Core 3 i31: byref, musttail, and register-ring handlers");
}
