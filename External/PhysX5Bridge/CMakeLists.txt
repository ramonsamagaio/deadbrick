cmake_minimum_required(VERSION 3.21)
project(DeadbrickPhysXBridge LANGUAGES CXX)

if(NOT DEFINED DEADBRICK_PHYSX_ROOT)
    message(FATAL_ERROR "DEADBRICK_PHYSX_ROOT must point to the normalized PhysX SDK root")
endif()

add_library(DeadbrickPhysXBridge STATIC
    DeadbrickPhysXBridge.cpp
)

target_include_directories(DeadbrickPhysXBridge
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${DEADBRICK_PHYSX_ROOT}/include"
)

target_compile_features(DeadbrickPhysXBridge PRIVATE cxx_std_17)
target_compile_definitions(DeadbrickPhysXBridge PRIVATE PX_CHECKED=1)

if(MSVC)
    set_property(TARGET DeadbrickPhysXBridge PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    target_compile_options(DeadbrickPhysXBridge PRIVATE /EHsc /permissive-)
endif()

set_target_properties(DeadbrickPhysXBridge PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/out"
)
