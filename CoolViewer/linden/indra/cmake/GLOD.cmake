# -*- cmake -*-
if (GLOD_CMAKE_INCLUDED)
  return()
endif (GLOD_CMAKE_INCLUDED)
set (GLOD_CMAKE_INCLUDED TRUE)

include(Prebuilt)

# NOTE: it is extremely unlikely that GLOD is installed on a system...
# So we unconditionally use our pre-built library.

use_prebuilt_binary(glod)

set(GLOD_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include)
set(GLOD_LIBRARIES glod)
