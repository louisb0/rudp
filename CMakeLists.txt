project(rudp)

set(CMAKE_CXX_STANDARD 17)
set(WARN_FLAGS "-Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic -Wcast-align -Wunused -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2 -Woverloaded-virtual -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast -Wold-style-cast")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARN_FLAGS}")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_library(rudp STATIC
    src/rudp.cpp
)

target_include_directories(rudp
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/internal
)

add_executable(client
    examples/client.cpp
)

add_executable(server
    examples/server.cpp
)

target_link_libraries(client PRIVATE rudp)
target_link_libraries(server PRIVATE rudp)
