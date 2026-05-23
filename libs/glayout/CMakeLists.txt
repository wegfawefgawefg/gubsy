cmake_minimum_required(VERSION 3.20)

project(glayout VERSION 0.1.0 LANGUAGES CXX)

option(GLAYOUT_STRICT "Enable strict compiler warnings" ON)
option(GLAYOUT_WARN_AS_ERROR "Treat warnings as errors" OFF)
option(GLAYOUT_BUILD_EXAMPLES "Build example programs" ON)
option(GLAYOUT_BUILD_TESTS "Build test programs" ON)
option(GLAYOUT_BUILD_SDL_DEMO "Build SDL3 demo program when SDL3 is available" ON)
option(GLAYOUT_WITH_IMGUI "Build optional Dear ImGui helpers" OFF)
option(GLAYOUT_FETCH_DEMO_IMGUI "Fetch Dear ImGui for the SDL demo when no ImGui is provided" ON)
set(GLAYOUT_IMGUI_TARGET "" CACHE STRING "Dear ImGui CMake target for optional ImGui helpers")
set(GLAYOUT_IMGUI_SOURCE_DIR "" CACHE PATH "Optional Dear ImGui source tree used to create an imgui target")
set(GLAYOUT_GSEXP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../gsexp"
    CACHE PATH "Path to a gsexp source tree when gsexp::gsexp is not already available")

if(NOT TARGET gsexp::gsexp)
    if(EXISTS "${GLAYOUT_GSEXP_SOURCE_DIR}/CMakeLists.txt")
        set(GSEXP_BUILD_EXAMPLES OFF CACHE BOOL "Build gsexp examples" FORCE)
        set(GSEXP_BUILD_TESTS OFF CACHE BOOL "Build gsexp tests" FORCE)
        add_subdirectory("${GLAYOUT_GSEXP_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/gsexp")
    else()
        message(FATAL_ERROR
            "gsexp::gsexp target was not found and GLAYOUT_GSEXP_SOURCE_DIR does not point "
            "to a gsexp source tree: ${GLAYOUT_GSEXP_SOURCE_DIR}")
    endif()
endif()

if(GLAYOUT_WITH_IMGUI AND NOT GLAYOUT_IMGUI_TARGET AND NOT TARGET imgui AND
   NOT TARGET ImGui::ImGui AND NOT GLAYOUT_IMGUI_SOURCE_DIR AND GLAYOUT_FETCH_DEMO_IMGUI)
    include(FetchContent)
    FetchContent_Declare(
        imgui_demo_source
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.9b
    )
    FetchContent_GetProperties(imgui_demo_source)
    if(NOT imgui_demo_source_POPULATED)
        if(POLICY CMP0169)
            cmake_policy(PUSH)
            cmake_policy(SET CMP0169 OLD)
        endif()
        FetchContent_Populate(imgui_demo_source)
        if(POLICY CMP0169)
            cmake_policy(POP)
        endif()
    endif()
    set(GLAYOUT_IMGUI_SOURCE_DIR "${imgui_demo_source_SOURCE_DIR}"
        CACHE PATH "Optional Dear ImGui source tree used to create an imgui target" FORCE)
endif()

add_library(glayout_core
    src/editor.cpp
    src/editor_overlay.cpp
    src/layout.cpp
)

add_library(glayout::core ALIAS glayout_core)

target_include_directories(glayout_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(glayout_core PUBLIC cxx_std_20)
target_link_libraries(glayout_core PUBLIC gsexp::gsexp)

function(glayout_apply_warnings target_name)
    if(NOT GLAYOUT_STRICT)
        return()
    endif()

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
        if(GLAYOUT_WARN_AS_ERROR)
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
        if(GLAYOUT_WARN_AS_ERROR)
            target_compile_options(${target_name} PRIVATE -Werror)
        endif()
    endif()
endfunction()

glayout_apply_warnings(glayout_core)

if(GLAYOUT_BUILD_EXAMPLES)
    add_executable(glayout_demo examples/glayout_demo.cpp)
    target_link_libraries(glayout_demo PRIVATE glayout::core)
    target_compile_features(glayout_demo PRIVATE cxx_std_20)
    glayout_apply_warnings(glayout_demo)
endif()

if(GLAYOUT_BUILD_SDL_DEMO)
    find_package(SDL3 CONFIG QUIET)
    if(TARGET SDL3::SDL3)
        set(GLAYOUT_SDL3_TARGET SDL3::SDL3)
    else()
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL3_PC QUIET IMPORTED_TARGET sdl3)
            if(TARGET PkgConfig::SDL3_PC)
                set(GLAYOUT_SDL3_TARGET PkgConfig::SDL3_PC)
            endif()
        endif()
    endif()

    if(GLAYOUT_SDL3_TARGET)
        add_executable(glayout_sdl_demo examples/sdl_demo/main.cpp)
        target_link_libraries(glayout_sdl_demo PRIVATE glayout::core ${GLAYOUT_SDL3_TARGET})
        target_compile_features(glayout_sdl_demo PRIVATE cxx_std_20)
        target_compile_definitions(glayout_sdl_demo PRIVATE
            GLAYOUT_SDL_DEMO_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/examples/sdl_demo/data")
        glayout_apply_warnings(glayout_sdl_demo)
    else()
        message(STATUS "SDL3 not found; skipping glayout_sdl_demo")
    endif()
endif()

set(GLAYOUT_SDL_DEMO_IMGUI_BACKENDS_AVAILABLE OFF)
if(GLAYOUT_IMGUI_SOURCE_DIR)
    set(GLAYOUT_IMGUI_SDL3_BACKEND "${GLAYOUT_IMGUI_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp")
    set(GLAYOUT_IMGUI_SDLRENDERER3_BACKEND
        "${GLAYOUT_IMGUI_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp")
    if(EXISTS "${GLAYOUT_IMGUI_SDL3_BACKEND}" AND EXISTS "${GLAYOUT_IMGUI_SDLRENDERER3_BACKEND}")
        set(GLAYOUT_SDL_DEMO_IMGUI_BACKENDS_AVAILABLE ON)
    endif()
endif()

if(GLAYOUT_BUILD_TESTS)
    enable_testing()

    add_executable(glayout_core_tests tests/core_tests.cpp)
    target_link_libraries(glayout_core_tests PRIVATE glayout::core)
    target_compile_features(glayout_core_tests PRIVATE cxx_std_20)
    target_compile_definitions(glayout_core_tests PRIVATE
        GLAYOUT_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/examples/sdl_demo/data")
    glayout_apply_warnings(glayout_core_tests)

    add_test(NAME glayout_core_tests COMMAND glayout_core_tests)
endif()

if(GLAYOUT_WITH_IMGUI)
    set(GLAYOUT_RESOLVED_IMGUI_TARGET "")
    if(GLAYOUT_IMGUI_TARGET AND TARGET ${GLAYOUT_IMGUI_TARGET})
        set(GLAYOUT_RESOLVED_IMGUI_TARGET ${GLAYOUT_IMGUI_TARGET})
    elseif(TARGET imgui)
        set(GLAYOUT_RESOLVED_IMGUI_TARGET imgui)
    elseif(TARGET ImGui::ImGui)
        set(GLAYOUT_RESOLVED_IMGUI_TARGET ImGui::ImGui)
    elseif(GLAYOUT_IMGUI_SOURCE_DIR AND EXISTS "${GLAYOUT_IMGUI_SOURCE_DIR}/imgui.cpp")
        add_library(imgui STATIC
            "${GLAYOUT_IMGUI_SOURCE_DIR}/imgui.cpp"
            "${GLAYOUT_IMGUI_SOURCE_DIR}/imgui_draw.cpp"
            "${GLAYOUT_IMGUI_SOURCE_DIR}/imgui_tables.cpp"
            "${GLAYOUT_IMGUI_SOURCE_DIR}/imgui_widgets.cpp"
        )
        target_include_directories(imgui PUBLIC "${GLAYOUT_IMGUI_SOURCE_DIR}")
        target_compile_features(imgui PUBLIC cxx_std_20)
        set(GLAYOUT_RESOLVED_IMGUI_TARGET imgui)
    endif()

    if(NOT GLAYOUT_RESOLVED_IMGUI_TARGET)
        message(FATAL_ERROR
            "GLAYOUT_WITH_IMGUI=ON requires GLAYOUT_IMGUI_TARGET, imgui, ImGui::ImGui, "
            "or GLAYOUT_IMGUI_SOURCE_DIR")
    endif()

    add_library(glayout_imgui
        src/imgui.cpp
    )
    add_library(glayout::imgui ALIAS glayout_imgui)
    target_link_libraries(glayout_imgui PUBLIC glayout::core ${GLAYOUT_RESOLVED_IMGUI_TARGET})
    target_include_directories(glayout_imgui
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_compile_features(glayout_imgui PUBLIC cxx_std_20)
    glayout_apply_warnings(glayout_imgui)
endif()

if(TARGET glayout_sdl_demo AND TARGET glayout_imgui AND GLAYOUT_SDL_DEMO_IMGUI_BACKENDS_AVAILABLE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        set_source_files_properties(
            "${GLAYOUT_IMGUI_SDL3_BACKEND}"
            "${GLAYOUT_IMGUI_SDLRENDERER3_BACKEND}"
            PROPERTIES COMPILE_OPTIONS "-Wno-conversion;-Wno-error=conversion"
        )
    endif()
    target_sources(glayout_sdl_demo PRIVATE
        "${GLAYOUT_IMGUI_SDL3_BACKEND}"
        "${GLAYOUT_IMGUI_SDLRENDERER3_BACKEND}"
    )
    target_include_directories(glayout_sdl_demo PRIVATE
        "${GLAYOUT_IMGUI_SOURCE_DIR}"
        "${GLAYOUT_IMGUI_SOURCE_DIR}/backends"
    )
    target_link_libraries(glayout_sdl_demo PRIVATE glayout::imgui)
    target_compile_definitions(glayout_sdl_demo PRIVATE GLAYOUT_SDL_DEMO_WITH_IMGUI=1)
endif()
