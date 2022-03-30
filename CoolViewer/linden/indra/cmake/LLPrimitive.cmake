# -*- cmake -*-
if (LLPRIMITIVE_CMAKE_INCLUDED)
  return()
endif (LLPRIMITIVE_CMAKE_INCLUDED)
set (LLPRIMITIVE_CMAKE_INCLUDED TRUE)

include(ZLIB)

# These should be moved to their own cmake file
use_prebuilt_binary(colladadom)
# NOTE: libpcre, libpcrecpp and libxml2 are required by collada-dom...
use_prebuilt_binary(pcre)
use_prebuilt_binary(libxml2)

set(LLPRIMITIVE_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/llprimitive
    ${LIBS_PREBUILT_DIR}/include/collada
    ${LIBS_PREBUILT_DIR}/include/collada/1.4
    )

if (WINDOWS)
    set(LLPRIMITIVE_LIBRARIES
		llprimitive
		libcollada14dom23-s
		${MINIZIP_LIBRARIES}
		libxml2_a
		pcrecpp
		pcre
		)
elseif (DARWIN)
    set(LLPRIMITIVE_LIBRARIES 
        llprimitive
        collada14dom
        ${MINIZIP_LIBRARIES}
        xml2
       	pcrecpp
        pcre
        iconv           # Required by libxml2
        )
elseif (LINUX)
    set(LLPRIMITIVE_LIBRARIES 
        llprimitive
        collada14dom
        ${MINIZIP_LIBRARIES}
        # Make sure we link against our pre-built static libraries
        ${ARCH_PREBUILT_DIRS_RELEASE}/libxml2.a
		${ARCH_PREBUILT_DIRS_RELEASE}/libpcrecpp.a
		${ARCH_PREBUILT_DIRS_RELEASE}/libpcre.a
        )
endif (WINDOWS)
