# -*- cmake -*-
if (LLMATH_CMAKE_INCLUDED)
  return()
endif (LLMATH_CMAKE_INCLUDED)
set (LLMATH_CMAKE_INCLUDED TRUE)

set(LLMATH_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/llmath)

set(LLMATH_LIBRARIES llmath)