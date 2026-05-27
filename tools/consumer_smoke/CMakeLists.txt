cmake_minimum_required(VERSION 3.20)

project(gubsy_consumer_smoke LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../.." gubsy)

if (NOT TARGET gubsy::engine)
  message(FATAL_ERROR "consumer mode did not expose gubsy::engine")
endif()

foreach(disallowed_target IN ITEMS gubsy gubsy_roomd mod_server)
  if (TARGET ${disallowed_target})
    message(FATAL_ERROR "consumer mode unexpectedly built ${disallowed_target}")
  endif()
endforeach()

add_executable(gubsy_consumer_smoke main.cpp)
target_include_directories(gubsy_consumer_smoke PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../include")

foreach(sdl_target IN ITEMS SDL3::SDL3 SDL3::SDL3-shared SDL3::SDL3-static)
  if (TARGET ${sdl_target})
    get_target_property(sdl_include_dirs ${sdl_target} INTERFACE_INCLUDE_DIRECTORIES)
    if (sdl_include_dirs)
      target_include_directories(gubsy_consumer_smoke PRIVATE ${sdl_include_dirs})
    endif()
  endif()
endforeach()

foreach(sdl_include_dir IN ITEMS
    "${CMAKE_BINARY_DIR}/_deps/sdl3-build/include-revision"
    "${CMAKE_BINARY_DIR}/_deps/sdl3-src/include"
    "${CMAKE_BINARY_DIR}/_deps/sdl3_image-src/include"
    "${CMAKE_BINARY_DIR}/_deps/sdl3_mixer-src/include"
    "${CMAKE_BINARY_DIR}/_deps/sdl3_ttf-src/include")
  if (EXISTS "${sdl_include_dir}")
    target_include_directories(gubsy_consumer_smoke PRIVATE "${sdl_include_dir}")
  endif()
endforeach()
