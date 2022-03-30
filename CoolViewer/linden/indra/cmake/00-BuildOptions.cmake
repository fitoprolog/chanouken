# -*- cmake -*-
if (BUILDOPTIONS_CMAKE_INCLUDED)
	return()
endif (BUILDOPTIONS_CMAKE_INCLUDED)
set (BUILDOPTIONS_CMAKE_INCLUDED TRUE)

include(Variables)

###############################################################################
# Build options shared by all viewer components.
###############################################################################

# NOTE: jemalloc cannot currently replace macOS's neither the Visual C++ run-
# time malloc()... So, only Linux can truly benefit from it so far.
if (LINUX)
	# If you want to enable or disable jemalloc in Linux viewer builds, this is
	# the place. Set ON or OFF as desired.
	set(USE_JEMALLOC ON)
endif (LINUX)

###############################################################################

# Change to OFF to disable parallel-hashmap usage (a header-only rewrite of the
# Abseil hash maps) and instead use the (slower but well tested) equivalent
# boost containers. In the event you would get a weird crash involving a phmap
# container, this is the thing to do (and to report, please !).
set(FAST_CONTAINERS ON)

# Change this flag to link the llcommon library as either shared (ON) or static
# (OFF). Snowglobe (v1 and 2) and most v2 viewers used to have it shared; old
# v1 viewers (v1.23.5 and older) as well as the recent, v3/4/5 viewers got it
# statically linked... Up to you !	Note that some of the newest code additions
# in LL's viewer v3 are incompatible with a shared llcommon library but I fixed
# that in the Cool VL Viewer by moving the culprit code out of llcommon and
# into llrender (namely llstaticstringtable.h which caused dllexport/dllimport
# linking errors under Windows) or by keeping the old, compatible code (namely,
# llinstancetracker.h/cpp, which new implementation is plain incompatible with
# a shared library).
# Of course, a shared library allows for smaller binaries (especially for the
# plugins !) and therefore a (marginally) smaller memory consumption... On the
# other hand, function calls are faster when the called function pertains to a
# static library.
set(LLCOMMON_LINK_SHARED OFF)

# Set to OFF if you want do not want to build with FMOD support.
set(BUILD_WITH_FMOD ON)

# Change to ON enable the so-called "fast" texture cache: that cache is bogus
# (it causes random failures to reach the appropriate discard level on large
# textures), not really fast (it actually delays "large" textures loading), and
# only stores rarely ever used, tiny, 256 pixels or less textures (i.e. 16x16
# pixels or such textures). Best kept OFF. Turning it ON would likely cause
# failures to load mini-map terrain textures, for example...
set(ENABLE_FAST_TEXTURE_CACHE OFF)

# Change to ON to build with libcurl v1.64.1 (the last pipelining-compatible
# version, once patched with a one-liner) and OpenSSL v1.1.1. Alas, this curl
# version (and in fact all versions after v7.4x) got a buggy pipelining
# implementation causing "rainbow textures" (especially seen in OpenSim grids,
# but sometimes too in SL, depending on the configuration of the CDN server you
# are hitting).
# The good old libcurl v7.47.0 (sadly using the deprecated OpenSSL v1.0.2) is
# still, for now, the only safe and reliable choice. :-(
set(USE_NEW_LIBCURL OFF)

# Set to ON to use an older CEF version that does not require SSE3.
set(USE_OLD_CEF OFF)

# Set to OFF to build with libSDL1 instead of libSDL2 under Linux.
set(USE_SDL2 ON)

# Set to ON to use the "meshoptimizer" library for optimizing meshes: this is
# known to cause issues with some (badly designed) mesh hair models... With
# this setting OFF, LL's (partially fixed) algorithm will be used, which sadly
# does not always provide the expected results under Linux (it relies on a STL
# container idiosyncrasy in VS2017 to give proper results under Windows).
set(USE_MESHOPTIMIZER OFF)

# Set to ON to replace fast timers with Tracy. This results in a slightly
# slower (~1%) viewer. Use for development builds only. NOTE: this may be
# enabled at configuration time instead by passing -DTRACY:BOOL=TRUE to cmake.
set(USE_TRACY OFF)
# Set to OFF to disable memory usage profiling when Tracy is enabled. Note that
# only allocations done via the viewer custom allocators are actually logged
# (which represents only part of the total used memory). 
set(TRACY_MEMORY ON)
# Set to ON to keep fast timers along Tracy's when the latter is enabled; the
# resulting binary is then slightly slower, of course.
set(TRACY_WITH_FAST_TIMERS OFF)

# Set to ON to enable Animesh visual params support (Muscadine project).
# Experimental and only supported in the Animesh* sims on the SL Aditi grid.
set(ENABLE_ANIMESH_VISUAL_PARAMS OFF)

# Set to OFF to do away with the netapi32 DLL (Netbios) dependency in Windows
# builds; sadly, this causes the MAC address to change, invalidating all saved
# login passwords...
set(USE_NETBIOS ON)

# Compilation/optimization options: uncomment to enable (may also be passed as
# boolean defines to cmake: see Variables.cmake). Mainly relevant to Windows
# and macOS builds (for Linux, simply use the corresponding options in the
# linux-build.sh script, which will pass the appropriate boolean defines to
# cmake).
#set(USELTO ON)
#set(USEAVX ON)
#set(USEAVX2 ON)
# Please note that the current OpenMP optimizations are totally experimental,
# insufficiently tested, and may result in crashes !
#set(OPENMP ON)
# Set to use cmake v3.16.0+ UNITY_BUILD feature to speed-up the compilation
# (experimental and resulting binaries are untested).
#set(USEUNITYBUILD ON)

###############################################################################
# SELECTION LOGIC: DO NOT EDIT UNLESS YOU KNOW EXACTLY WHAT YOU ARE DOING !
###############################################################################

# Select audio backend(s):
if (BUILD_WITH_FMOD AND ARCH STREQUAL "arm64")
	# No FMOD Studio package available for arm64...
	set(BUILD_WITH_FMOD OFF)
endif (BUILD_WITH_FMOD AND ARCH STREQUAL "arm64")
if (BUILD_WITH_FMOD)
	if (LINUX)
		set(FMOD ON)
		set(OPENAL ON)
	else (LINUX)
		set(FMOD ON)
		set(OPENAL OFF)
	endif (LINUX)
else (BUILD_WITH_FMOD)
	set(FMOD OFF)
	set(OPENAL ON)
endif (BUILD_WITH_FMOD)

if (DARWIN)
	# The Dullahan plugin based on CEF after v74 do not work for now under
	# Darwinchanges are needed to viewer_manifest.py, dealing with
	# SLPlugin.app).
	set(USE_OLD_CEF ON)

	# OpenMP support is missing from Apple's llvm/clang...
	set(OPENMP OFF)
endif (DARWIN)

if (TRACY)
	set(USE_TRACY ON)
endif (TRACY)
