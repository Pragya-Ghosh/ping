# Ping! A Secure Chat Application.
**Ping!** is a lightweight, concurrent command-line chat application written in standard C. Designed for secure, low-latency communication over TCP, it features a custom application-layer protocol that handles user authentication, targeted real-time messaging, room-wide broadcasting, and local file transfers. The system employs a centralized server architecture that multiplexes up to 100 simultaneous client connections, utilizing hop-by-hop symmetric encryption to ensure zero plaintext data traverses the network.

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

* **Concurrency Model (`poll` multiplexing):** The server utilizes a single-threaded `poll()` event loop rather than spawning POSIX threads (`pthreads`) or processes (`fork`) per connection. This "radar board" approach monitors file descriptors continuously, virtually eliminating context-switching overhead and preventing the race conditions associated with shared-memory chat states.

* **State Management & Data Locality:** Client states (usernames, symmetric keys, and socket FDs) are managed in parallel arrays indexed identically to the `pollfd` array. This stylistic choice guarantees $O(1)$ lookups for the active client context during the `recv()` loop, separating connection state from protocol logic.

* **Protocol Framing & Payload Handling:** To safely transmit binary `.txt` files containing arbitrary newlines, the `SENDFILE` protocol discards standard null-terminated string logic in favor of strict length-prefixed framing (e.g., `RECVFILE FROM user file.txt 1024\n<raw_bytes>`). 

* **Dynamic Buffer Allocation:** Instead of utilizing massive fixed-size stack arrays that risk stack overflow faults, the client and server dynamically allocate 1MB+ buffers on the heap (`malloc`) exclusively during I/O events that require handling heavy file payloads.

* **Client-Side Command Interception:** The client executable pre-parses local commands (like `HELP` and file-read operations for `SENDFILE`) before transmitting to the socket. This structural choice reduces unnecessary network round-trips and prevents server-side bottlenecking on invalid local commands.

* **Broadcast Routing (`SEND ALL`):** Added a custom broadcast feature that isolates the sender's socket and loops through the active `pollfd` array to dispatch room-wide messages, acting as a secondary routing layer separate from the targeted `SEND TO` command.