# -*- cmake -*-
if (IOURING_CMAKE_INCLUDED)
  return()
endif (IOURING_CMAKE_INCLUDED)
set (IOURING_CMAKE_INCLUDED TRUE)

# No io_uring support for macOS and native support for Windows.
# Note: Linux arm64 also lacks the liburing pre-built-library for now...
if (NOT LINUX)
	return()
endif (NOT LINUX)

include(00-BuildOptions)
include(Variables)

if (USESYSTEMLIBS OR ARCH STREQUAL "arm64")
	include(FindPkgConfig)
	pkg_check_modules(LIBURING liburing)
endif (USESYSTEMLIBS OR ARCH STREQUAL "arm64")
if (NOT LIBURING_FOUND)
	if (ARCH STREQUAL "arm64")
		message(FATAL_ERROR "No pre-built library available (for now) for liburing on arm64, please install liburing-devel !")
	endif (ARCH STREQUAL "arm64")
	include(Prebuilt)
	use_prebuilt_binary(liburing)
	set(LIBURING_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include)
	set(LIBURING_LIBRARIES ${LIBS_PREBUILT_DIR}/lib/release/liburing.a)
endif (NOT LIBURING_FOUND)
