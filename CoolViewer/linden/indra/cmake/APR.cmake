# -*- cmake -*-
if (APR_CMAKE_INCLUDED)
  return()
endif (APR_CMAKE_INCLUDED)
set (APR_CMAKE_INCLUDED TRUE)

include(Linking)
include(Prebuilt)

if (USESYSTEMLIBS)
  set(APR_FIND_QUIETLY ON)
  set(APR_FIND_REQUIRED OFF)
  set(APRUTIL_FIND_QUIETLY ON)
  set(APRUTIL_FIND_REQUIRED OFF)
  include(FindAPR)
endif (USESYSTEMLIBS)

if (NOT APR_FOUND OR NOT APRUTIL_FOUND)
  use_prebuilt_binary(apr_suite)
  if (WINDOWS)
    if (LLCOMMON_LINK_SHARED)
      set(APR_selector "lib")
    else (LLCOMMON_LINK_SHARED)
      add_definitions(-DAPR_DECLARE_STATIC -DAPU_DECLARE_STATIC)
      set(APR_selector "")
    endif (LLCOMMON_LINK_SHARED)
    set(APR_LIBRARIES ${ARCH_PREBUILT_DIRS_RELEASE}/${APR_selector}apr-1.lib)
    set(APRICONV_LIBRARIES ${ARCH_PREBUILT_DIRS_RELEASE}/${APR_selector}apriconv-1.lib)
    set(APRUTIL_LIBRARIES  ${ARCH_PREBUILT_DIRS_RELEASE}/${APR_selector}aprutil-1.lib ${APRICONV_LIBRARIES})
  elseif (DARWIN)
     if (LLCOMMON_LINK_SHARED)
      set(APR_selector     "0.dylib")
      set(APRUTIL_selector "0.dylib")
    else (LLCOMMON_LINK_SHARED)
      set(APR_selector     "a")
      set(APRUTIL_selector "a")
    endif (LLCOMMON_LINK_SHARED)
   set(APR_LIBRARIES ${ARCH_PREBUILT_DIRS_RELEASE}/libapr-1.${APRUTIL_selector})
    set(APRUTIL_LIBRARIES ${ARCH_PREBUILT_DIRS_RELEASE}/libaprutil-1.${APRUTIL_selector})
    set(APRICONV_LIBRARIES iconv)
  else (WINDOWS)
    set(APR_LIBRARIES apr-1)
    set(APRUTIL_LIBRARIES aprutil-1)
    set(APRICONV_LIBRARIES iconv)
  endif (WINDOWS)
  set(APR_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include/apr-1)
  set(APRUTIL_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include/apr-1)

  if (LINUX)
    list(APPEND APRUTIL_LIBRARIES uuid)
    list(APPEND APRUTIL_LIBRARIES rt)
  endif (LINUX)
endif (NOT APR_FOUND OR NOT APRUTIL_FOUND)