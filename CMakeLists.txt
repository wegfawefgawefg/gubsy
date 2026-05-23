cmake_minimum_required(VERSION 3.20)

project(gubsy LANGUAGES C CXX)
include(CTest)

set(GUB_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(GUB_DEFAULT_BUILD_EXTRAS ON)
else()
  set(GUB_DEFAULT_BUILD_EXTRAS OFF)
endif()

option(GUB_REQUIRE_DEPS "Fail CMake configure if deps are missing" ON)
option(GUB_STRICT "Enable very strict warnings" ON)
option(GUB_WARN_AS_ERROR "Treat warnings as errors" ON)
option(GUB_BUILD_SAMPLE "Build bundled sample game consumer" ${GUB_DEFAULT_BUILD_EXTRAS})
option(GUB_BUILD_TOOLS "Build bundled development tools and smokes" ${GUB_DEFAULT_BUILD_EXTRAS})
option(GUB_COMPILE_IMGUI_SOURCES "Compile bundled Dear ImGui sources into gubsy::engine" ON)
option(GUB_ENABLE_LUA_MOD_HOST "Build Lua-backed mod hosting support" ${GUB_DEFAULT_BUILD_EXTRAS})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_definitions(GUB_PROJECT_SOURCE_DIR="${GUB_SOURCE_DIR}")

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

# Auto-discover library and bundled demo sources separately.
file(GLOB_RECURSE GUB_ENGINE_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)
if (GUB_ENABLE_LUA_MOD_HOST)
  list(REMOVE_ITEM GUB_ENGINE_SOURCES "${GUB_SOURCE_DIR}/src/mod_host_disabled.cpp")
else()
  list(REMOVE_ITEM GUB_ENGINE_SOURCES "${GUB_SOURCE_DIR}/src/mod_host.cpp")
endif()
file(GLOB_RECURSE GUB_DEMO_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/demo/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/demo/*.hpp"
)

add_library(gubsy_engine STATIC ${GUB_ENGINE_SOURCES})
add_library(gubsy::engine ALIAS gubsy_engine)

if (GUB_ENABLE_LUA_MOD_HOST AND EXISTS "${GUB_SOURCE_DIR}/third_party/lua/src/lua.h")
  file(GLOB GUB_LUA_SOURCES "${GUB_SOURCE_DIR}/third_party/lua/src/*.c")
  list(REMOVE_ITEM GUB_LUA_SOURCES
    "${GUB_SOURCE_DIR}/third_party/lua/src/lua.c"
    "${GUB_SOURCE_DIR}/third_party/lua/src/luac.c"
    "${GUB_SOURCE_DIR}/third_party/lua/src/onelua.c"
  )
  add_library(gubsy_lua STATIC ${GUB_LUA_SOURCES})
  target_include_directories(gubsy_lua PUBLIC "${GUB_SOURCE_DIR}/third_party/lua/src")
  target_compile_definitions(gubsy_lua PRIVATE LUA_USE_LINUX)
  target_link_libraries(gubsy_lua PUBLIC ${CMAKE_DL_LIBS} m)
endif()

# Dear ImGui sources. Embedders that already compile ImGui can disable this to
# avoid duplicate symbols while still letting Gubsy compile against its headers.
if (GUB_COMPILE_IMGUI_SOURCES)
  set(IMGUI_SOURCES
    ${GUB_SOURCE_DIR}/imgui/imgui.cpp
    ${GUB_SOURCE_DIR}/imgui/imgui_demo.cpp
    ${GUB_SOURCE_DIR}/imgui/imgui_draw.cpp
    ${GUB_SOURCE_DIR}/imgui/imgui_tables.cpp
    ${GUB_SOURCE_DIR}/imgui/imgui_widgets.cpp
    ${GUB_SOURCE_DIR}/imgui/backends/imgui_impl_sdl3.cpp
    ${GUB_SOURCE_DIR}/imgui/backends/imgui_impl_sdlrenderer3.cpp
  )
  target_sources(gubsy_engine PRIVATE ${IMGUI_SOURCES})
  set_source_files_properties(${IMGUI_SOURCES} PROPERTIES COMPILE_FLAGS "-w")
endif()

if (GUB_BUILD_SAMPLE)
  add_executable(gubsy ${GUB_DEMO_SOURCES})
endif()

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
  target_enable_strict_warnings(gubsy_engine)
  if (GUB_BUILD_SAMPLE)
    target_enable_strict_warnings(gubsy)
  endif()
else()
  if (MSVC)
    target_compile_options(gubsy_engine PRIVATE /W3)
    if (GUB_BUILD_SAMPLE)
      target_compile_options(gubsy PRIVATE /W3)
    endif()
  else()
    target_compile_options(gubsy_engine PRIVATE -Wall -Wextra)
    if (GUB_BUILD_SAMPLE)
      target_compile_options(gubsy PRIVATE -Wall -Wextra)
    endif()
  endif()
endif()

# SDL3
set(SDL3_FOUND_LOCAL OFF)
find_package(SDL3 CONFIG QUIET)
if (SDL3_FOUND)
  set(SDL3_FOUND_LOCAL ON)
  if (TARGET SDL3::SDL3)
    target_link_libraries(gubsy_engine PUBLIC SDL3::SDL3)
  elseif (TARGET SDL3::SDL3-shared)
    target_link_libraries(gubsy_engine PUBLIC SDL3::SDL3-shared)
  else()
    target_link_libraries(gubsy_engine PUBLIC SDL3::SDL3-static)
  endif()
else()
  if (PkgConfig_FOUND)
    pkg_check_modules(SDL3 QUIET IMPORTED_TARGET sdl3)
    if (SDL3_FOUND)
      set(SDL3_FOUND_LOCAL ON)
      target_link_libraries(gubsy_engine PUBLIC PkgConfig::SDL3)
    endif()
  endif()
endif()

if (NOT SDL3_FOUND_LOCAL)
  if (GUB_REQUIRE_DEPS)
    message(FATAL_ERROR "SDL3 not found. Install libsdl3-dev or provide SDL3 via CMake or pkg-config.")
  else()
    message(WARNING "SDL3 not found. Configure passes, but build will fail without it.")
  endif()
endif()

# GLM (header-only)
set(GLM_FOUND_LOCAL OFF)
find_package(glm CONFIG QUIET)
if (glm_FOUND)
  set(GLM_FOUND_LOCAL ON)
  target_link_libraries(gubsy_engine PUBLIC glm::glm)
else()
  if (PkgConfig_FOUND)
    pkg_check_modules(GLM QUIET IMPORTED_TARGET glm)
    if (GLM_FOUND)
      set(GLM_FOUND_LOCAL ON)
      target_link_libraries(gubsy_engine PUBLIC PkgConfig::GLM)
    endif()
  endif()
endif()

if (NOT GLM_FOUND_LOCAL)
  find_path(GLM_INCLUDE_DIR glm/glm.hpp
    PATHS
      ${GUB_SOURCE_DIR}/third_party/glm_upstream
  )
  if (GLM_INCLUDE_DIR)
    set(GLM_FOUND_LOCAL ON)
    target_include_directories(gubsy_engine SYSTEM PUBLIC ${GLM_INCLUDE_DIR})
  elseif (GUB_REQUIRE_DEPS)
    message(FATAL_ERROR "glm not found. Install glm and provide glm_DIR or pkg-config glm.")
  else()
    message(WARNING "glm not found. Assuming headers available in include path.")
  endif()
endif()

# SDL3_ttf (text rendering)
set(SDL3_TTF_FOUND_LOCAL OFF)
find_package(SDL3_ttf CONFIG QUIET)
if (SDL3_ttf_FOUND)
  set(SDL3_TTF_FOUND_LOCAL ON)
  if (TARGET SDL3_ttf::SDL3_ttf)
    target_link_libraries(gubsy_engine PUBLIC SDL3_ttf::SDL3_ttf)
  elseif (TARGET SDL3_ttf::SDL3_ttf-shared)
    target_link_libraries(gubsy_engine PUBLIC SDL3_ttf::SDL3_ttf-shared)
  else()
    target_link_libraries(gubsy_engine PUBLIC SDL3_ttf::SDL3_ttf-static)
  endif()
else()
  if (PkgConfig_FOUND)
    pkg_check_modules(SDL3_TTF QUIET IMPORTED_TARGET sdl3-ttf)
    if (SDL3_TTF_FOUND)
      set(SDL3_TTF_FOUND_LOCAL ON)
      target_link_libraries(gubsy_engine PUBLIC PkgConfig::SDL3_TTF)
    endif()
  endif()
endif()

if (NOT SDL3_TTF_FOUND_LOCAL)
  message(WARNING "SDL3_ttf not found. Numeric text overlays will be disabled at runtime if initialization fails.")
endif()

target_include_directories(gubsy_engine BEFORE
  PUBLIC
    ${GUB_SOURCE_DIR}/include
  PRIVATE
    ${GUB_SOURCE_DIR}
    ${GUB_SOURCE_DIR}/imgui
    ${GUB_SOURCE_DIR}/imgui/backends
)
target_include_directories(gubsy_engine SYSTEM PUBLIC
  ${GUB_SOURCE_DIR}/third_party
)
if (GUB_BUILD_SAMPLE)
  target_include_directories(gubsy SYSTEM PRIVATE
    ${GUB_SOURCE_DIR}
    ${GUB_SOURCE_DIR}/imgui
    ${GUB_SOURCE_DIR}/third_party/lua/src
  )
  target_link_libraries(gubsy PRIVATE gubsy_engine)

  # Developer convenience: alias target and CLI symlink 'arti'
  add_executable(arti ALIAS gubsy)
  add_custom_command(
    TARGET gubsy POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:gubsy> arti
    COMMENT "Creating 'arti' symlink -> gubsy"
  )
endif()

if (GUB_BUILD_TOOLS)
  add_executable(public_api_smoke tools/public_api_smoke/main.cpp)
  target_link_libraries(public_api_smoke PRIVATE gubsy_engine)
  target_enable_strict_warnings(public_api_smoke)

  if (BUILD_TESTING)
    add_test(NAME public_api_smoke COMMAND public_api_smoke)
    add_test(
      NAME consumption_boundary
      COMMAND "${GUB_SOURCE_DIR}/scripts/check_consumption_boundary.sh"
    )
    add_test(
      NAME consumer_smoke
      COMMAND "${GUB_SOURCE_DIR}/scripts/consumer_smoke.sh"
    )
  endif()

  add_executable(mod_server
    tools/mod_server/main.cpp
    src/project_paths.cpp
  )
  target_include_directories(mod_server PRIVATE
    ${GUB_SOURCE_DIR}/include
    ${GUB_SOURCE_DIR}
    ${GUB_SOURCE_DIR}/third_party
  )

  add_executable(room_server tools/room_server/main.cpp)
  target_include_directories(room_server PRIVATE
    ${GUB_SOURCE_DIR}
    ${GUB_SOURCE_DIR}/third_party
  )

  add_executable(room_smoke
    tools/room_smoke/main.cpp
    src/room_matchmaking.cpp
    src/session_contract.cpp
  )
  target_include_directories(room_smoke PRIVATE
    ${GUB_SOURCE_DIR}/include
    ${GUB_SOURCE_DIR}
    ${GUB_SOURCE_DIR}/third_party
  )

  add_executable(room_timeout_smoke
    tools/room_timeout_smoke/main.cpp
    src/room_matchmaking.cpp
    src/session_contract.cpp
  )
  target_include_directories(room_timeout_smoke PRIVATE
    ${GUB_SOURCE_DIR}/include
    ${GUB_SOURCE_DIR}
    ${GUB_SOURCE_DIR}/third_party
  )

  add_executable(coop_sync_smoke
    tools/coop_sync_smoke/main.cpp
    src/room_matchmaking.cpp
    src/session_contract.cpp
    src/sync_payload_codec.cpp
    src/sync_transport_udp.cpp
    src/sync_transport_packet_codec.cpp
    demo/coop_protocol.cpp
    demo/coop_sim.cpp
  )
  target_include_directories(coop_sync_smoke PRIVATE
    ${GUB_SOURCE_DIR}/include
    ${GUB_SOURCE_DIR}
    ${GUB_SOURCE_DIR}/third_party
  )
  target_include_directories(coop_sync_smoke SYSTEM PRIVATE ${GUB_SOURCE_DIR}/third_party/glm_upstream)
endif()

# Lua 5.4. Embedders can disable this when they do not want Gubsy's Lua mod
# host compiled into the engine target.
set(LUA_FOUND_LOCAL OFF)
if (NOT GUB_ENABLE_LUA_MOD_HOST)
  target_compile_definitions(gubsy_engine PUBLIC GUB_ENABLE_LUA=0)
elseif (TARGET gubsy_lua)
  set(LUA_FOUND_LOCAL ON)
  target_link_libraries(gubsy_engine PRIVATE gubsy_lua)
else()
find_package(Lua 5.4 QUIET)
if (Lua_FOUND OR LUA_FOUND)
  set(LUA_FOUND_LOCAL ON)
  if (TARGET Lua::Lua)
    target_link_libraries(gubsy_engine PUBLIC Lua::Lua)
  else()
    target_include_directories(gubsy_engine PUBLIC ${LUA_INCLUDE_DIR})
    target_link_libraries(gubsy_engine PUBLIC ${LUA_LIBRARIES})
  endif()
elseif (PkgConfig_FOUND)
  pkg_check_modules(LUA54 QUIET IMPORTED_TARGET lua5.4)
  if (LUA54_FOUND)
    set(LUA_FOUND_LOCAL ON)
    target_link_libraries(gubsy_engine PUBLIC PkgConfig::LUA54)
  else()
    pkg_check_modules(LUA_alt QUIET IMPORTED_TARGET lua-5.4 lua54 lua)
    if (LUA_alt_FOUND)
      set(LUA_FOUND_LOCAL ON)
      target_link_libraries(gubsy_engine PUBLIC PkgConfig::LUA_alt)
    endif()
  endif()
endif()
endif()
if (LUA_FOUND_LOCAL)
  target_compile_definitions(gubsy_engine PUBLIC GUB_ENABLE_LUA=1)
elseif (GUB_ENABLE_LUA_MOD_HOST)
  message(FATAL_ERROR "Lua 5.4 not found. Install lua5.4 (liblua5.4-dev) or brew install lua.")
endif()

# SDL3_image for textures
set(SDL3_IMAGE_FOUND_LOCAL OFF)
find_package(SDL3_image CONFIG QUIET)
if (SDL3_image_FOUND)
  set(SDL3_IMAGE_FOUND_LOCAL ON)
  if (TARGET SDL3_image::SDL3_image)
    target_link_libraries(gubsy_engine PUBLIC SDL3_image::SDL3_image)
  elseif (TARGET SDL3_image::SDL3_image-shared)
    target_link_libraries(gubsy_engine PUBLIC SDL3_image::SDL3_image-shared)
  else()
    target_link_libraries(gubsy_engine PUBLIC SDL3_image::SDL3_image-static)
  endif()
elseif (PkgConfig_FOUND)
  pkg_check_modules(SDL3_IMAGE QUIET IMPORTED_TARGET sdl3-image)
  if (SDL3_IMAGE_FOUND)
    set(SDL3_IMAGE_FOUND_LOCAL ON)
    target_link_libraries(gubsy_engine PUBLIC PkgConfig::SDL3_IMAGE)
  endif()
endif()

# SDL3_mixer for audio. The game can run without it; audio calls become no-ops.
set(SDL3_MIXER_FOUND_LOCAL OFF)
find_package(SDL3_mixer CONFIG QUIET)
if (SDL3_mixer_FOUND)
  set(SDL3_MIXER_FOUND_LOCAL ON)
  if (TARGET SDL3_mixer::SDL3_mixer)
    target_link_libraries(gubsy_engine PUBLIC SDL3_mixer::SDL3_mixer)
  elseif (TARGET SDL3_mixer::SDL3_mixer-shared)
    target_link_libraries(gubsy_engine PUBLIC SDL3_mixer::SDL3_mixer-shared)
  else()
    target_link_libraries(gubsy_engine PUBLIC SDL3_mixer::SDL3_mixer-static)
  endif()
elseif (PkgConfig_FOUND)
  pkg_check_modules(SDL3_MIXER QUIET IMPORTED_TARGET sdl3-mixer)
  if (SDL3_MIXER_FOUND)
    set(SDL3_MIXER_FOUND_LOCAL ON)
    target_link_libraries(gubsy_engine PUBLIC PkgConfig::SDL3_MIXER)
  endif()
endif()
if (SDL3_MIXER_FOUND_LOCAL)
  target_compile_definitions(gubsy_engine PUBLIC GUB_ENABLE_SDL_MIXER=1)
else()
  target_compile_definitions(gubsy_engine PUBLIC GUB_ENABLE_SDL_MIXER=0)
  message(WARNING "SDL3_mixer not found. Audio playback will be disabled.")
endif()

# sol2 (optional header-only). Only needed when the Lua mod host is built.
if (GUB_ENABLE_LUA_MOD_HOST)
  if (EXISTS "${GUB_SOURCE_DIR}/sol/sol.hpp")
    set(SOL2_INCLUDE_DIR ${GUB_SOURCE_DIR})
  endif()
  find_path(SOL2_INCLUDE_DIR ${SOL2_INCLUDE_DIR} sol/sol.hpp)
  if (SOL2_INCLUDE_DIR)
    target_compile_definitions(gubsy_engine PUBLIC GUB_USE_SOL2=1)
    # Treat sol2 as a system include to avoid third-party warnings-as-errors
    target_include_directories(gubsy_engine SYSTEM PRIVATE ${SOL2_INCLUDE_DIR})
    message(STATUS "Found sol2: ${SOL2_INCLUDE_DIR}")
  else()
    message(STATUS "sol2 not found; using Lua C API path")
  endif()
endif()
if (NOT SDL3_IMAGE_FOUND_LOCAL)
  if (GUB_REQUIRE_DEPS)
    message(FATAL_ERROR "SDL3_image not found. Install libsdl3-image-dev.")
  else()
    message(WARNING "SDL3_image not found. Texture loading will not work.")
  endif()
endif()

find_package(Threads REQUIRED)
target_link_libraries(gubsy_engine PUBLIC Threads::Threads)
if (GUB_BUILD_TOOLS)
  target_link_libraries(mod_server PRIVATE Threads::Threads)
endif()
