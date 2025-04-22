
# Add CBMC-specific sources to main Mallob executable
set(CBMC_MALLOB_SOURCES)
set(MALLOB_COREPLUSCOMM_SOURCES ${MALLOB_COREPLUSCOMM_SOURCES} ${CBMC_MALLOB_SOURCES} CACHE INTERNAL "")

#message("commons+SAT sources: ${BASE_SOURCES}") # Use to debug

# Include external libraries as necessary
set(BASE_LINK_DIRS ${BASE_LINK_DIRS} lib/cbmc/src/libcprover-cpp lib/cbmc/src/cbmc CACHE INTERNAL "")
set(BASE_LIBS ${BASE_LIBS} cprover-cpp cbmc CACHE INTERNAL "")
set(BASE_INCLUDES ${BASE_INCLUDES} lib/cbmc/src CACHE INTERNAL "")