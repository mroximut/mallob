
# Add CBMC-specific sources to main Mallob executable
set(2LS_MALLOB_SOURCES)
set(MALLOB_COREPLUSCOMM_SOURCES ${MALLOB_COREPLUSCOMM_SOURCES} ${2LS_MALLOB_SOURCES} CACHE INTERNAL "")

#message("commons+SAT sources: ${BASE_SOURCES}") # Use to debug

# Include external libraries as necessary
set(BASE_LINK_DIRS ${BASE_LINK_DIRS} lib/2ls/src/2ls CACHE INTERNAL "")
set(BASE_LIBS ${BASE_LIBS} 2ls CACHE INTERNAL "")
set(BASE_INCLUDES ${BASE_INCLUDES} lib/2ls/src lib/2ls/lib/cbmc/src CACHE INTERNAL "")
