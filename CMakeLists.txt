cmake_minimum_required(VERSION 3.21)

project(gubsy LANGUAGES C CXX)
include(CTest)
include(FetchContent)

set(GUB_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(GUB_INFERRED_MODE developer)
else()
  set(GUB_INFERRED_MODE consumer)
endif()
set(GUB_MODE "${GUB_INFERRED_MODE}" CACHE STRING "Gubsy build mode: developer, consumer, or release")
set_property(CACHE GUB_MODE PROPERTY STRINGS developer consumer release)
if (NOT GUB_MODE MATCHES "^(developer|consumer|release)$")
  message(FATAL_ERROR "GUB_MODE must be one of: developer, consumer, release")
endif()

if (GUB_MODE STREQUAL "consumer")
  set(GUB_DEFAULT_BUILD_EXTRAS OFF)
else()
  set(GUB_DEFAULT_BUILD_EXTRAS ON)
endif()

option(GUB_REQUIRE_DEPS "Fail CMake configure if deps are missing" ON)
option(GUB_STRICT "Enable very strict warnings" ON)
option(GUB_WARN_AS_ERROR "Treat warnings as errors" ON)
option(GUB_BUILD_SAMPLE "Build bundled sample game" ${GUB_DEFAULT_BUILD_EXTRAS})
option(GUB_BUILD_TOOLS "Build bundled development tools and smokes" ${GUB_DEFAULT_BUILD_EXTRAS})
option(GUB_COMPILE_IMGUI_SOURCES "Compile bundled Dear ImGui sources into gubsy::engine" ON)
option(GUB_ENABLE_LUA_MOD_HOST "Build Lua-backed mod hosting support" ${GUB_DEFAULT_BUILD_EXTRAS})
option(GUB_ENABLE_STEAM "Compile Steam backend scaffolding" OFF)
option(GUB_FETCH_DEPS "Fetch missing SDL dependencies with CMake FetchContent" ON)

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

function(gub_link_sdl3_target tgt)
  if (TARGET SDL3::SDL3)
    target_link_libraries(${tgt} PUBLIC SDL3::SDL3)
  elseif (TARGET SDL3::SDL3-shared)
    target_link_libraries(${tgt} PUBLIC SDL3::SDL3-shared)
  elseif (TARGET SDL3::SDL3-static)
    target_link_libraries(${tgt} PUBLIC SDL3::SDL3-static)
  else()
    message(FATAL_ERROR "SDL3 was found, but no supported SDL3 CMake target exists.")
  endif()
endfunction()

function(gub_link_sdl3_ttf_target tgt)
  if (TARGET SDL3_ttf::SDL3_ttf)
    target_link_libraries(${tgt} PUBLIC SDL3_ttf::SDL3_ttf)
  elseif (TARGET SDL3_ttf::SDL3_ttf-shared)
    target_link_libraries(${tgt} PUBLIC SDL3_ttf::SDL3_ttf-shared)
  elseif (TARGET SDL3_ttf::SDL3_ttf-static)
    target_link_libraries(${tgt} PUBLIC SDL3_ttf::SDL3_ttf-static)
  else()
    message(FATAL_ERROR "SDL3_ttf was found, but no supported SDL3_ttf CMake target exists.")
  endif()
endfunction()

function(gub_link_sdl3_image_target tgt)
  if (TARGET SDL3_image::SDL3_image)
    target_link_libraries(${tgt} PUBLIC SDL3_image::SDL3_image)
  elseif (TARGET SDL3_image::SDL3_image-shared)
    target_link_libraries(${tgt} PUBLIC SDL3_image::SDL3_image-shared)
  elseif (TARGET SDL3_image::SDL3_image-static)
    target_link_libraries(${tgt} PUBLIC SDL3_image::SDL3_image-static)
  else()
    message(FATAL_ERROR "SDL3_image was found, but no supported SDL3_image CMake target exists.")
  endif()
endfunction()

function(gub_link_sdl3_mixer_target tgt)
  if (TARGET SDL3_mixer::SDL3_mixer)
    target_link_libraries(${tgt} PUBLIC SDL3_mixer::SDL3_mixer)
  elseif (TARGET SDL3_mixer::SDL3_mixer-shared)
    target_link_libraries(${tgt} PUBLIC SDL3_mixer::SDL3_mixer-shared)
  elseif (TARGET SDL3_mixer::SDL3_mixer-static)
    target_link_libraries(${tgt} PUBLIC SDL3_mixer::SDL3_mixer-static)
  else()
    message(FATAL_ERROR "SDL3_mixer was found, but no supported SDL3_mixer CMake target exists.")
  endif()
endfunction()

function(gub_copy_runtime_dlls tgt)
  if (WIN32)
    add_custom_command(
      TARGET ${tgt} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:${tgt}> $<TARGET_FILE_DIR:${tgt}>
      COMMAND_EXPAND_LISTS
    )
  endif()
endfunction()

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

if (GUB_ENABLE_STEAM)
  target_compile_definitions(gubsy_engine PUBLIC GUB_ENABLE_STEAM=1)
else()
  target_compile_definitions(gubsy_engine PUBLIC GUB_ENABLE_STEAM=0)
endif()

if (GUB_ENABLE_LUA_MOD_HOST AND EXISTS "${GUB_SOURCE_DIR}/third_party/lua/src/lua.h")
  file(GLOB GUB_LUA_SOURCES "${GUB_SOURCE_DIR}/third_party/lua/src/*.c")
  list(REMOVE_ITEM GUB_LUA_SOURCES
    "${GUB_SOURCE_DIR}/third_party/lua/src/lua.c"
    "${GUB_SOURCE_DIR}/third_party/lua/src/luac.c"
    "${GUB_SOURCE_DIR}/third_party/lua/src/onelua.c"
  )
  add_library(gubsy_lua STATIC ${GUB_LUA_SOURCES})
  target_include_directories(gubsy_lua PUBLIC "${GUB_SOURCE_DIR}/third_party/lua/src")
  if (WIN32)
    target_compile_definitions(gubsy_lua PRIVATE LUA_USE_WINDOWS)
  elseif (APPLE)
    target_compile_definitions(gubsy_lua PRIVATE LUA_USE_MACOSX)
    target_link_libraries(gubsy_lua PUBLIC ${CMAKE_DL_LIBS} m)
  else()
    target_compile_definitions(gubsy_lua PRIVATE LUA_USE_LINUX)
    target_link_libraries(gubsy_lua PUBLIC ${CMAKE_DL_LIBS} m)
  endif()
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
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(gubsy PRIVATE -Wno-array-bounds)
      if (WIN32)
        target_compile_options(gubsy PRIVATE -Wno-null-dereference)
      endif()
    endif()
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
set(GUB_TRIED_FIND_PACKAGE_SDL3 OFF)
if (TARGET SDL3::SDL3)
  set(SDL3_FOUND_LOCAL ON)
  gub_link_sdl3_target(gubsy_engine)
elseif (TARGET SDL3::SDL3-shared)
  set(SDL3_FOUND_LOCAL ON)
  gub_link_sdl3_target(gubsy_engine)
elseif (TARGET SDL3::SDL3-static)
  set(SDL3_FOUND_LOCAL ON)
  gub_link_sdl3_target(gubsy_engine)
else()
  find_package(SDL3 CONFIG QUIET)
  set(GUB_TRIED_FIND_PACKAGE_SDL3 ON)
endif()
if (GUB_TRIED_FIND_PACKAGE_SDL3 AND SDL3_FOUND)
  set(SDL3_FOUND_LOCAL ON)
  gub_link_sdl3_target(gubsy_engine)
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
  if (GUB_FETCH_DEPS)
    set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
      SDL3
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      # release-3.4.0
      GIT_TAG a962f40bbba175e9716557a25d5d7965f134a3d3
    )
    FetchContent_MakeAvailable(SDL3)
    set(SDL3_FOUND_LOCAL ON)
    gub_link_sdl3_target(gubsy_engine)
  elseif (GUB_REQUIRE_DEPS)
    message(FATAL_ERROR "SDL3 not found. Install libsdl3-dev, provide SDL3 via CMake/pkg-config, or enable GUB_FETCH_DEPS.")
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
  gub_link_sdl3_ttf_target(gubsy_engine)
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
  if (GUB_FETCH_DEPS)
    if (DEFINED sdl3_BINARY_DIR AND EXISTS "${sdl3_BINARY_DIR}/SDL3Config.cmake")
      set(SDL3_DIR "${sdl3_BINARY_DIR}" CACHE PATH "" FORCE)
    endif()
    FetchContent_Declare(
      SDL3_ttf
      GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
      # release-3.2.2
      GIT_TAG a1ce3670aec736ecbf0936c43f2f0cc53aa61e5b
    )
    set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL3_ttf)
    set(SDL3_TTF_FOUND_LOCAL ON)
    gub_link_sdl3_ttf_target(gubsy_engine)
  else()
    message(WARNING "SDL3_ttf not found. Numeric text overlays will be disabled at runtime if initialization fails.")
  endif()
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
  gub_copy_runtime_dlls(gubsy)

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
  gub_copy_runtime_dlls(public_api_smoke)

  if (BUILD_TESTING)
    add_test(NAME public_api_smoke COMMAND public_api_smoke)
    add_test(NAME lobby_config_smoke COMMAND lobby_config_smoke)
    add_test(NAME lobby_input_smoke COMMAND lobby_input_smoke)
    add_test(NAME room_rendezvous_smoke COMMAND room_rendezvous_smoke)
    add_test(
      NAME lobby_online_smoke
      COMMAND bash "${GUB_SOURCE_DIR}/scripts/lobby_online_smoke.sh"
    )
    add_test(
      NAME room_punch_smoke
      COMMAND bash "${GUB_SOURCE_DIR}/scripts/room_punch_smoke.sh"
    )
    add_test(
      NAME consumption_boundary
      COMMAND bash "${GUB_SOURCE_DIR}/scripts/check_consumption_boundary.sh"
    )
    add_test(
      NAME sdl3_shim_guard
      COMMAND bash "${GUB_SOURCE_DIR}/scripts/check_sdl3_shim_cleanup.sh"
    )
    add_test(
      NAME consumer_smoke
      COMMAND bash "${GUB_SOURCE_DIR}/scripts/consumer_smoke.sh"
    )
  endif()

  add_executable(mod_server
    tools/mod_server/main.cpp
    src/project_paths.cpp
  )
  target_link_libraries(mod_server PRIVATE gubsy_engine)
  target_include_directories(mod_server PRIVATE
    ${GUB_SOURCE_DIR}/include
    ${GUB_SOURCE_DIR}
    ${GUB_SOURCE_DIR}/third_party
  )

  add_executable(gubsy_roomd
    tools/room_server/main.cpp
  )
  set_target_properties(gubsy_roomd PROPERTIES OUTPUT_NAME gubsy-roomd)
  target_include_directories(gubsy_roomd PRIVATE
    ${GUB_SOURCE_DIR}
    ${GUB_SOURCE_DIR}/third_party
  )
  target_link_libraries(gubsy_roomd PRIVATE gubsy_engine)
  if (WIN32)
    target_link_libraries(gubsy_roomd PRIVATE ws2_32)
  endif()
  add_custom_command(
    TARGET gubsy_roomd POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:gubsy_roomd> room_server
    COMMENT "Creating 'room_server' symlink -> gubsy-roomd"
  )

  add_executable(room_smoke
    tools/room_smoke/main.cpp
    src/connection_cascade.cpp
    src/room_matchmaking.cpp
    src/session_contract.cpp
  )
  target_include_directories(room_smoke PRIVATE
    ${GUB_SOURCE_DIR}/include
    ${GUB_SOURCE_DIR}
    ${GUB_SOURCE_DIR}/third_party
  )
  if (WIN32)
    target_link_libraries(room_smoke PRIVATE ws2_32)
  endif()

  add_executable(room_rendezvous_smoke
    tools/room_rendezvous_smoke/main.cpp
  )
  target_include_directories(room_rendezvous_smoke PRIVATE
    ${GUB_SOURCE_DIR}
    ${GUB_SOURCE_DIR}/third_party
  )
  target_link_libraries(room_rendezvous_smoke PRIVATE gubsy_engine)

  add_executable(room_punch_smoke
    tools/room_punch_smoke/main.cpp
  )
  target_include_directories(room_punch_smoke PRIVATE
    ${GUB_SOURCE_DIR}
    ${GUB_SOURCE_DIR}/third_party
  )
  target_link_libraries(room_punch_smoke PRIVATE gubsy_engine)
  if (WIN32)
    target_link_libraries(room_punch_smoke PRIVATE ws2_32)
  endif()

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
  if (WIN32)
    target_link_libraries(room_timeout_smoke PRIVATE ws2_32)
  endif()

  add_executable(lobby_config_smoke tools/lobby_config_smoke/main.cpp)
  target_link_libraries(lobby_config_smoke PRIVATE gubsy_engine)
  target_include_directories(lobby_config_smoke PRIVATE
    ${GUB_SOURCE_DIR}/include
    ${GUB_SOURCE_DIR}
    ${GUB_SOURCE_DIR}/third_party
  )
  target_enable_strict_warnings(lobby_config_smoke)
  gub_copy_runtime_dlls(lobby_config_smoke)

  add_executable(lobby_input_smoke tools/lobby_input_smoke/main.cpp)
  target_link_libraries(lobby_input_smoke PRIVATE gubsy_engine)
  target_include_directories(lobby_input_smoke PRIVATE
    ${GUB_SOURCE_DIR}/include
    ${GUB_SOURCE_DIR}
    ${GUB_SOURCE_DIR}/third_party
  )
  target_enable_strict_warnings(lobby_input_smoke)
  gub_copy_runtime_dlls(lobby_input_smoke)

  add_executable(lobby_online_smoke tools/lobby_online_smoke/main.cpp)
  target_link_libraries(lobby_online_smoke PRIVATE gubsy_engine)
  target_include_directories(lobby_online_smoke PRIVATE
    ${GUB_SOURCE_DIR}/include
    ${GUB_SOURCE_DIR}
    ${GUB_SOURCE_DIR}/third_party
  )
  target_enable_strict_warnings(lobby_online_smoke)
  gub_copy_runtime_dlls(lobby_online_smoke)

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
  if (WIN32)
    target_link_libraries(coop_sync_smoke PRIVATE ws2_32)
  endif()
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
  gub_link_sdl3_image_target(gubsy_engine)
elseif (PkgConfig_FOUND)
  pkg_check_modules(SDL3_IMAGE QUIET IMPORTED_TARGET sdl3-image)
  if (SDL3_IMAGE_FOUND)
    set(SDL3_IMAGE_FOUND_LOCAL ON)
    target_link_libraries(gubsy_engine PUBLIC PkgConfig::SDL3_IMAGE)
  endif()
endif()

if (NOT SDL3_IMAGE_FOUND_LOCAL AND GUB_FETCH_DEPS)
  if (DEFINED sdl3_BINARY_DIR AND EXISTS "${sdl3_BINARY_DIR}/SDL3Config.cmake")
    set(SDL3_DIR "${sdl3_BINARY_DIR}" CACHE PATH "" FORCE)
  endif()
  FetchContent_Declare(
    SDL3_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    # release-3.2.0
    GIT_TAG 4a762bdfb7b43dae7a8a818567847881e49bdab4
  )
  set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_TESTS OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_VENDORED ON CACHE BOOL "" FORCE)
  set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_JPG OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_TIF OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_QOI OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_BMP OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_GIF OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_LBM OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_PCX OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_PNM OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_SVG OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_TGA OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_XCF OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_XPM OFF CACHE BOOL "" FORCE)
  set(SDLIMAGE_XV OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(SDL3_image)
  set(SDL3_IMAGE_FOUND_LOCAL ON)
  gub_link_sdl3_image_target(gubsy_engine)
endif()

# SDL3_mixer for audio. The game can run without it; audio calls become no-ops.
set(SDL3_MIXER_FOUND_LOCAL OFF)
find_package(SDL3_mixer CONFIG QUIET)
if (SDL3_mixer_FOUND)
  set(SDL3_MIXER_FOUND_LOCAL ON)
  gub_link_sdl3_mixer_target(gubsy_engine)
elseif (PkgConfig_FOUND)
  pkg_check_modules(SDL3_MIXER QUIET IMPORTED_TARGET sdl3-mixer)
  if (SDL3_MIXER_FOUND)
    set(SDL3_MIXER_FOUND_LOCAL ON)
    target_link_libraries(gubsy_engine PUBLIC PkgConfig::SDL3_MIXER)
  endif()
endif()
if (NOT SDL3_MIXER_FOUND_LOCAL AND GUB_FETCH_DEPS)
  if (DEFINED sdl3_BINARY_DIR AND EXISTS "${sdl3_BINARY_DIR}/SDL3Config.cmake")
    set(SDL3_DIR "${sdl3_BINARY_DIR}" CACHE PATH "" FORCE)
  endif()
  FetchContent_Declare(
    SDL3_mixer
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
    # release-3.2.0
    GIT_TAG cedfeef30e93db35eee6b25759117da63f8e5a4f
  )
  set(SDLMIXER_VENDORED ON CACHE BOOL "" FORCE)
  set(SDLMIXER_WERROR OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_TESTS OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_FLAC OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_GME OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_MOD OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_MP3 OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_MIDI OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_OPUS OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_WAVPACK OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_VORBIS_STB ON CACHE BOOL "" FORCE)
  set(SDLMIXER_VORBIS_VORBISFILE OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_VORBIS_TREMOR OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(SDL3_mixer)
  set(SDL3_MIXER_FOUND_LOCAL ON)
  gub_link_sdl3_mixer_target(gubsy_engine)
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
    message(FATAL_ERROR "SDL3_image not found. Install libsdl3-image-dev, provide SDL3_image via CMake/pkg-config, or enable GUB_FETCH_DEPS.")
  else()
    message(WARNING "SDL3_image not found. Texture loading will not work.")
  endif()
endif()

find_package(Threads REQUIRED)
target_link_libraries(gubsy_engine PUBLIC Threads::Threads)
if (WIN32)
  target_link_libraries(gubsy_engine PUBLIC ws2_32)
endif()
if (GUB_BUILD_TOOLS)
  target_link_libraries(mod_server PRIVATE Threads::Threads)
  target_link_libraries(gubsy_roomd PRIVATE Threads::Threads)
endif()
