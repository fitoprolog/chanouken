# -*- cmake -*-

# NOTE: do NOT include 00-Common here: it would cause winmmsihm linking to fail
include(00-BuildOptions)

# Plugins common to all OSes
add_subdirectory(base)
add_subdirectory(cef)
add_subdirectory(gstreamer)
