# -*- cmake -*-
if (CEFPLUGIN_CMAKE_INCLUDED)
  return()
endif (CEFPLUGIN_CMAKE_INCLUDED)
set (CEFPLUGIN_CMAKE_INCLUDED TRUE)

include(00-BuildOptions)
include(Linking)
include(Prebuilt)

if (USE_OLD_CEF)
  use_prebuilt_binary(dullahan-old)
else ()
  use_prebuilt_binary(dullahan)
endif ()

set(CEF_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include/cef)

if (DARWIN)
  FIND_LIBRARY(APPKIT_LIBRARY AppKit)
  if (NOT APPKIT_LIBRARY)
    message(FATAL_ERROR "AppKit not found")
  endif()

  set(CEF_LIBRARY "${ARCH_PREBUILT_DIRS_RELEASE}/Chromium Embedded Framework.framework/Chromium Embedded Framework")

  set(CEF_PLUGIN_LIBRARIES
      ${ARCH_PREBUILT_DIRS_RELEASE}/libcef_dll_wrapper.a
      ${ARCH_PREBUILT_DIRS_RELEASE}/libdullahan.a
      ${APPKIT_LIBRARY}
      ${CEF_LIBRARY}
     )
elseif (LINUX)
  set(CEF_PLUGIN_LIBRARIES
      -Wl,-whole-archive
      ${ARCH_PREBUILT_DIRS_RELEASE}/libcef_dll_wrapper.a
      ${ARCH_PREBUILT_DIRS_RELEASE}/libdullahan.a
      -Wl,-no-whole-archive
      ${ARCH_PREBUILT_DIRS_RELEASE}/libcef.so
  )
elseif (WINDOWS)
  set(CEF_PLUGIN_LIBRARIES
      libcef.lib
      libcef_dll_wrapper.lib
      dullahan.lib
  )
endif ()
