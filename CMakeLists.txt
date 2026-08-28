cmake_minimum_required(VERSION 3.10)

project(Ping C)

# Build your shared utilities and crypto module as a single static library
add_library(shared_utils shared/utils.c shared/crypto.c)

# Build the Server executable and link the shared library
add_executable(server ServerSocket/main.c)
target_link_libraries(server shared_utils)

# Build the Client executable and link the shared library
add_executable(client ClientSocket/main.c)
target_link_libraries(client shared_utils)