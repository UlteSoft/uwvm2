#pragma once

/**
 * @file
 * @brief Preserves the compatibility include for stream-based transcoders.
 *
 * The former printable `manipulators::transcode` carrier was intentionally
 * removed. Transcoders are stream adapters; their core engine protocol is
 * available through `impl.h`.
 */

#include "impl.h"
