# Team Ping! Secure Chat Application

```text
  ____  _             !
 |  _ \(_)_ __   __ _
 | |_) | | '_ \ / _` |
 |  __/| | | | | (_| |
 |_|   |_|_| |_|\__, |
                |___/
```

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
* **Hop-by-hop encryption:** This scheme decrypts and re-encrypts messages per client key at the server level. The server briefly holds the plaintext in memory to parse commands (like `SEND TO`) and re-encrypts the payload using the recipient's key before forwarding. True end-to-end encryption would require clients to establish a shared key directly with each other (e.g., via Diffie-Hellman key exchange) without the server ever possessing the ability to decrypt the payloads.

* **File transfer size cap:** 1 MB. This hard limit prevents buffer overflow faults and ensures stable heap allocation without requiring chunked or streaming transfer implementations.

* **Concurrency model:** `poll` event loop chosen because it monitors up to 100 client file descriptors simultaneously in a single thread. This avoids the heavy context-switching overhead of `fork()` processes and eliminates the race conditions and complex mutex locking required by `pthreads` when managing shared state arrays.

* **State Management:** Client usernames, symmetric keys, and socket FDs are managed in parallel arrays indexed identically to the `pollfd` array to guarantee $O(1)$ lookups during the `recv()` loop.

* **Client-Side Command Interception:** The client executable pre-parses local commands (`HELP`, `SENDFILE`) before transmitting to the socket, reducing unnecessary network round-trips.

## Known Limitations
* Because encryption is hop-by-hop, a compromised server would expose all plaintext communications.
* The 1MB file transfer limit means large files will be explicitly rejected by the server rather than chunked.
* The server relies on a centralized architecture; if the main server process terminates, all client connections are dropped simultaneously.

## UI & Terminal Aesthetics
To enhance readability and provide clear visual feedback in the command-line interface, the application utilizes standard ANSI escape codes for local terminal output.
* **Red (`\x1b[31m`):** Used for critical failures, bad input warnings, server disconnections, and error states.
* **Green (`\x1b[32m`):** Highlights successful operations, such as completing the registration handshake or successfully downloading a file payload.
* **Yellow (`\x1b[33m`):** Denotes local system alerts, local terminal prompts, and non-critical server events like client disconnects.
* **Cyan (`\x1b[36m`):** Used for standard system branding (ASCII startup banner), UI menus, and routing logs.
