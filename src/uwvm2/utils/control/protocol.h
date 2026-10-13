/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <fast_io_dsal/array.h>
# include <cstddef>
# include <cstdint>
# include <limits>
# include <fast_io.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::utils::control
{
    enum class error
    {
        none, disabled, unsupported_backend, unsupported_mode, invalid_launch,
        unauthorized, same_process, wrong_peer, busy, closed, revoked,
        malformed, unsupported_version, unsupported_command, oversized,
        truncated, wrong_instance, stale_generation, replay, exhausted,
        unavailable_capability, invalid_state, wrong_completion, would_block,
        transport_failure, unsupported_transport, unexpected_ancillary, launcher_exited,
        invalid_memory_range, invalid_replacement_source, invalid_replacement_target,
        stale_function_generation, invalid_function_body, function_abi_mismatch,
        replacement_compile_failure, active_replacement_frame,
        source_debug_info_unavailable, source_debug_info_invalid,
        source_location_unmapped, source_step_policy_unavailable,
        invalid_breakpoint_target, breakpoint_not_executable, wasm_step_policy_unavailable
    };
    enum class operation : ::std::uint16_t
    {
        status = 1, pause, resume, step, read_memory, replace_function, detach, breakpoint_set, breakpoint_clear, breakpoint_list, backtrace, locals
    };
    using wire_byte = unsigned char;
    using input_buffer = ::fast_io::basic_ibuffer_view<wire_byte>;
    using output_buffer = ::fast_io::basic_obuffer_view<wire_byte>;
    using instance_id = ::fast_io::array<wire_byte, 16>;
    inline constexpr ::std::size_t header_bytes{48};
    inline constexpr ::std::size_t max_function_body_bytes{65536};
    inline constexpr ::std::size_t max_payload_bytes{32 + max_function_body_bytes};
    inline constexpr ::std::size_t max_frame_bytes{header_bytes + max_payload_bytes};
    inline constexpr ::std::uint32_t max_memory_read_bytes{65536};

    struct frame_header
    {
        operation command{};
        ::std::uint32_t payload_bytes{};
        instance_id instance{};
        ::std::uint64_t generation{};
        ::std::uint64_t request_id{};
    };
    // [one live host-owned byte range: curr_ptr ... end_ptr]
    // [safe                                                ]
    // ^^ Both cursors belong to that same range. Views are native borrows;
    // decoded wire integers never become either cursor. Empty default views
    // have equal null cursors, so their pointers are never subtracted.
    [[nodiscard]] inline constexpr ::std::size_t remaining_bytes(input_buffer const& input) noexcept
    { return input.curr_ptr == input.end_ptr ? 0u : static_cast<::std::size_t>(input.end_ptr - input.curr_ptr); }
    [[nodiscard]] inline constexpr ::std::size_t remaining_bytes(output_buffer const& output) noexcept
    { return output.curr_ptr == output.end_ptr ? 0u : static_cast<::std::size_t>(output.end_ptr - output.curr_ptr); }

    namespace details
    {
        inline constexpr ::std::uint32_t frame_magic{0x31435755u}; // UWC1 on the wire.
        [[nodiscard]] constexpr bool known(operation command) noexcept
        { return command >= operation::status && command <= operation::locals; }
        [[nodiscard]] constexpr error decode_header(input_buffer input, frame_header& out) noexcept
        {
            if(remaining_bytes(input) < header_bytes) { return error::truncated; }
            ::std::uint32_t magic{}, reserved{};
            ::std::uint16_t version{}, command{};
            // [16-byte fixed prefix][16-byte instance][16-byte suffix] end
            // [safe                                                 ]
            // ^^ input.curr_ptr: the 48-byte check proves this complete read.
            // The unsigned carriers cover all values; no decimal/LE overflow.
            ::fast_io::io::scan(input, ::fast_io::mnp::le_get<32>(magic), ::fast_io::mnp::le_get<16>(version),
                               ::fast_io::mnp::le_get<16>(command), ::fast_io::mnp::le_get<32>(out.payload_bytes),
                               ::fast_io::mnp::le_get<32>(reserved));
            // [consumed prefix][instance][suffix] end
            // [safe                                 ]
            //                  ^^ input.curr_ptr advanced exactly 16 bytes.
            if(magic != frame_magic) { return error::malformed; }
            if(version != 1u) { return error::unsupported_version; }
            if(reserved != 0u) { return error::malformed; }
            out.command = static_cast<operation>(command);
            if(!known(out.command)) { return error::unsupported_command; }
            if(out.payload_bytes > max_payload_bytes) { return error::oversized; }
            // [consumed prefix][16-byte instance][16-byte suffix] end
            // [safe                                                  ]
            //                  ^^ input.curr_ptr; the destination is the entire
            // owned 16-byte array. Its end is one-past, never dereferenced.
            ::fast_io::operations::read_all(input, out.instance.data(), out.instance.data() + out.instance.size());
            // [consumed prefix and instance][16-byte suffix] end
            // [safe                                               ]
            //                               ^^ input.curr_ptr moved by 16.
            ::fast_io::io::scan(input, ::fast_io::mnp::le_get<64>(out.generation), ::fast_io::mnp::le_get<64>(out.request_id));
            // [48 consumed header bytes] end [possible remaining bytes]
            // [safe                                                    ]
            //                            ^^ input.curr_ptr; a header-only view
            // is now one-past. No byte is read at the resulting cursor.
            if(out.generation == 0u || out.request_id == 0u) { return error::malformed; }
            return error::none;
        }
    }

    // One bounded frame per read. Additional bytes remain with the transport;
    // ready() must be consumed before feeding another frame. No length from the
    // wire controls allocation. A parse error is sticky until the owner resets.
    class frame_decoder
    {
        ::fast_io::array<wire_byte, max_frame_bytes> storage{};
        ::std::size_t used{}, target{header_bytes};
        frame_header decoded{};
        error failure{};
    public:
        struct feed_result { error status; ::std::size_t consumed; bool complete; };
        [[nodiscard]] bool ready() const noexcept { return failure == error::none && used == target && used >= header_bytes; }
        [[nodiscard]] frame_header const& header() const noexcept { return decoded; }
        [[nodiscard]] input_buffer payload() const noexcept
        {
            if(!ready()) { return {}; }
            // [owned storage: complete header][complete payload] ... capacity
            // [safe                                                         ]
            //                                 ^^ new curr_ptr      ^^ end_ptr
            // ready() proves header_bytes <= used == target <= max_frame_bytes.
            // The returned view borrows this decoder until reset/destruction.
            return {storage.data() + header_bytes, storage.data() + used};
        }
        // The view is borrowed by value; consumed identifies the caller's next byte.
        [[nodiscard]] feed_result feed(input_buffer input) noexcept
        {
            if(failure != error::none) { return {failure, 0u, false}; }
            if(ready()) { return {error::busy, 0u, true}; }
            ::std::size_t consumed{};
            while(input.curr_ptr != input.end_ptr && used != target)
            {
                auto const available{remaining_bytes(input)};
                auto const needed{target - used};
                auto const count{available < needed ? available : needed};
                // [native input: count available bytes ...] input.end_ptr
                // [safe                                              ]
                // ^^ input.curr_ptr: count <= available. The destination starts
                // at storage+used; used+count <= target <= storage.size(). Both
                // destination endpoints are inside that one owning array.
                ::fast_io::operations::read_all(input, storage.data() + used, storage.data() + used + count);
                // [consumed count][remaining native bytes ...] input.end_ptr
                // [safe                                                 ]
                //                 ^^ input.curr_ptr advanced by count, at most
                // to one-past. A complete frame stops further input consumption.
                used += count;
                consumed += count;
                if(used == header_bytes)
                {
                    // [owned storage: 48-byte header][remaining capacity]
                    // [safe                                            ]
                    // ^^ new view starts at storage; end is storage+48 <= used.
                    failure = details::decode_header(input_buffer{storage.data(), storage.data() + header_bytes}, decoded);
                    if(failure != error::none) { return {failure, consumed, false}; }
                    target = header_bytes + decoded.payload_bytes;
                }
            }
            return {error::none, consumed, ready()};
        }
        [[nodiscard]] error finish_stream() noexcept
        {
            if(failure != error::none) { return failure; }
            if(used != 0u && !ready()) { failure = error::truncated; }
            return failure;
        }
        void reset() noexcept { used = 0u; target = header_bytes; decoded = {}; failure = error::none; }
    };

    struct encode_result { error status; ::std::size_t written; };
    [[nodiscard]] inline constexpr encode_result encode_frame(frame_header const& header,
                                                               input_buffer payload, output_buffer& output) noexcept
    {
        auto const payload_size{remaining_bytes(payload)};
        if(payload_size > max_payload_bytes) { return {error::oversized, 0u}; }
        if(!details::known(header.command)) { return {error::unsupported_command, 0u}; }
        if(header.generation == 0u || header.request_id == 0u || header.payload_bytes != payload_size)
        { return {error::malformed, 0u}; }
        auto const total{header_bytes + payload_size};
        // No output cursor or byte changes until the entire frame fits.
        if(remaining_bytes(output) < total) { return {error::truncated, 0u}; }
        // [48 writable header bytes][payload_size writable bytes ...] end
        // [safe                                                       ]
        // ^^ output.curr_ptr: total <= remaining_bytes(output). The instance
        // endpoints borrow exactly its owned 16-byte array for this call.
        ::fast_io::io::print(output, ::fast_io::mnp::le_put<32>(details::frame_magic), ::fast_io::mnp::le_put<16>(::std::uint16_t{1}),
                            ::fast_io::mnp::le_put<16>(static_cast<::std::uint16_t>(header.command)),
                            ::fast_io::mnp::le_put<32>(header.payload_bytes), ::fast_io::mnp::le_put<32>(::std::uint32_t{}),
                            ::fast_io::mnp::strvw(header.instance.data(), header.instance.data() + header.instance.size()),
                            ::fast_io::mnp::le_put<64>(header.generation), ::fast_io::mnp::le_put<64>(header.request_id));
        // [48 emitted header bytes][payload_size writable bytes ...] end
        // [safe                                                       ]
        //                           ^^ output.curr_ptr advanced exactly 48.
        // payload is a native live borrow, never a pointer decoded from wire.
        if(payload_size != 0u) { ::fast_io::io::print(output, ::fast_io::mnp::strvw(payload.curr_ptr, payload.end_ptr)); }
        // [header and payload emitted: total bytes][remaining capacity] end
        // [safe                                                          ]
        //                                          ^^ output.curr_ptr is at
        // most one-past. The input payload view itself has not been advanced.
        return {error::none, total};
    }
}
