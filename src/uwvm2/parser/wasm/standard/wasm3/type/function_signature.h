/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include "recursive_type.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm3::type
{
    // Full declared types and their currently executable carrier projection have separate storage.
    // In particular, a typed or non-null reference MUST NOT become a nullable carrier implicitly.
    template<typename Carrier>
    struct owned_function_signature
    {
        ::std::size_t type_index{};
        bool requires_function_references{};
        bool requires_exceptions{};
        bool requires_simd{}, requires_reference_types{}, requires_multi_value{};
        ::uwvm2::utils::container::vector<core_value_type> parameters{}, results{};
        // One trailing sentinel keeps empty begin/end views within a live allocation. It is not a Wasm type.
        ::uwvm2::utils::container::vector<Carrier> carriers{};
    };

    template<typename Carrier, typename FunctionType>
    inline constexpr void bind_owned_function_signature(owned_function_signature<Carrier> const& signature,
                                                         FunctionType& function) noexcept
    {
        // [parameters][results][sentinel]
        // [safe                        ] the parser publishes only a complete projection with its sentinel.
        // ^^ base borrows a vector owned by the same section; rebinding is also required after a deep copy.
        auto const base{signature.carriers.cbegin()};
        function.parameter.begin = base;
        // [parameters][results][sentinel]
        // [safe                        ] parameter count <= carriers.size() - 1; the endpoint is live.
        //              ^^ parameter.end / result.begin
        function.parameter.end = base + signature.parameters.size();
        function.result.begin = function.parameter.end;
        // [parameters][results][sentinel]
        // [safe                        ] both counts were checked before constructing carriers.
        //                       ^^ result.end: points at sentinel, without reading it.
        function.result.end = function.result.begin + signature.results.size();
    }
}
