cmake_minimum_required(VERSION 3.20)

project(gubsy_consumer_smoke LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../.." gubsy)

add_executable(gubsy_consumer_smoke main.cpp)
target_link_libraries(gubsy_consumer_smoke PRIVATE gubsy::engine)
