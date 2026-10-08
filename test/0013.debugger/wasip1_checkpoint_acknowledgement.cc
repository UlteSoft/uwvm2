/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)           *
 * Copyright (c) 2025-present UlteSoft. All rights reserved.   *
 * Licensed under the APL-2.0 License (see LICENSE file).      *
 *************************************************************/
// Detached native formatter packets for DAP interoperability tests. No VM
// authority, checkpoint mutation, host handle or live environment is involved.
#include <uwvm2/uwvm/debugger/wasip1_state.h>
#include <fast_io_unit/string.h>
namespace ws = ::uwvm2::uwvm::debugger::wasip1_state;
int main(int argc, char const**)
{
    // The cross-OS runner supplies a private working directory, even though
    // this DATA fixture uses only FastIO strings and standard output.
    if(argc != 1 && argc != 2) { return 91; }
    unsigned checks{};
    for(unsigned slot{}; slot != 8u; ++slot)
    {
        for(unsigned variant{}; variant != 5u; ++variant)
        {
            ws::view data{};
            data.result = variant < 3u ? ws::status::ok :
                variant == 3u ? ws::status::entry_not_found : ws::status::unavailable_resource_rollback;
            data.mutation_applied = variant < 3u;
            data.shared_environment = true;
            data.module = 0u; data.observed_runtime_epoch = UINT64_MAX;
            data.checkpoint_operation = true; data.checkpoint_slot = slot;
            data.managed_resources = variant == 2u ? 32768u : variant == 0u ? 0u : 1u;
            data.retained_external_resources = variant == 2u ? 32768u : variant == 0u ? 0u : 3u;
            ::fast_io::string packet{};
            ws::print(::fast_io::ostring_ref_fast_io{__builtin_addressof(packet)}, data);
            auto expected{::fast_io::concat_fast_io(
                "wasip1 module=0 status=", ws::status_text(data.result),
                " epoch=18446744073709551615 applied=", data.mutation_applied ? 1u : 0u,
                " shared-environment=1 total=0\nwasip1-checkpoint slot=", slot,
                " managed=", data.managed_resources, " retained-external=", data.retained_external_resources,
                " external-io-rollback=false\n"
                "reminder: checkpoint Wasm and WASIp1 together at the same cooperative stop; WASIp1-only restore does not restore Wasm state.\n")};
            ++checks;
            if(packet != expected) { ::fast_io::io::perrln("checkpoint formatter mismatch slot=", slot, " variant=", variant); return 1; }
            ::fast_io::string hex{};
            hex.reserve(packet.size() * 2u);
            constexpr char digits[]{"0123456789abcdef"};
            for(char c : packet)
            {
                auto const byte{static_cast<unsigned char>(c)};
                hex.push_back(digits[byte >> 4u]); hex.push_back(digits[byte & 15u]);
            }
            ::fast_io::io::println("PACKET slot=", slot, " variant=", variant, " hex=", hex);
        }
    }
    ::fast_io::io::println("wasip1_checkpoint_acknowledgement ", checks, " checks passed cases=1 unsupported=0 phase=post");
}
