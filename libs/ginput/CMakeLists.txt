cmake_minimum_required(VERSION 3.20)

project(ginput VERSION 0.1.0 LANGUAGES CXX)

option(GINPUT_STRICT "Enable strict compiler warnings" ON)
option(GINPUT_WARN_AS_ERROR "Treat warnings as errors" OFF)
option(GINPUT_BUILD_EXAMPLES "Build example programs" ON)
option(GINPUT_BUILD_TESTS "Build test programs" ON)
set(GINPUT_GSEXP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../gsexp"
    CACHE PATH "Path to a gsexp source tree when gsexp::gsexp is not already available")

if(NOT TARGET gsexp::gsexp)
    if(EXISTS "${GINPUT_GSEXP_SOURCE_DIR}/CMakeLists.txt")
        set(GSEXP_BUILD_EXAMPLES OFF CACHE BOOL "Build gsexp examples" FORCE)
        set(GSEXP_BUILD_TESTS OFF CACHE BOOL "Build gsexp tests" FORCE)
        set(GSEXP_BUILD_BENCHMARKS OFF CACHE BOOL "Build gsexp benchmarks" FORCE)
        add_subdirectory("${GINPUT_GSEXP_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/gsexp")
    else()
        message(FATAL_ERROR
            "gsexp::gsexp target was not found and GINPUT_GSEXP_SOURCE_DIR does not point "
            "to a gsexp source tree: ${GINPUT_GSEXP_SOURCE_DIR}")
    endif()
endif()

add_library(ginput
    src/io.cpp
    src/profile.cpp
    src/reconcile.cpp
    src/schema.cpp
    src/types.cpp
)

add_library(ginput::ginput ALIAS ginput)

add_library(ginput_runtime INTERFACE)
add_library(ginput::runtime ALIAS ginput_runtime)

target_include_directories(ginput
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(ginput PUBLIC cxx_std_20)
target_link_libraries(ginput PUBLIC gsexp::gsexp)

target_link_libraries(ginput_runtime INTERFACE ginput::ginput)

function(ginput_apply_warnings target_name)
    if(NOT GINPUT_STRICT)
        return()
    endif()

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
        if(GINPUT_WARN_AS_ERROR)
            target_compile_options(${target_name} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target_name} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
        )
        if(GINPUT_WARN_AS_ERROR)
            target_compile_options(${target_name} PRIVATE -Werror)
        endif()
    endif()
endfunction()

ginput_apply_warnings(ginput)

if(GINPUT_BUILD_EXAMPLES)
    add_executable(ginput_demo examples/ginput_demo.cpp)
    target_link_libraries(ginput_demo PRIVATE ginput::ginput)
    target_compile_features(ginput_demo PRIVATE cxx_std_20)
    ginput_apply_warnings(ginput_demo)
endif()

if(GINPUT_BUILD_TESTS)
    enable_testing()

    add_executable(ginput_tests tests/ginput_tests.cpp)
    target_link_libraries(ginput_tests PRIVATE ginput::ginput)
    target_compile_features(ginput_tests PRIVATE cxx_std_20)
    ginput_apply_warnings(ginput_tests)

    add_test(NAME ginput_tests COMMAND ginput_tests)
endif()
