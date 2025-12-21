cmake_minimum_required(VERSION 3.20)

project(gubsy LANGUAGES CXX)

option(GUB_REQUIRE_DEPS "Fail CMake configure if deps are missing" ON)
option(GUB_STRICT "Enable very strict warnings" ON)
option(GUB_WARN_AS_ERROR "Treat warnings as errors" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Homebrew defaults; helps find_package locate deps on macOS.
if (APPLE)
  if (EXISTS "/opt/homebrew")
    list(PREPEND CMAKE_PREFIX_PATH "/opt/homebrew")
  endif()
  if (EXISTS "/usr/local")
    list(PREPEND CMAKE_PREFIX_PATH "/usr/local")
  endif()
endif()

# Pkg-config is optional, but used as a fallback for deps.
find_package(PkgConfig QUIET)

# Auto-discover all .cpp and .hpp files recursively in src/
file(GLOB_RECURSE GUB_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

add_executable(gubsy ${GUB_SOURCES})

# Dear ImGui sources
set(IMGUI_SOURCES
  ${CMAKE_SOURCE_DIR}/imgui/imgui.cpp
  ${CMAKE_SOURCE_DIR}/imgui/imgui_demo.cpp
  ${CMAKE_SOURCE_DIR}/imgui/imgui_draw.cpp
  ${CMAKE_SOURCE_DIR}/imgui/imgui_tables.cpp
  ${CMAKE_SOURCE_DIR}/imgui/imgui_widgets.cpp
  ${CMAKE_SOURCE_DIR}/imgui/backends/imgui_impl_sdl2.cpp
  ${CMAKE_SOURCE_DIR}/imgui/backends/imgui_impl_sdlrenderer2.cpp
)
target_sources(gubsy PRIVATE ${IMGUI_SOURCES})
set_source_files_properties(${IMGUI_SOURCES} PROPERTIES COMPILE_FLAGS "-w")

# Developer convenience: alias target and CLI symlink 'arti'
add_executable(arti ALIAS gubsy)
add_custom_command(
  TARGET gubsy POST_BUILD
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:gubsy> arti
  COMMENT "Creating 'arti' symlink -> gubsy"
)

function(target_enable_strict_warnings tgt)
  if (MSVC)
    target_compile_options(${tgt} PRIVATE
      /W4
      /permissive-
      $<$<BOOL:${GUB_WARN_AS_ERROR}>:/WX>
      # Extra warning promotions
      /w14242 /w14263 /w14265 /w14287 /we4289 /w14296 /w14311
      /w14545 /w14546 /w14547 /w14549 /w14555 /w14640 /w14826
      /w14905 /w14906 /w14928
    )
  else()
    target_compile_options(${tgt} PRIVATE
      -Wall -Wextra -Wpedantic
      -Wshadow -Wconversion -Wsign-conversion -Wdouble-promotion
      -Wformat=2 -Wundef -Wnon-virtual-dtor
      -Woverloaded-virtual -Wimplicit-fallthrough
      $<$<BOOL:${GUB_WARN_AS_ERROR}>:-Werror>
    )
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      target_compile_options(${tgt} PRIVATE -Wno-unknown-warning-option)
    else()
      target_compile_options(${tgt} PRIVATE -Wnull-dereference)
    endif()
  endif()
endfunction()

if (GUB_STRICT)
  target_enable_strict_warnings(gubsy)
else()
  if (MSVC)
    target_compile_options(gubsy PRIVATE /W3)
  else()
    target_compile_options(gubsy PRIVATE -Wall -Wextra)
  endif()
endif()

# SDL2
set(SDL2_FOUND_LOCAL OFF)
find_package(SDL2 CONFIG QUIET)
if (SDL2_FOUND)
  set(SDL2_FOUND_LOCAL ON)
  target_link_libraries(gubsy PRIVATE SDL2::SDL2)
else()
  if (PkgConfig_FOUND)
    pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
    if (SDL2_FOUND)
      set(SDL2_FOUND_LOCAL ON)
      target_link_libraries(gubsy PRIVATE PkgConfig::SDL2)
    endif()
  endif()
endif()

if (NOT SDL2_FOUND_LOCAL)
  if (GUB_REQUIRE_DEPS)
    message(FATAL_ERROR "SDL2 not found. Install libsdl2-dev or provide SDL2 via CMake or pkg-config.")
  else()
    message(WARNING "SDL2 not found. Configure passes, but build will fail without it.")
  endif()
endif()

# GLM (header-only)
set(GLM_FOUND_LOCAL OFF)
find_package(glm CONFIG QUIET)
if (glm_FOUND)
  set(GLM_FOUND_LOCAL ON)
  target_link_libraries(gubsy PRIVATE glm::glm)
else()
  if (PkgConfig_FOUND)
    pkg_check_modules(GLM QUIET IMPORTED_TARGET glm)
    if (GLM_FOUND)
      set(GLM_FOUND_LOCAL ON)
      target_link_libraries(gubsy PRIVATE PkgConfig::GLM)
    endif()
  endif()
endif()

if (NOT GLM_FOUND_LOCAL)
  find_path(GLM_INCLUDE_DIR glm/glm.hpp)
  if (GLM_INCLUDE_DIR)
    set(GLM_FOUND_LOCAL ON)
    target_include_directories(gubsy PRIVATE ${GLM_INCLUDE_DIR})
  elseif (GUB_REQUIRE_DEPS)
    message(FATAL_ERROR "glm not found. Install glm and provide glm_DIR or pkg-config glm.")
  else()
    message(WARNING "glm not found. Assuming headers available in include path.")
  endif()
endif()

# SDL2_ttf (text rendering)
set(SDL2_TTF_FOUND_LOCAL OFF)
find_package(SDL2_ttf CONFIG QUIET)
if (SDL2_ttf_FOUND)
  set(SDL2_TTF_FOUND_LOCAL ON)
  target_link_libraries(gubsy PRIVATE SDL2_ttf::SDL2_ttf)
else()
  if (PkgConfig_FOUND)
    pkg_check_modules(SDL2_TTF QUIET IMPORTED_TARGET SDL2_ttf)
    if (SDL2_TTF_FOUND)
      set(SDL2_TTF_FOUND_LOCAL ON)
      target_link_libraries(gubsy PRIVATE PkgConfig::SDL2_TTF)
    endif()
  endif()
endif()

if (NOT SDL2_TTF_FOUND_LOCAL)
  message(WARNING "SDL2_ttf not found. Numeric text overlays will be disabled at runtime if initialization fails.")
endif()

target_include_directories(gubsy PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/third_party
  ${CMAKE_SOURCE_DIR}/imgui
  ${CMAKE_SOURCE_DIR}/imgui/backends
)

add_executable(mod_server tools/mod_server/main.cpp)
target_include_directories(mod_server PRIVATE
  ${CMAKE_SOURCE_DIR}/third_party
)

# Lua 5.4 (required at runtime; build-time optional with stub)
set(LUA_FOUND_LOCAL OFF)
find_package(Lua 5.4 QUIET)
if (Lua_FOUND OR LUA_FOUND)
  set(LUA_FOUND_LOCAL ON)
  if (TARGET Lua::Lua)
    target_link_libraries(gubsy PRIVATE Lua::Lua)
  else()
    target_include_directories(gubsy PRIVATE ${LUA_INCLUDE_DIR})
    target_link_libraries(gubsy PRIVATE ${LUA_LIBRARIES})
  endif()
elseif (PkgConfig_FOUND)
  pkg_check_modules(LUA54 QUIET IMPORTED_TARGET lua5.4)
  if (LUA54_FOUND)
    set(LUA_FOUND_LOCAL ON)
    target_link_libraries(gubsy PRIVATE PkgConfig::LUA54)
  else()
    pkg_check_modules(LUA_alt QUIET IMPORTED_TARGET lua-5.4 lua54 lua)
    if (LUA_alt_FOUND)
      set(LUA_FOUND_LOCAL ON)
      target_link_libraries(gubsy PRIVATE PkgConfig::LUA_alt)
    endif()
  endif()
endif()
if (LUA_FOUND_LOCAL)
  target_compile_definitions(gubsy PRIVATE GUB_ENABLE_LUA=1)
else()
  message(FATAL_ERROR "Lua 5.4 not found. Install lua5.4 (liblua5.4-dev) or brew install lua.")
endif()

# SDL2_image for textures
set(SDL2_IMAGE_FOUND_LOCAL OFF)
find_package(SDL2_image CONFIG QUIET)
if (SDL2_image_FOUND)
  set(SDL2_IMAGE_FOUND_LOCAL ON)
  target_link_libraries(gubsy PRIVATE SDL2_image::SDL2_image)
elseif (PkgConfig_FOUND)
  pkg_check_modules(SDL2_IMAGE QUIET IMPORTED_TARGET SDL2_image)
  if (SDL2_IMAGE_FOUND)
    set(SDL2_IMAGE_FOUND_LOCAL ON)
    target_link_libraries(gubsy PRIVATE PkgConfig::SDL2_IMAGE)
  endif()
endif()

# SDL2_mixer for audio
set(SDL2_MIXER_FOUND_LOCAL OFF)
find_package(SDL2_mixer CONFIG QUIET)
if (SDL2_mixer_FOUND)
  set(SDL2_MIXER_FOUND_LOCAL ON)
  target_link_libraries(gubsy PRIVATE SDL2_mixer::SDL2_mixer)
elseif (PkgConfig_FOUND)
  pkg_check_modules(SDL2_MIXER QUIET IMPORTED_TARGET SDL2_mixer)
  if (SDL2_MIXER_FOUND)
    set(SDL2_MIXER_FOUND_LOCAL ON)
    target_link_libraries(gubsy PRIVATE PkgConfig::SDL2_MIXER)
  endif()
endif()
if (NOT SDL2_MIXER_FOUND_LOCAL)
  find_path(SDL2_MIXER_INCLUDE_DIR NAMES SDL2/SDL_mixer.h SDL_mixer.h)
  find_library(SDL2_MIXER_LIBRARY NAMES SDL2_mixer)
  if (SDL2_MIXER_INCLUDE_DIR AND SDL2_MIXER_LIBRARY)
    set(SDL2_MIXER_FOUND_LOCAL ON)
    target_link_libraries(gubsy PRIVATE ${SDL2_MIXER_LIBRARY})
    set(SDL2_MIXER_INCLUDE_DIRS ${SDL2_MIXER_INCLUDE_DIR})
    if (EXISTS "${SDL2_MIXER_INCLUDE_DIR}/SDL2/SDL_mixer.h")
      list(APPEND SDL2_MIXER_INCLUDE_DIRS "${SDL2_MIXER_INCLUDE_DIR}/SDL2")
    else()
      get_filename_component(SDL2_MIXER_INCLUDE_PARENT "${SDL2_MIXER_INCLUDE_DIR}" DIRECTORY)
      if (EXISTS "${SDL2_MIXER_INCLUDE_PARENT}/SDL2/SDL_mixer.h")
        list(APPEND SDL2_MIXER_INCLUDE_DIRS "${SDL2_MIXER_INCLUDE_PARENT}")
      endif()
    endif()
    list(REMOVE_DUPLICATES SDL2_MIXER_INCLUDE_DIRS)
    target_include_directories(gubsy PRIVATE ${SDL2_MIXER_INCLUDE_DIRS})
  endif()
endif()
if (NOT SDL2_MIXER_FOUND_LOCAL)
  if (GUB_REQUIRE_DEPS)
    message(FATAL_ERROR "SDL2_mixer not found. Install libsdl2-mixer-dev.")
  else()
    message(WARNING "SDL2_mixer not found. Audio playback will not work.")
  endif()
endif()

# sol2 (optional header-only). If found, enable GUB_USE_SOL2
if (EXISTS "${CMAKE_SOURCE_DIR}/sol/sol.hpp")
  set(SOL2_INCLUDE_DIR ${CMAKE_SOURCE_DIR})
endif()
find_path(SOL2_INCLUDE_DIR ${SOL2_INCLUDE_DIR} sol/sol.hpp)
if (SOL2_INCLUDE_DIR)
  target_compile_definitions(gubsy PRIVATE GUB_USE_SOL2=1)
  # Treat sol2 as a system include to avoid third-party warnings-as-errors
  target_include_directories(gubsy SYSTEM PRIVATE ${SOL2_INCLUDE_DIR})
  message(STATUS "Found sol2: ${SOL2_INCLUDE_DIR}")
else()
  message(STATUS "sol2 not found; using Lua C API path")
endif()
if (NOT SDL2_IMAGE_FOUND_LOCAL)
  if (GUB_REQUIRE_DEPS)
    message(FATAL_ERROR "SDL2_image not found. Install libsdl2-image-dev.")
  else()
    message(WARNING "SDL2_image not found. Texture loading will not work.")
  endif()
endif()

find_package(Threads REQUIRED)
target_link_libraries(gubsy PRIVATE Threads::Threads)
target_link_libraries(mod_server PRIVATE Threads::Threads)
