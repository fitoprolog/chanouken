# -*- cmake -*-
if (LLMESSAGE_CMAKE_INCLUDED)
  return()
endif (LLMESSAGE_CMAKE_INCLUDED)
set (LLMESSAGE_CMAKE_INCLUDED TRUE)

include(CURL)
include(XmlRpcEpi)
include(ZLIB)
include(Boost)

set(LLMESSAGE_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/llmessage
    ${CURL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
    ${XMLRPCEPI_INCLUDE_DIRS}
    ${BOOST_INCLUDE_DIRS}
    )

set(LLMESSAGE_LIBRARIES llmessage)
