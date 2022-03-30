# -*- cmake -*-
if (JEMALLOC_CMAKE_INCLUDED)
  return()
endif (JEMALLOC_CMAKE_INCLUDED)
set (JEMALLOC_CMAKE_INCLUDED TRUE)

if (NOT USE_JEMALLOC)
  return()
endif ()

include(Prebuilt)
use_prebuilt_binary(jemalloc)

set(JEMALLOC_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include/jemalloc)
include_directories(${JEMALLOC_INCLUDE_DIR})
set(JEMALLOC_LIBRARY jemalloc)
