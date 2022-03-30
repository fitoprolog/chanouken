# -*- cmake -*-
if (LLCOMMON_CMAKE_INCLUDED)
  return()
endif (LLCOMMON_CMAKE_INCLUDED)
set (LLCOMMON_CMAKE_INCLUDED TRUE)

include(APR)
include(Boost)
include(EXPAT)
include(Tracy)
include(ZLIB)

set(LLCOMMON_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/llcommon
    ${APRUTIL_INCLUDE_DIR}
    ${APR_INCLUDE_DIR}
    ${TRACY_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
    )

if (LINUX)
  # In order to support using ld.gold on linux, we need to explicitely
  # specify all libraries that llcommon uses.
  # llcommon uses `clock_gettime' which is provided by librt on linux.
  if (LLCOMMON_LINK_SHARED)
    set(LLCOMMON_LIBRARIES llcommon
        ${BOOST_FILESYSTEM_LIBRARY}
        ${BOOST_PROGRAM_OPTIONS_LIBRARY}
        ${BOOST_FIBER_LIBRARY}
        ${BOOST_CONTEXT_LIBRARY}
        ${BOOST_THREAD_LIBRARY}
        ${BOOST_CHRONO_LIBRARY}
        ${BOOST_SYSTEM_LIBRARY}
        ${BOOST_ATOMIC_LIBRARY}
        rt
       )
  else (LLCOMMON_LINK_SHARED)
    set(LLCOMMON_LIBRARIES llcommon rt)
  endif (LLCOMMON_LINK_SHARED)
else (LINUX)
  if (LLCOMMON_LINK_SHARED)
    set(LLCOMMON_LIBRARIES llcommon
        ${BOOST_FILESYSTEM_LIBRARY}
        ${BOOST_PROGRAM_OPTIONS_LIBRARY}
        ${BOOST_FIBER_LIBRARY}
        ${BOOST_CONTEXT_LIBRARY}
        ${BOOST_THREAD_LIBRARY}
        ${BOOST_CHRONO_LIBRARY}
        ${BOOST_SYSTEM_LIBRARY}
        ${BOOST_ATOMIC_LIBRARY}
       )
  else (LLCOMMON_LINK_SHARED)
    set(LLCOMMON_LIBRARIES llcommon)
  endif (LLCOMMON_LINK_SHARED)
endif (LINUX)

if (LLCOMMON_LINK_SHARED)
  add_definitions(-DLL_COMMON_LINK_SHARED=1)
endif (LLCOMMON_LINK_SHARED)
