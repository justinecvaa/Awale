cmake_minimum_required(VERSION 3.10)
project(Awale C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Create directories in build folder
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/saves)

# Common source files
set(SERVER_SOURCES
    server/server.c
    server/command.c
    server/util.c
)

set(AWALE_SOURCES
    awale/awale.c
    awale/awale_save.c
)

# Create awale library
add_library(awale_lib ${AWALE_SOURCES})
target_include_directories(awale_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Create server executable
add_executable(server ${SERVER_SOURCES})
target_include_directories(server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(server 
    PRIVATE 
        awale_lib
        m
)

# Create client executable
add_executable(client client/client1.c)
target_include_directories(client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(client
    PRIVATE
        awale_lib
)

# Add platform-specific libraries if needed
if(WIN32)
    target_link_libraries(server PRIVATE wsock32 ws2_32)
    target_link_libraries(client PRIVATE wsock32 ws2_32)
endif()