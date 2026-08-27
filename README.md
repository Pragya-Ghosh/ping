# Ping! A Secure Chat Application.

## Group Members
Pragya Ghosh (24051500) 

## How to Build
This project uses CMake to ensure cross-platform compatibility and clean source directories.
1. Create a build directory: `mkdir build && cd build`
2. Generate the Makefile: `cmake ..`
3. Compile the executables: `make`

## How to Run
* Server: `./server` (Defaults to port 2000)
* Client: `./client` (Connects to 127.0.0.1 on port 2000)

## Cipher Choice

**Known weakness:** 

## Design Notes
* **File Transfer:** Set to a strict maximum capacity of 1MB per transfer to prevent buffer overflows and ensure stable heap allocation.
* **Concurrency Model:** The server utilizes a `poll()` event loop. This single-threaded, non-blocking I/O approach was chosen because it allows the server to monitor up to 100 client file descriptors simultaneously without the overhead, context-switching costs, or race conditions associated with spawning separate POSIX threads for every connection.