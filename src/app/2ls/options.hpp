
#pragma once

#include "optionslist.hpp"
#include "util/option.hpp"

// Application-specific program options for SAT solving.
// memberName                               short option name, long option name          default   min  max

OPTION_GROUP(grpApp2ls, "app/2ls", "2LS options")

OPT_STRING(twolsOptions, "2ls-opts", "", "", "2LS options to be passed to the solver")
OPT_STRING(terminationAnalysis, "termination-analysis", "", "", "start two jobs a la sv-comp")