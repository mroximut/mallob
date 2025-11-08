
# Add CBMC-specific sources to main Mallob executable
set(CBMC_MALLOB_SOURCES src/app/sat/solvers/cadical.cpp src/app/sat/solvers/portfolio_solver_interface.cpp)
set(MALLOB_COREPLUSCOMM_SOURCES ${MALLOB_COREPLUSCOMM_SOURCES} ${CBMC_MALLOB_SOURCES} CACHE INTERNAL "")

#message("commons+SAT sources: ${BASE_SOURCES}") # Use to debug

# Include external libraries as necessary
set(BASE_LINK_DIRS ${BASE_LINK_DIRS} lib/cbmc/src/cbmc CACHE INTERNAL "")
set(BASE_LIBS ${BASE_LIBS} cbmc CACHE INTERNAL "")
set(BASE_INCLUDES ${BASE_INCLUDES} lib/cbmc/src CACHE INTERNAL "")
