cmake_minimum_required(VERSION 3.5)

## project name
project(open62541cpp)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})

## library name
set(OPEN62541_CPP open62541cpp)

## build settings - try and match the c library version
set(VERSION_MAJOR 1)
set(VERSION_MINOR 2)
set(VERSION_PATCH 0)

set(PACKAGE_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH})

#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY C:/oxsas-source/Bin/Oxsas)
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY C:/oxsas-source/Bin/Oxsas)
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY C:/oxsas-source/Bin/Oxsas)
## Set the UA log level.
# IF(CMAKE_BUILD_TYPE MATCHES DEBUG)
#     add_definitions(-DUA_LOGLEVEL=100)
# ELSE()
#     add_definitions(-DUA_LOGLEVEL=500)
# ENDIF(CMAKE_BUILD_TYPE MATCHES DEBUG)

## --- Build options ---
set(BUILD_EXAMPLES FALSE CACHE BOOL "Build example programs")

## --- C++14 build flags ---
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(INCLUDE_INSTALL_DIR include)
set(LIB_INSTALL_DIR lib)

set(UA_CPP_SANITIZE ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND UA_CPP_SANITIZE AND
        "x${CMAKE_C_COMPILER_ID}" STREQUAL "xClang")
    # Add default sanitizer settings when using clang and Debug build.
    # This allows e.g. CLion to find memory locations for SegFaults
    message(STATUS "Sanitizer enabled")
    set(SANITIZER_FLAGS "-g -fno-omit-frame-pointer -gline-tables-only -fsanitize=address -fsanitize-address-use-after-scope -fsanitize=leak -fsanitize=undefined")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10.0)
        set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize-coverage=trace-pc-guard")
    endif()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZER_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
endif()

add_compile_options(/wd4251 /wd4200 /wd4820 /wd4365 /wd4100 /wd4668 /wd4710)
add_compile_options(/Qspectre)

cmake_policy(SET CMP0079 NEW)
set(UA_NAMESPACE_ZERO FULL CACHE STRING "" FORCE)
set(open62541_TOOLS_DIR "${CMAKE_SOURCE_DIR}/open62541/tools")
set(open62541_NODESET_DIR "${CMAKE_SOURCE_DIR}/open62541/deps/ua-nodeset")
set(oxsas_NODESET_DIR "${CMAKE_SOURCE_DIR}/bea-model")

add_subdirectory(open62541)

include_directories(${CMAKE_BINARY_DIR}/open62541/src_generated)
include_directories(${CMAKE_BINARY_DIR}/open62541/src_generated)
include_directories("${CMAKE_SOURCE_DIR}/open62541/include")
include_directories("${CMAKE_SOURCE_DIR}/open62541/arch")
include_directories("${CMAKE_SOURCE_DIR}/open62541/deps")

ua_generate_nodeset_and_datatypes(
  NAME "di"
  FILE_NS "${open62541_NODESET_DIR}/DI/Opc.Ua.Di.NodeSet2.xml"
  FILE_CSV "${open62541_NODESET_DIR}/DI/OpcUaDiModel.csv"
  FILE_BSD "${open62541_NODESET_DIR}/DI/Opc.Ua.Di.Types.bsd"
  NAMESPACE_MAP "2:http://opcfoundation.org/UA/DI/"
)

add_library(ua_gen_di
  STATIC
  src_generated/open62541/namespace_di_generated.c
  src_generated/open62541/types_di_generated.c
  src_generated/open62541/types_di_generated.h
  src_generated/open62541/types_di_generated_handling.h
)

 add_dependencies(ua_gen_di open62541::open62541)

ua_generate_nodeset_and_datatypes(
  NAME "adi"
  FILE_NS "${open62541_NODESET_DIR}/ADI/Opc.Ua.Adi.NodeSet2.xml"
  FILE_CSV "${open62541_NODESET_DIR}/ADI/OpcUaAdiModel.csv"
  FILE_BSD "${open62541_NODESET_DIR}/ADI/Opc.Ua.Adi.Types.bsd"
  NAMESPACE_MAP "3:http://opcfoundation.org/UA/ADI/"
  DEPENDS "di" 
)

add_library(ua_gen_adi
  STATIC
  src_generated/open62541/namespace_adi_generated.c
  src_generated/open62541/types_adi_generated.c
  src_generated/open62541/types_adi_generated.h
  src_generated/open62541/types_adi_generated_handling.h
)

add_dependencies(ua_gen_adi open62541::open62541 ua_gen_di)

ua_generate_nodeset_and_datatypes(
  NAME "bea"
  FILE_NS "${oxsas_NODESET_DIR}/TFS.Bea.Nodeset2.xml"
  FILE_CSV "${oxsas_NODESET_DIR}/Nodes.csv"
  FILE_BSD "${oxsas_NODESET_DIR}/TFS.Bea.Types.bsd"
  NAMESPACE_MAP "4:https://thermofisher.com/UA/TFS/BEA/"
  DEPENDS "adi"
)

add_library(ua_gen_bea
  STATIC
  src_generated/open62541/namespace_bea_generated.c
  src_generated/open62541/bea_nodeids.h 
  src_generated/open62541/types_bea_generated.h 
  src_generated/open62541/types_bea_generated.c 
  src_generated/open62541/types_bea_generated_handling.h
)

add_dependencies(ua_gen_bea open62541::open62541 ua_gen_di ua_gen_adi)

include_directories(${CMAKE_BINARY_DIR}/src_generated/open62541)

add_subdirectory(src)
target_link_libraries(${OPEN62541_CPP} PUBLIC ua_gen_di ua_gen_adi ua_gen_bea)

if (BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

