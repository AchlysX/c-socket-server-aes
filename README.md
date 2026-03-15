# C Sockets: Multithreaded & Encrypted TCP Server

This repository contains a progressive implementation of a TCP client-server architecture written in C. The project evolves from a simple single-threaded echo server to a fully multithreaded messaging system featuring AES-256-GCM encryption.

It demonstrates core networking and systems programming concepts, including POSIX sockets, `pthread` concurrency, memory management (linked lists), and OpenSSL cryptographic pipelines.

## Project Structure & Evolution

The project is divided into four iterative versions, showing the step-by-step development of the server architecture.

* **`/` (Root):** Contains the base `client.c` used to connect to versions 1 through 3.
* **`v1_echo_server/`**: A foundational single-threaded TCP server that accepts a connection, reads a message, and echoes it back.
* **`v2_multithreaded/`**: Introduces `pthread` to handle concurrent client connections. A `pthread_mutex_t` lock is implemented to safely track the active client count.
* **`v3_command_parser/`**: 
  * `serverCom.c`: Adds a command execution loop (`/echo`, `/time`, `/random`, `/list`, `/quit`).
  * `serverComList.c`: Introduces a doubly linked list (`client_data *first, *last`) to actively track and manage client sessions dynamically.
* **`v4_encrypted_chat/`**: The final version implementing a mailbox messaging system (`/alias`, `/send`, `/recv`) secured with **AES-256-GCM encryption** and **Base64 encoding** via OpenSSL. Includes a specialized encrypted client and shared `crypto.h` header.

## Compilation & Usage

### Building Versions 1-3 (Unencrypted)
Compile the base client and any of the earlier servers using `gcc`.

```bash
# Compile the base client (Root Directory)
gcc client.c -o client

# Compile the multithreaded or command server (e.g., v3)
cd v3_command_parser
gcc serverComList.c -o server -lpthread
```

Run the server on a specified port, then connect with the client:
```bash
./server 4242
./client 127.0.0.1 4242
```

### Building Version 4 (AES Encrypted)
The final version requires the OpenSSL library (`libssl-dev` on Debian/Ubuntu). It uses the specialized client located inside the `v4` folder.

```bash
cd v4_encrypted_chat

# Compile the Encrypted Server
gcc serverMess.c -o server_enc -lpthread -lssl -lcrypto

# Compile the Encrypted Client
gcc client.c -o client_enc -lssl -lcrypto
```

## Security Disclaimer (Version 4)
The current implementation of the AES-256-GCM encryption in `v4_encrypted_chat` uses a hardcoded Pre-Shared Key (PSK) located in `crypto.h` to demonstrate the encryption and Base64 wrapping pipeline over TCP sockets. In a production environment, this should be replaced with a dynamic session key exchange (such as Diffie-Hellman `EVP_PKEY`) to prevent unauthorized decryption by actors with access to the source code.

## Key Concepts Demonstrated
- **POSIX Sockets:** `socket()`, `bind()`, `listen()`, `accept()`, `send()`, `recv()`.
- **Concurrency:** Handling multiple clients simultaneously using `pthread_create`.
- **Synchronization:** Preventing race conditions on shared resources (client count, linked lists) using `pthread_mutex_lock`.
- **Cryptography:** Utilizing the OpenSSL `EVP` API for Authenticated Encryption with Associated Data (AEAD) via AES-GCM, and safely transmitting raw ciphertext over text-based sockets using Base64 encoding.
