# -*- cmake -*-

project(llcommon)

include(00-Common)
include(LLCommon)
include(Linking)
include(JsonCpp)
include(ZLIB)

include_directories(
    ${EXPAT_INCLUDE_DIRS}
    ${LLCOMMON_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIRS}
    )

set(llcommon_SOURCE_FILES
    indra_constants.cpp
    llapp.cpp
    llapr.cpp
    llassettype.cpp
    llbase64.cpp
    llbitpack.cpp
    llcallbacklist.cpp
    llcommandlineparser.cpp
    llcommon.cpp
    llcoros.cpp
    llcrc.cpp
    llcriticaldamp.cpp
    lldate.cpp
    lldependencies.cpp
    lldictionary.cpp
    llerror.cpp
    llevent.cpp
    lleventcoro.cpp
    lleventdispatcher.cpp
    lleventfilter.cpp
    llevents.cpp
    lleventtimer.cpp
    llfasttimer.cpp
    llfile.cpp
    llfindlocale.cpp
    llformat.cpp
    llframetimer.cpp
    llinstancetracker.cpp
    llliveappconfig.cpp
    lllivefile.cpp
    llmd5.cpp
    llmemory.cpp
    llmemorystream.cpp
    llmortician.cpp
    llmutex.cpp
    llpointer.cpp
    llprocesslauncher.cpp
    llprocessor.cpp
    llqueuedthread.cpp
    llrand.cpp
    llrefcount.cpp
    llsafehandle.cpp
    llsd.cpp
    llsdjson.cpp
    llsdserialize.cpp
    llsdutil.cpp
    llsingleton.cpp
    llstat.cpp
    llstreamtools.cpp
    llstring.cpp
    llstringtable.cpp
    llsys.cpp
    llthread.cpp
    lltimer.cpp
    hbtracy.cpp
    lluri.cpp
    lluuid.cpp
    llwatchdog.cpp
    llworkerthread.cpp
    llxorcipher.cpp
    )
    
set(llcommon_HEADER_FILES
    CMakeLists.txt

    indra_constants.h
    linden_common.h
    llagentconstants.h
    llalignedarray.h
    llapp.h
    llapr.h
    llassettype.h
    llatomic.h
    llbase64.h
    llbitpack.h
    llcallbacklist.h
    llcommandlineparser.h
    llcommon.h
    llcommonmath.h
    llcoros.h
    llcrc.h
    llcriticaldamp.h
    lldate.h
    lldefs.h
    lldependencies.h
    lldepthstack.h
    lldictionary.h
    llendianswizzle.h
    llerror.h
    llerrorcontrol.h
    llevent.h
    lleventcoro.h
    lleventdispatcher.h
    lleventfilter.h
    llevents.h
    lleventtimer.h
    llfastmap.h
    llfastset.h
    llfasttimer.h
    llfile.h
    llfindlocale.h
    llformat.h
    llframetimer.h
    llhandle.h
    llinitdestroyclass.h
    llinstancetracker.h
    llliveappconfig.h
    lllivefile.h
    llmd5.h
    llmemory.h
    llmemorystream.h
    llmortician.h
    llmutex.h
    llpointer.h
    llpreprocessor.h
    llpriqueuemap.h
    llprocesslauncher.h
    llprocessor.h
    llqueuedthread.h
    llrand.h
    llrefcount.h
    llsafehandle.h
    llsd.h
    llsdjson.h
    llsdserialize.h
    llsdutil.h
    llsimplehash.h
    llsingleton.h
    llstat.h
    llstl.h
    llstreamtools.h
    llstrider.h
    llstring.h
    llstringtable.h
    llsys.h
    llthread.h
    llthreadsafequeue.h
    lltimer.h
    hbtracy.h
    lluri.h
    lluuid.h
    llversionviewer.h
    llwatchdog.h
    llwin32headers.h
    llwin32headerslean.h
    llworkerthread.h
    llxorcipher.h
    stdtypes.h
    stringize.h
    )

set_source_files_properties(${llcommon_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND llcommon_SOURCE_FILES ${llcommon_HEADER_FILES})

if (NOT WINDOWS)
  add_definitions(-fPIC)
endif (NOT WINDOWS)

if (LLCOMMON_LINK_SHARED)
  add_library(llcommon SHARED ${llcommon_SOURCE_FILES})
  if (WINDOWS)
    # always generate llcommon.pdb, even for "Release" builds, and do not link
    # against LIBCMT to avoid linker warnings.
    set_target_properties(llcommon PROPERTIES LINK_FLAGS "/DEBUG /NODEFAULTLIB:LIBCMT")
  endif (WINDOWS)
  if (DARWIN)
      # Don't embed a full path in the library's install name
      set_target_properties(
        llcommon
        PROPERTIES
        BUILD_WITH_INSTALL_RPATH 1
        INSTALL_NAME_DIR "@executable_path/../Resources"
      )
  endif (DARWIN)
else(LLCOMMON_LINK_SHARED)
    add_library(llcommon ${llcommon_SOURCE_FILES})
endif(LLCOMMON_LINK_SHARED)

add_dependencies(llcommon prepare)
target_link_libraries(
    llcommon
    ${APRUTIL_LIBRARIES}
    ${APR_LIBRARIES}
    ${EXPAT_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${WINDOWS_LIBRARIES}
    ${TRACY_LIBRARY}
    ${BOOST_FIBER_LIBRARY}
    ${BOOST_CONTEXT_LIBRARY}
    ${BOOST_PROGRAM_OPTIONS_LIBRARY}
    ${BOOST_THREAD_LIBRARY}
    ${BOOST_CHRONO_LIBRARY}
    ${BOOST_SYSTEM_LIBRARY}
    ${BOOST_ATOMIC_LIBRARY}
    )

if (NOT ${CMAKE_VERSION} VERSION_LESS "3.16.0")
  target_precompile_headers(llcommon PRIVATE linden_common.h llfastmap.h)
  if (USEUNITYBUILD)
    set_target_properties(llcommon PROPERTIES UNITY_BUILD ON)
	# Exclude this one because of old events and new events ambiguity...
	set_property(SOURCE llevent.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
	# Exclude this one because it #defines an S32 symbol...
	set_property(SOURCE llmd5.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
  endif (USEUNITYBUILD)
endif ()

get_directory_property(ALLDEFINES COMPILE_DEFINITIONS)
message("llcommon COMPILE_DEFINITIONS = ${ALLDEFINES}")
