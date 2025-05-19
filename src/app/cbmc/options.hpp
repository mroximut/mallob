
#pragma once

#include "optionslist.hpp"
#include "util/option.hpp"

// Application-specific program options for SAT solving.
// memberName                               short option name, long option name          default   min  max

OPTION_GROUP(grpAppCbmc, "app/cbmc", "CBMC options")

OPT_STRING(cbmcOptions, "cbmc-opts", "", "", "CBMC options to be passed to the solver")
OPT_STRING(s2f, "s2f", "", "", "file, where the CBMC output will be stored")
OPT_STRING(unwindLoops, "unwind-loops", "", "", "unwind loops")