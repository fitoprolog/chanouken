# -*- cmake -*-
if (COMMON_CMAKE_INCLUDED)
	return()
endif (COMMON_CMAKE_INCLUDED)
set (COMMON_CMAKE_INCLUDED TRUE)

# Include compilation options
include(00-BuildOptions)

# This is for cmake v3.11.1 and newer:
set(OpenGL_GL_PREFERENCE "LEGACY")

# Platform-specific compilation flags.

if (DARWIN)
	# Supported build types (only Release for macOS):
	set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING
		"Supported build types." FORCE)

	# Specify the C and C++ standard in use for clang
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu89")
	# Use C++14. Also, libc++ must now be used instead of libstdc++ on Macs...
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -stdlib=libc++")
	set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")

	# OpenMP support is missing from Apple's llvm/clang...
	#if (OPENMP)
	#	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")
	#endif (OPENMP)

	# Never add thread-locking code to local static variables (we never use
	# non-thread_local static local variables in thread-sensitive code):
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-threadsafe-statics")

	# Disable some clang warnings triggered because of boost or even stdlib (!) headers
	set(GCC_CXX_WARNINGS "-Wno-deprecated-declarations -Wno-tautological-overlap-compare")

	# Vectorized maths selection
	if (ARCH STREQUAL "arm64")
		# We need a conversion header to get the SSE2 code translated into its
		# NEON counterpart (thanks to sse2neon.h).
		add_definitions(-DSSE2NEON=1)
		# If not already manualy specified, add the necessary -march tunes for
		# sse2neon.h to take full effect. See the "Usage" section at:
		# https://github.com/DLTcollab/sse2neon/blob/master/README.md
		set(TUNED_BUILD ON)
		if (NOT "${CMAKE_C_FLAGS}" MATCHES ".*-march.*")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a+fp+simd")
		endif ()
		if (NOT "${CMAKE_CXX_FLAGS}" MATCHES ".*-march=.*")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a+fp+simd")
		endif ()
		# Do not mess up with aligment optimizations that would be irrelevant
		# for arm64...
		set(TUNED_BUILD ON)
	else (ARCH STREQUAL "arm64")
		# We only support 64 bits builds for x86 targets
		set(ARCH_FLAG "-m64")
		# SSE2 is always enabled by default for 64 bits targets
		set(COMPILER_MATHS "-mfpmath=sse")
		if (USEAVX2)
			set(COMPILER_MATHS "-mavx2 -mfpmath=sse")
		elseif (USEAVX)
			set(COMPILER_MATHS "-mavx -mfpmath=sse")
		endif ()
	endif (ARCH STREQUAL "arm64")

	# Compilation flags common to all build types and compilers
	# NOTE: do not use -ffast-math: it makes us loose the Moon (!) and probably
	# causes other issues as well...
	# Please, note that -fno-strict-aliasing is *required*, because the code is
	# not strict aliasing safe and omitting this option would cause warnings and
	# bogus optimizations by gcc.
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pipe -g -fexceptions -fno-strict-aliasing -fvisibility=hidden -fsigned-char ${ARCH_FLAG} ${COMPILER_MATHS} -fno-math-errno -fno-trapping-math -pthread")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pipe -g -fexceptions -fno-strict-aliasing -fsigned-char ${ARCH_FLAG} ${COMPILER_MATHS} -fno-math-errno -fno-trapping-math -pthread")

	if (PROTECTSTACK)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector")
	else (PROTECTSTACK)
		# We do not care for protecting the stack with canaries. This option
		# provides some (small) speed and caches usage gains.
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-stack-protector")
	endif (PROTECTSTACK)

	# XCode 9 clang-llvm got a totally broken optimizer at -O3, resulting in
	# viewer crashes. So let's use -O2.
	set(OPT_FLAGS "-O2 -fno-delete-null-pointer-checks -mno-retpoline -mno-retpoline-external-thunk")

	add_definitions(-DLL_DARWIN=1)
	set(CMAKE_CXX_LINK_FLAGS "-Wl,-headerpad_max_install_names,-search_paths_first")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_CXX_LINK_FLAGS}")
	# NOTE: it is critical to have both CXX_FLAGS and C_FLAGS covered.
	set(CMAKE_C_FLAGS_DEBUG "-fno-inline -D_DEBUG -DLL_DEBUG=1")
	set(CMAKE_CXX_FLAGS_DEBUG "-fno-inline -D_DEBUG -DLL_DEBUG=1")
	set(CMAKE_C_FLAGS_RELWITHDEBINFO "-fno-inline -DLL_NO_FORCE_INLINE=1 -DNDEBUG")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-fno-inline -DLL_NO_FORCE_INLINE=1 -DNDEBUG")
	set(CMAKE_C_FLAGS_RELEASE "${OPT_FLAGS} -DNDEBUG")
	set(CMAKE_CXX_FLAGS_RELEASE "${OPT_FLAGS} -DNDEBUG")
endif (DARWIN)

if (LINUX)
	# Supported build types:
	set(CMAKE_CONFIGURATION_TYPES "Release;RelWithDebInfo;Debug" CACHE STRING
		"Supported build types." FORCE)

	set(CMAKE_SKIP_RPATH TRUE)

	if (${CLANG_VERSION} LESS 340 AND ${CXX_VERSION} LESS 500)
		message(FATAL_ERROR "Cannot compile any more with gcc below version 5.0 or clang below version 3.4, sorry...")
	endif ()

	# Compiler-version-specific warnings disabling:
	set(GCC_WARNINGS "")
	set(GCC_CXX_WARNINGS "")
	if (${CLANG_VERSION} GREATER 0)
		# Disable some clang warnings triggered because of boost headers
		set(GCC_CXX_WARNINGS "-Wno-tautological-overlap-compare -Wno-unknown-warning-option")
	endif ()

	# Debugging information generation options
	set(GCC_DEBUG_OPTIONS "-g -gdwarf -gstrict-dwarf")
	if (NOT ${CLANG_VERSION} GREATER 0)
		set(GCC_DEBUG_OPTIONS "${GCC_DEBUG_OPTIONS} -fno-var-tracking-assignments")
	endif ()

	# Specify the C standard in use.
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu89")

	# Specify the C++ standard in use
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")

	if (PROTECTSTACK)
		# Also enable "safe" glibc functions variants usage (_FORTIFY_SOURCE)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector -D_FORTIFY_SOURCE=2")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector -D_FORTIFY_SOURCE=2")
	else (PROTECTSTACK)
		# We do not care for protecting the stack with canaries. This option
		# provides some (small) speed and caches usage gains. Also forbid the
		# usage of slower but "safe" glibc functions variants.
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-stack-protector -U_FORTIFY_SOURCE")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-stack-protector -U_FORTIFY_SOURCE")
	endif (PROTECTSTACK)

	if (GPROF)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
	endif (GPROF)

	# Prevent executable stack (used by gcc's implementation of lambdas) when
	# possible.
	if (NOEXECSTACK AND ${CXX_VERSION} GREATER 0)
		if (${CXX_VERSION} LESS 700)
			# With gcc 5 and 6 this merely results in removing the executable
			# stack flag from the final binary without preventing the actual
			# use of it, which poses a question: what happens if the OS is
			# configured to totally forbid executable stack for that program
			# and the latter still tries and uses it ?... A crash, probably.
			# This would need serious tests, under SELinux for example...
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wa,--noexecstack -Wl,-z,noexecstack")
			message(STATUS "Marking stack as non-executable despite the use of GNU lambdas: this is UNTESTED.")
		else (${CXX_VERSION} LESS 700)
			# With gcc 7+ we may use this new option, but then the generated
			# code (which uses function descriptors for lambdas) is slightly
			# slower since "most calls through function pointers then need to
			# test a bit in the pointer to identify descriptors".
			# See the implementor's note:
			# http://gcc.1065356.n8.nabble.com/RFC-PATCH-C-ADA-use-function-descriptors-instead-of-trampolines-in-C-td1503095.html#a1546131
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-trampolines")
		endif (${CXX_VERSION} LESS 700)
	endif (NOEXECSTACK AND ${CXX_VERSION} GREATER 0)

	# Never add thread-locking code to local static variables (we never use
	# non-thread_local static local variables in thread-sensitive code):
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-threadsafe-statics")

	# Use -fPIC under Linux for 64 bits (especially for our libraries),
	# else linking errors may happen (for example, when build llcommon as a
	# shared library, and trying to link it with the static libjsoncpp.a).
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

	# Remember whether the user asked for native optimization option or not.
	set(TUNED_BUILD OFF)
	if ("${CMAKE_CXX_FLAGS}" MATCHES ".*-march=native.*")
		set(TUNED_BUILD ON)
	endif ()

	# Vectorized maths selection
	if (ARCH STREQUAL "arm64")
		# We need a conversion header to get the SSE2 code translated into its
		# NEON counterpart (thanks to sse2neon.h).
		add_definitions(-DSSE2NEON=1)
		# If not already manualy specified, add the necessary -march tunes for
		# sse2neon.h to take full effect. See the "Usage" section at:
		# https://github.com/DLTcollab/sse2neon/blob/master/README.md
		set(TUNED_BUILD ON)
		if (NOT "${CMAKE_C_FLAGS}" MATCHES ".*-march.*")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a+fp+simd")
		endif ()
		if (NOT "${CMAKE_CXX_FLAGS}" MATCHES ".*-march=.*")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a+fp+simd")
		endif ()
		# Do not mess up sith aligment optimizations that would be irrelevant
		# for arm64...
		set(TUNED_BUILD ON)
	else (ARCH STREQUAL "arm64")
		# We only support 64 bits builds for x86 targets
		set(ARCH_FLAG "-m64")
		# SSE2 is always enabled by default for 64 bits targets
		set(COMPILER_MATHS "-mfpmath=sse")
		if (USEAVX2)
			set(COMPILER_MATHS "-mavx2 -mfpmath=sse")
		elseif (USEAVX)
			set(COMPILER_MATHS "-mavx -mfpmath=sse")
		endif ()
	endif (ARCH STREQUAL "arm64")

	# Compilation flags common to all build types and compilers.
	# NOTE: do not use -ffast-math: it makes us loose the Moon (!) and probably
	# causes other issues as well...
	# Please, note that -fno-strict-aliasing is *required*, because the code is
	# not strict aliasing safe and omitting this option would cause warnings and
	# possibly bogus optimizations by gcc.
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pipe ${GCC_DEBUG_OPTIONS} -fexceptions -fno-strict-aliasing -fvisibility=hidden -fsigned-char ${ARCH_FLAG} ${COMPILER_MATHS} -fno-math-errno -fno-trapping-math -pthread")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pipe ${GCC_DEBUG_OPTIONS} -fexceptions -fno-strict-aliasing -fsigned-char ${ARCH_FLAG} ${COMPILER_MATHS} -fno-math-errno -fno-trapping-math -pthread")

	# OpenMP support, if requested
	if (OPENMP)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")
	endif (OPENMP)

	# Use OPT_FLAGS to set optimizations (used for the release builds) that would
	# break debugging
	if (${CLANG_VERSION} GREATER 0)
		# Note: -Ofast *seems* to produce a valid binary (largely untested by me)
		# with clang v3.8.1, but causes an instant segfault for a binary compiled
		# with clang v3.9.0.
		set(OPT_FLAGS "-O3")
		if (${CLANG_VERSION} GREATER 799)
			set(OPT_FLAGS "${OPT_FLAGS} -fno-delete-null-pointer-checks -mno-retpoline -mno-retpoline-external-thunk")
		endif ()
		if (${CLANG_VERSION} GREATER 1099)
			set(OPT_FLAGS "${OPT_FLAGS} -mno-lvi-cfi -mno-lvi-hardening")
		endif ()
	else ()
		# Do not use our aggressive registers tuning options if the user
		# asked for -march=native optimization (i.e. let gcc set the adequate
		# optimizations in that case).
		if (TUNED_BUILD)
			set(OPT_FLAGS "-O3 -fno-delete-null-pointer-checks -fno-ipa-cp-clone -fno-align-labels -fno-align-loops")
		else (TUNED_BUILD)
			set(OPT_FLAGS "-O3 -fno-delete-null-pointer-checks -fno-ipa-cp-clone -fno-align-labels -fno-align-loops -fsched-pressure -frename-registers -fweb -fira-hoist-pressure")
		endif (TUNED_BUILD)
	endif ()

	add_definitions(-DLL_LINUX=1)

	set(CMAKE_C_FLAGS_DEBUG "-fno-inline -D_DEBUG -DLL_DEBUG=1")
	set(CMAKE_CXX_FLAGS_DEBUG "-fno-inline -D_DEBUG -DLL_DEBUG=1")
	set(CMAKE_C_FLAGS_RELWITHDEBINFO "-fno-inline -DLL_NO_FORCE_INLINE=1 -DNDEBUG")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-fno-inline -DLL_NO_FORCE_INLINE=1 -DNDEBUG")
	set(CMAKE_C_FLAGS_RELEASE "${OPT_FLAGS} -DNDEBUG")
	set(CMAKE_CXX_FLAGS_RELEASE "${OPT_FLAGS} -DNDEBUG")
endif (LINUX)

if (LINUX OR DARWIN)
	set(GCC_WARNINGS "-Wall ${GCC_WARNINGS}")
	set(GCC_CXX_WARNINGS "-Wall -Wno-reorder ${GCC_CXX_WARNINGS}")

	if (NOT NO_FATAL_WARNINGS)
		set(GCC_WARNINGS "${GCC_WARNINGS} -Werror")
		set(GCC_CXX_WARNINGS "${GCC_CXX_WARNINGS} -Werror")
	endif (NOT NO_FATAL_WARNINGS)

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${GCC_WARNINGS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_CXX_WARNINGS}")
endif (LINUX OR DARWIN)

if (WINDOWS)
	# Supported build types (only "Release" for Windows):
	set(CMAKE_CONFIGURATION_TYPES "Release"
		CACHE STRING "Supported build types." FORCE)

	# Remove default /Zm1000 flag that cmake inserts
	string (REPLACE "/Zm1000" " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

	# Do not build DLLs.
	set(BUILD_SHARED_LIBS OFF)

	# SSE2 is always enabled by default for 64 bits targets
	set(COMPILER_MATHS "/fp:fast")
	if (USEAVX2)
		set(COMPILER_MATHS "/arch:AVX2 /fp:fast")
	elseif (USEAVX)
		set(COMPILER_MATHS "/arch:AVX /fp:fast")
	endif ()

	# Compilation flags common to all build types
	if (USING_CLANG)
		set(IGNORED_WARNINGS "-Wno-deprecated-declarations -Wno-reorder-ctor -Wno-inconsistent-dllimport -Wno-dll-attribute-on-redeclaration -Wno-ignored-pragma-optimize -Wno-writable-strings -Wno-unused-function -Wno-unused-local-typedef -Wno-unknown-warning-option")
		set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} /EHs ${COMPILER_MATHS} -m64 /MP /W2 /c")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++14 /EHs ${COMPILER_MATHS} -m64 -fno-strict-aliasing /MP /TP /W2 /c ${IGNORED_WARNINGS}")
	else (USING_CLANG)
		set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} /EHs ${COMPILER_MATHS} /MP /W2 /c /nologo")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++14 /EHs ${COMPILER_MATHS} /MP /TP /W2 /c /nologo")
	endif (USING_CLANG)

	# Not strictly stack protection, but buffer overrun detection/protection
	if (PROTECTSTACK)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GS")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /GS")
	else (PROTECTSTACK)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GS-")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /GS-")
	endif (PROTECTSTACK)

	# Never add thread-locking code to local static variables (we never use
	# non-thread_local static local variables in thread-sensitive code):
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:threadSafeInit-")

	# OpenMP support, if requested
	if (OPENMP)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /openmp")
	endif (OPENMP)

	# Build type and compiler dependent compilation flags
	if (USING_CLANG)
		set(CMAKE_CXX_FLAGS_RELEASE "/clang:-Ofast /DNDEBUG /D_SECURE_SCL=0 /D_HAS_ITERATOR_DEBUGGING=0"
			CACHE STRING "C++ compiler release options" FORCE)
	else (USING_CLANG)
		set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Oi /DNDEBUG /D_SECURE_SCL=0 /D_HAS_ITERATOR_DEBUGGING=0"
			CACHE STRING "C++ compiler release options" FORCE)
	endif (USING_CLANG)
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

	# zlib has assembly-language object files incompatible with SAFESEH
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")

	set(CMAKE_CXX_STANDARD_LIBRARIES "")
	set(CMAKE_C_STANDARD_LIBRARIES "")

	add_definitions(/DLL_WINDOWS=1 /DUNICODE /D_UNICODE)

	# See: http://msdn.microsoft.com/en-us/library/aa383745%28v=VS.85%29.aspx
	# Configure the win32 API for Windows 7 compatibility
	set(WINVER "0x0601" CACHE STRING "Win32 API Target version")
	add_definitions("/DWINVER=${WINVER}" "/D_WIN32_WINNT=${WINVER}")

	if (NOT NO_FATAL_WARNINGS)
		add_definitions(/WX)
	endif ()
endif (WINDOWS)

# Add link-time optimizations if requested, enabling it via cmake (which knows
# what compilers support it and what option(s) to use). Note that only cmake
# versions 3.9 and newer will actually enable lto for gcc and clang.
# Note also that from my experiments, it actually makes the resulting binary
# slower... I suspect that it results in some normally (and flagged to be)
# inlined functions to get factorized into normal functions...
if (USELTO)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif (USELTO)

if (NOT USESYSTEMLIBS)
	if (LINUX)
		set(${ARCH}_linux_INCLUDES
			glib-2.0
			freetype
			)
	endif (LINUX)
endif (NOT USESYSTEMLIBS)

# Add defines corresponding to compilation options
if (OPENMP)
	add_definitions(-DLL_OPENMP=1)
endif (OPENMP)

if (USE_JEMALLOC)
	add_definitions(-DLL_JEMALLOC=1)
endif (USE_JEMALLOC)

if (FAST_CONTAINERS)
  add_definitions(-DLL_PHMAP=1)
endif ()

if (ENABLE_FAST_TEXTURE_CACHE)
  add_definitions(-DLL_FAST_TEX_CACHE=1)
endif (ENABLE_FAST_TEXTURE_CACHE)

if (ENABLE_ANIMESH_VISUAL_PARAMS)
  add_definitions(-DLL_ANIMESH_VPARAMS=1)
endif (ENABLE_ANIMESH_VISUAL_PARAMS)

if (USE_MESHOPTIMIZER)
  add_definitions(-DLL_MESHOPT=1)
endif (USE_MESHOPTIMIZER)

if (USE_TRACY)
  if (TRACY_WITH_FAST_TIMERS)
    if (TRACY_MEMORY)
      add_definitions(-DTRACY_ENABLE=4)
    else (TRACY_MEMORY)
      add_definitions(-DTRACY_ENABLE=3)
    endif (TRACY_MEMORY)
  else (TRACY_WITH_FAST_TIMERS)
    if (TRACY_MEMORY)
      add_definitions(-DTRACY_ENABLE=2)
    else (TRACY_MEMORY)
      add_definitions(-DTRACY_ENABLE=1)
    endif (TRACY_MEMORY)
  endif (TRACY_WITH_FAST_TIMERS)
endif (USE_TRACY)

if (WINDOWS AND USE_NETBIOS)
  add_definitions(-DLL_NETBIOS=1)
endif (WINDOWS AND USE_NETBIOS)
