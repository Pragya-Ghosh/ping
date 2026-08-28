cmake_minimum_required(VERSION 3.10)

project(Ping C)

# Build shared utilities as a single static library
add_library(shared_utils shared/utils.c shared/crypto.c)

# Build Server and link shared utilities
add_executable(server ServerSocket/main.c ServerSocket/server.c)
target_link_libraries(server shared_utils)

# Build Client and link shared utilities
add_executable(client ClientSocket/main.c ClientSocket/client.c)
target_link_libraries(client shared_utils)