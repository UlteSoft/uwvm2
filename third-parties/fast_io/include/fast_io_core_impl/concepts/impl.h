#pragma once

/*
 * Aggregation boundary for the core CPO/protocol vocabulary.
 *
 * The included files define common protocol carriers, parse results, private
 * recognition helpers, value print/scan capabilities, strlike destination
 * capabilities, and decorator capabilities. This layer describes what a
 * provider can do and which semantic promises it makes; operation-level
 * dispatch is implemented by print, scan, concat, read, and write engines.
 */

#include "type.h"
#include "parse_code.h"
#include "details.h"
#include "operation_details.h"
#include "operation.h"
#include "strlike.h"
#include "decorator.h"
