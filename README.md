# Ping! A Secure Chat Application.

```text
  ____  _             !
 |  _ \(_)_ __   __ _
 | |_) | | '_ \ / _` |
 |  __/| | | | | (_| |
 |_|   |_|_| |_|\__, |
                |___/
```

**Ping!** is a lightweight, concurrent command-line chat application written in C. Designed for secure, low-latency communication over TCP, it features a custom application-layer protocol that handles user authentication, targeted real-time messaging, room-wide broadcasting, and local file transfers. The system employs a centralized server architecture that multiplexes up to 100 simultaneous client connections, utilizing hop-by-hop symmetric encryption to ensure zero plaintext data traverses the network.

## Developer
Pragya Ghosh\
24051500\
CSE-31

## How to Build
This project uses CMake to ensure cross-platform compatibility and clean source directories.
1. Create a build directory: `mkdir build && cd build`
2. Generate the Makefile: `cmake ..`
3. Compile the executables: `make`

## How to Run
* Server: `./server <port>` 
* Client: `./client <server_ip> <port>` 

## Cipher Choice
I chose the Repeating-Key XOR cipher over the Vigenère and Mini Substitution-Permutation options because it operates directly at the bitwise level, making it incredibly lightweight and efficient in C. Unlike Vigenère, which is traditionally restricted to alphabetical letters, XOR seamlessly works on arbitrary printable text, including the specific newline (\n) delimiter characters used in our protocol framing. Also, it avoids the complex block matrices of a Substitution-Permutation network, allowing us to encrypt large 1MB file buffers entirely in-place without allocating extra heap memory.

**Known weakness:**
The Repeating-Key XOR cipher is highly vulnerable to known-plaintext attacks and frequency analysis. If an attacker knows or guesses a predictable part of the message (such as our protocol's REGISTER  or SENDFILE TO  headers), they can simply XOR the intercepted ciphertext against that known plaintext to instantly reveal the secret key. Additionally, if the message is significantly longer than the key, an attacker can use index of coincidence or Hamming distance calculations to deduce the key's length and crack the cipher. (The server actually exploits this exact known-plaintext vulnerability to logically deduce a new client's key during the encrypted registration handshake).

## Architecture Flow (Hop-by-Hop Encryption)
```text
+-------------------+                                      +-------------------+
| Client A          |                                      | Client B          |
| Key: secret_A     |                                      | Key: secret_B     |
+--------+----------+                                      +----------+--------+
         |                                                            ^
         | 1. Encrypts payload with                                   | 5. Decrypts payload with
         |    Client A's key.                                         |    Client B's key.
         |                                                            |
         v                                                            |
.-------------------.                                      .-------------------.
|   [Encrypted]     |                                      |   [Encrypted]     |
|  Network Traffic  |                                      |  Network Traffic  |
'--------+----------'                                      '----------+--------'
         |                                                            ^
         | 2. Server receives ciphertext.                             | 4. Server re-encrypts
         v                                                            |    payload using Client B's key.
+-------------------------------------------------------------------------+
|                               CENTRAL SERVER                            |
|                                                                         |
|  3. Hop-by-Hop Routing:                                                 |
|     - Decrypts incoming payload using Client A's key.                   |
|     - Parses command (e.g., "SEND TO Client_B").                        |
|     - Looks up target client (Client B) and retrieves their key.        |
+-------------------------------------------------------------------------+
```

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
* **Cyan (`\x1b[36m`):** Used for UI menus and routing logs.
