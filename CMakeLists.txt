cmake_minimum_required(VERSION 3.15)
project(rudp)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

string(APPEND CMAKE_CXX_FLAGS
    " -Werror -Wall -Wextra -Wpedantic"
    " -Wshadow"
    " -Wnon-virtual-dtor"
    " -Wold-style-cast"
    " -Wcast-align"
    " -Woverloaded-virtual"
    " -Wconversion"
    " -Wsign-conversion"
    " -Wmisleading-indentation"
    " -Wduplicated-cond"
    " -Wduplicated-branches"
    " -Wlogical-op"
    " -Wnull-dereference"
    " -Wuseless-cast"
    " -Wdouble-promotion"
    " -Wformat=2"
)

add_library(${PROJECT_NAME}
    src/rudp.cpp
    src/socket.cpp
    src/listener.cpp
    src/event_loop.cpp
    src/connection.cpp
    src/event_handler.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
