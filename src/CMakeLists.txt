cmake_minimum_required(VERSION 3.5)

set(LIB_SOURCES
    "objects/ArgumentList.cpp"
    "objects/BrowserBase.cpp"
    "objects/EventFilterSelect.cpp"
    "objects/EventSelectClauseArray.cpp"
    "objects/ExpandedNodeId.cpp"
    "objects/MethodAttributes.cpp"
    "objects/NodeId.cpp"
    "objects/NodeIdMap.cpp"
    "objects/ObjectAttributes.cpp"
    "objects/StringUtils.cpp"
    "objects/UANodeIdList.cpp"
    "objects/UANodeTree.cpp"
    "objects/VariableAttributes.cpp"
    "objects/Variant.cpp"
    clientbrowser.cpp
    clientcache.cpp
    clientcachethread.cpp
    clientnodetree.cpp
    clientsubscription.cpp
    condition.cpp
    discoveryserver.cpp
    historydatabase.cpp
    jsoncpp.cpp
    monitoreditem.cpp
    nodecontext.cpp
    open62541client.cpp
    open62541objects.cpp
    open62541server.cpp
    "open62541timer.cpp"
    serverbrowser.cpp
    servermethod.cpp
    servernodetree.cpp
    serverobjecttype.cpp
    serverrepeatedcallback.cpp
    servertimedcallback.cpp
     "ServerRegister.cpp")
if(MSVC)
add_definitions(/FI"open62541cpp/open62541cppexp.h")
else()
    # GCC or Clang
    add_definitions(-include foo.h)
endif()

## library name
set(OPEN62541_CPP open62541cpp)

# Building shared library
add_library(${OPEN62541_CPP} SHARED ${LIB_SOURCES} )
set_property(TARGET ${OPEN62541_CPP} PROPERTY POSITION_INDEPENDENT_CODE ON)

add_compile_definitions(UA_DYNAMIC_LINKING_EXPORT)
target_compile_definitions(${OPEN62541_CPP} PRIVATE -DUA_DYNAMIC_LINKING_EXPORT)
target_include_directories(open62541cpp PUBLIC ../include)
set_target_properties(${OPEN62541_CPP} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    #LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "C:/oxsas-source/Bin/Oxsas"
    RUNTIME_OUTPUT_DIRECTORY "C:/oxsas-source/Bin/Oxsas"
    )

# add boost to target
include(../Common.cmake)
add_boost_lib(${OPEN62541_CPP})
set_build_system_option(${OPEN62541_CPP})

#find_package(open62541)
link_directories(${CMAKE_SOURCE_DIR}/bin/Debug)
target_link_libraries(${OPEN62541_CPP} PUBLIC ${Boost_LIBRARIES} open62541)

## set the shared library soname
set_target_properties(${OPEN62541_CPP} PROPERTIES
        VERSION ${PACKAGE_VERSION}
        SOVERSION ${VERSION_MAJOR})

include(CMakePackageConfigHelpers)
configure_package_config_file(open62541cppConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/open62541cppConfig.cmake
        INSTALL_DESTINATION ${LIB_INSTALL_DIR}/${OPEN62541_CPP}/cmake
        PATH_VARS INCLUDE_INSTALL_DIR LIB_INSTALL_DIR
        )

write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/${OPEN62541_CPP}ConfigVersion.cmake
        VERSION ${PACKAGE_VERSION}
        COMPATIBILITY SameMajorVersion)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${OPEN62541_CPP}Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${OPEN62541_CPP}ConfigVersion.cmake
        DESTINATION ${LIB_INSTALL_DIR}/${OPEN62541_CPP}/cmake )

## install the shared library
install(TARGETS ${OPEN62541_CPP}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
        )
