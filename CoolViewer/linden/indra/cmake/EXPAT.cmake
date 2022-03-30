# -*- cmake -*-
if (EXPAT_CMAKE_INCLUDED)
	return()
endif (EXPAT_CMAKE_INCLUDED)
set (EXPAT_CMAKE_INCLUDED TRUE)

include(Prebuilt)

set(EXPAT_FIND_QUIETLY OFF)
set(EXPAT_FIND_REQUIRED ON)

if (USESYSTEMLIBS)
  include(FindEXPAT)
else (USESYSTEMLIBS)
    use_prebuilt_binary(expat)
	add_definitions(-DXML_STATIC)
    if (WINDOWS)
        set(EXPAT_LIBRARIES libexpatMT)
    else (WINDOWS)
        set(EXPAT_LIBRARIES expat)
    endif (WINDOWS)
    set(EXPAT_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/expat)
endif (USESYSTEMLIBS)
