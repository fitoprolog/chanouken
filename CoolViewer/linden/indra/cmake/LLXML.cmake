# -*- cmake -*-
if (LLXML_CMAKE_INCLUDED)
  return()
endif (LLXML_CMAKE_INCLUDED)
set (LLXML_CMAKE_INCLUDED TRUE)

include(Boost)
include(EXPAT)

set(LLXML_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/llxml
    ${Boost_INCLUDE_DIRS}
    ${EXPAT_INCLUDE_DIRS}
    )

set(LLXML_LIBRARIES llxml)
