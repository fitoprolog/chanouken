# -*- cmake -*-
if (LLWINDOW_CMAKE_INCLUDED)
  return()
endif (LLWINDOW_CMAKE_INCLUDED)
set (LLWINDOW_CMAKE_INCLUDED TRUE)

include(00-BuildOptions)
include(Prebuilt)

if (LINUX)
  # We do not use the system libraries for SDL1, because ours got bugs patched
  # (window resizing) and standard system libraries are most likely not...
  if (USESYSTEMLIBS AND USE_SDL2)
    set(SDL2_FIND_QUIETLY ON)
    set(SDL2_FIND_REQUIRED OFF)
    include(FindSDL2)

    # This should be done by FindSDL2.
    mark_as_advanced(
      SDLMAIN_LIBRARY
      SDL2_INCLUDE_DIR
      SDL2_LIBRARY
    )
	set (SDL_LIBRARY ${SDL2_LIBRARY})
	set (SDL_INCLUDE_DIR ${SDL2_INCLUDE_DIR})
  endif (USESYSTEMLIBS AND USE_SDL2)

  if (NOT SDL_INCLUDE_DIR)
    if (USE_SDL2)
      use_prebuilt_binary(libSDL2)
      set (SDL_LIBRARY SDL2)
    else (USE_SDL2)
      use_prebuilt_binary(libSDL1)
      set (SDL_LIBRARY SDL)
    endif (USE_SDL2)
    set (SDL_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include)
    set (SDL_FOUND "YES")
  endif (NOT SDL_INCLUDE_DIR)

  if (USE_SDL2)
    add_definitions(-DLL_SDL2=1)
  else (USE_SDL2)
    add_definitions(-DLL_SDL=1)
  endif (USE_SDL2)
  include_directories(${SDL_INCLUDE_DIR})
endif (LINUX)

set(LLWINDOW_INCLUDE_DIRS
    # Note: GL/ includes are inside llrender
    ${CMAKE_SOURCE_DIR}/llrender
    ${CMAKE_SOURCE_DIR}/llwindow
    )

set(LLWINDOW_LIBRARIES llwindow)

# llwindowsdl.cpp uses X11 and fontconfig...
if (LINUX)
    list(APPEND LLWINDOW_LIBRARIES X11)

  if (USESYSTEMLIBS)
    include(FindPkgConfig)
    pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
    link_directories(${FONTCONFIG_LIBRARY_DIRS})
    list(APPEND LLWINDOW_LIBRARIES ${FONTCONFIG_LIBRARIES})
    include_directories(${FONTCONFIG_INCLUDE_DIRS})
  else (USESYSTEMLIBS)
    use_prebuilt_binary(fontconfig)
    list(APPEND LLWINDOW_LIBRARIES fontconfig)
    include_directories(${LIBS_PREBUILT_DIR}/include/fontconfig)
  endif (USESYSTEMLIBS)
endif (LINUX)

# llwindowwin32.cpp uses comdlg32 and imm32
if (WINDOWS)
    list(APPEND LLWINDOW_LIBRARIES comdlg32 imm32)
endif (WINDOWS)
