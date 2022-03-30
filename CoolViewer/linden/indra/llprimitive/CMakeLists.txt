# -*- cmake -*-

project(llprimitive)

include(00-Common)
include(LLCommon)
include(LLCharacter)
include(LLMath)
include(LLMessage)
include(LLXML)
include(ZLIB)

include_directories(
    ${LLCOMMON_INCLUDE_DIRS}
    ${LLMATH_INCLUDE_DIRS}
    ${LLMESSAGE_INCLUDE_DIRS}
    ${LLXML_INCLUDE_DIRS}
    ${LIBS_PREBUILT_DIR}/include/collada
    ${LIBS_PREBUILT_DIR}/include/collada/1.4
    ${LLCHARACTER_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    )

set(llprimitive_SOURCE_FILES
    imageids.cpp	
    lldaeloader.cpp
    llmaterial.cpp
    llmaterialid.cpp
    llmaterialtable.cpp
    llmediaentry.cpp
    llmodel.cpp
    llmodelloader.cpp
    llphysicsshapebuilderutil.cpp
    llprimitive.cpp
    llprimtexturelist.cpp
    lltextureanim.cpp
    lltextureentry.cpp
    lltreeparams.cpp
    llvolumemessage.cpp
    sound_ids.cpp
    )

set(llprimitive_HEADER_FILES
    CMakeLists.txt
    imageids.h
    lldaeloader.h
    lllslconstants.h
    llmaterial.h
    llmaterialid.h
    llmaterialtable.h
    llmediaentry.h
    llmodel.h
    llmodelloader.h
    llphysicsshapebuilderutil.h
    llprimitive.h
    llprimtexturelist.h
    lltextureanim.h
    lltextureentry.h
    lltreeparams.h
    lltree_common.h
    llvolumemessage.h
    object_flags.h
    sound_ids.h
    )
        
set_source_files_properties(${llprimitive_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND llprimitive_SOURCE_FILES ${llprimitive_HEADER_FILES})

add_library (llprimitive ${llprimitive_SOURCE_FILES})
add_dependencies(llprimitive prepare)

target_link_libraries(llprimitive
    ${LLCOMMON_LIBRARIES}
    ${LLMATH_LIBRARIES}
    ${LLMESSAGE_LIBRARIES}
    ${LLXML_LIBRARIES}
    ${LLCHARACTER_LIBRARIES}
    ${ZLIB_LIBRARIES}
    )

if (NOT ${CMAKE_VERSION} VERSION_LESS "3.16.0")
  target_precompile_headers(llprimitive PRIVATE <linden_common.h>)
  if (USEUNITYBUILD)
    set_target_properties(llprimitive PROPERTIES UNITY_BUILD ON)
  endif (USEUNITYBUILD)
endif ()

get_directory_property(ALLDEFINES COMPILE_DEFINITIONS)
message("llprimitive COMPILE_DEFINITIONS = ${ALLDEFINES}")
