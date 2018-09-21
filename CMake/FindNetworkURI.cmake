FIND_PATH(NETWORKURI_INCLUDE_DIR NAMES network/uri.h network/optional.hpp network/string_view.hpp network/uri.hpp
				       network/uri/config.hpp network/uri/uri_builder.hpp network/uri/uri_errors.hpp network/uri/uri.hpp network/uri/uri_io.hpp
				       network/uri/detail/decode.hpp network/uri/detail/encode.hpp network/uri/detail/translate.hpp network/uri/detail/uri_parts.hpp
     PATHS /usr/include /usr/local/include)


FIND_LIBRARY(NETWORKURI_LIBRARY NAMES network-uri PATHS /usr/lib64 /usr/local/lib64)

IF (NETWORKURI_INCLUDE_DIR)
  MESSAGE(STATUS "network-uri Include dir: ${NETWORKURI_INCLUDE_DIR}")
ELSE()
  MESSAGE(FATAL_ERROR "Cannot find libnetwork-uri include files")
ENDIF()


IF (NETWORKURI_LIBRARY)
  MESSAGE(STATUS "network-uri library: ${NETWORKURI_LIBRARY}")
ELSE()
  MESSAGE(FATAL_ERROR "Cannot find libnetwork-uri library")
ENDIF()

include_directories(${NETWORKURI_INCLUDE_DIR})
LIST(APPEND DRIVESHAFT_LINK_LIBRARIES network-uri)
