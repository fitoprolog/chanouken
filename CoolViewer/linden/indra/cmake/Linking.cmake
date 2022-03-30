# -*- cmake -*-
if (LINKING_CMAKE_INCLUDED)
	return()
endif (LINKING_CMAKE_INCLUDED)
set (LINKING_CMAKE_INCLUDED TRUE)

include(00-BuildOptions)

set(ARCH_PREBUILT_DIRS ${LIBS_PREBUILT_DIR}/lib/release)
set(ARCH_PREBUILT_DIRS_RELEASE ${LIBS_PREBUILT_DIR}/lib/release)
set(ARCH_PREBUILT_DIRS_DEBUG ${LIBS_PREBUILT_DIR}/lib/debug)

if (WINDOWS)
	# Under Windows, the Debug and Release/RelWithDebInfo libraries use
	# different link flags, so we can only use the Debug libaries for the Debug
	# release type.
	if ("${CMAKE_BUILD_TYPE}" STREQUAL "DEBUG" OR "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
		# Note: automatically fall back to non-debug lib when a debug lib is absent
		link_directories(${ARCH_PREBUILT_DIRS_DEBUG} ${ARCH_PREBUILT_DIRS})
	else ()
		link_directories(${ARCH_PREBUILT_DIRS})
	endif ()
else (WINDOWS)
	# Under Linux or macOS we use the release pre-built libraries for the
	# Release build only, i.e. the debug libraries will be used for both Debug
	# and RelWithDebInfo builds.
	if ("${CMAKE_BUILD_TYPE}" STREQUAL "RELEASE" OR "${CMAKE_BUILD_TYPE}" STREQUAL "Release")
		link_directories(${ARCH_PREBUILT_DIRS})
	else ()
		# Note: automatically fall back to non-debug lib when a debug lib is absent
		link_directories(${ARCH_PREBUILT_DIRS_DEBUG} ${ARCH_PREBUILT_DIRS})
	endif ()
endif (WINDOWS)

if (LINUX)
	if (NOT USESYSTEMLIBS)
		# Needed for ld to find the link-time dependency (libffi.so.x) for
		# libglib and libgobject
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath-link,${ARCH_PREBUILT_DIRS_RELEASE}")
	endif (NOT USESYSTEMLIBS)

	if (PROTECTSTACK)
		# If the user requested stack protection, then also protect the ELF GOT
		# by making it read-only (with shared functions address resolution
		# performed at startup time).
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,relro -Wl,-z,now -Wl,--as-needed")
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,relro -Wl,-z,now -Wl,--as-needed")
	else (PROTECTSTACK)
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed")
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--as-needed")
	endif (PROTECTSTACK)

	set(DL_LIBRARY dl)
	set(PTHREAD_LIBRARY pthread)
else (LINUX)
	set(DL_LIBRARY "")
	set(PTHREAD_LIBRARY "")
endif (LINUX)

if (WINDOWS)
	# Needed by VS2017 to avoid missing printf/scanf/_iob symbols
	set(LEGACY_STDIO_LIBS legacy_stdio_definitions)

	if (USE_NETBIOS)
		set(NETWORK_LIBS netapi32 iphlpapi)
	else (USE_NETBIOS)
		set(NETWORK_LIBS iphlpapi)
	endif (USE_NETBIOS)

	set(WINDOWS_LIBRARIES
		advapi32
		shell32
		ws2_32
		mswsock
		psapi
		winmm
		${NETWORK_LIBS}
		wldap32
		gdi32
		user32
		dbghelp
		wer
		${LEGACY_STDIO_LIBS}
	)
	# No libcmt and do not bother us with warnings about missing *.pdb files...
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:LIBCMT /IGNORE:4099")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /NODEFAULTLIB:LIBCMT /IGNORE:4099")
else (WINDOWS)
	set(WINDOWS_LIBRARIES "")
endif (WINDOWS)

mark_as_advanced(DL_LIBRARY PTHREAD_LIBRARY WINDOWS_LIBRARIES)
