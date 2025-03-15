cmake_minimum_required(VERSION 3.20)
project(rudp)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(COMMON_WARNINGS
    -Werror -Wall -Wextra -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Woverloaded-virtual
    -Wconversion
    -Wsign-conversion
    -Wmisleading-indentation
    -Wduplicated-cond
    -Wduplicated-branches
    -Wlogical-op
    -Wnull-dereference
    -Wuseless-cast
    -Wdouble-promotion
    -Wformat=2
)

# RUDP
add_library(${PROJECT_NAME}
    src/rudp.cpp
    src/socket.cpp
    src/listener.cpp
    src/event_loop.cpp
    src/connection.cpp
    src/state.cpp
    src/packet.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/internal
)
target_compile_options(${PROJECT_NAME} PRIVATE ${COMMON_WARNINGS})

# Examples
add_executable(rudp_server examples/server-client/server.cpp)
target_link_libraries(rudp_server PRIVATE ${PROJECT_NAME})
target_compile_options(rudp_server PRIVATE ${COMMON_WARNINGS})

add_executable(rudp_client examples/server-client/client.cpp)
target_link_libraries(rudp_client PRIVATE ${PROJECT_NAME})
target_compile_options(rudp_client PRIVATE ${COMMON_WARNINGS})

# Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

# Tests
include(GoogleTest)
enable_testing()
add_executable(tests
    test/unit/bind.cpp
    test/unit/listen.cpp
    test/unit/accept.cpp
    test/unit/connect.cpp
    test/unit/send.cpp
    test/unit/recv.cpp
    test/integration/send_recv.cpp
)
target_link_libraries(tests PRIVATE ${PROJECT_NAME} GTest::gtest_main)
target_compile_options(tests PRIVATE ${COMMON_WARNINGS})
gtest_discover_tests(tests)
