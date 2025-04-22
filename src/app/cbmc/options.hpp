
#pragma once

#include "optionslist.hpp"
#include "util/option.hpp"

// Application-specific program options for SAT solving.
// memberName                               short option name, long option name          default   min  max

OPTION_GROUP(grpAppCbmc, "app/cbmc", "CBMC options")

OPT_STRING(cbmcOptions, "cbmc-opts", "", "", "CBMC options to be passed to the solver")
