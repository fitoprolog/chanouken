# -*- cmake -*-
if (LLAPPEARANCE_CMAKE_INCLUDED)
  return()
endif (LLAPPEARANCE_CMAKE_INCLUDED)
set (LLAPPEARANCE_CMAKE_INCLUDED TRUE)

include(Variables)

set(LLAPPEARANCE_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/llappearance)

set(LLAPPEARANCE_LIBRARIES llappearance)
