# C Sockets: Multithreaded & Encrypted TCP Server

This repository contains a progressive implementation of a TCP client-server architecture written in C. The project evolves from a simple single-threaded echo server to a fully multithreaded messaging system featuring AES-256-GCM authenticated encryption and Base64 stream wrapping.

It demonstrates core networking and systems programming concepts, including POSIX sockets, `pthread` concurrency, memory management (linked lists, message queues), and OpenSSL cryptographic pipelines.

---

## Project Structure & Evolution

The project is divided into four iterative versions, showing the step-by-step development of the server architecture:

* **`/` (Root):** Contains the base unencrypted `client.c` used to connect to versions 1 through 3, as well as a master `Makefile`.
* **`v1_echo_server/`**: A foundational single-threaded TCP server (`serverEcho.c`) that accepts a connection, reads an incoming message, and echoes it back.
* **`v2_multithreaded/`**: Introduces POSIX threads (`serverMulti.c`) to handle concurrent client connections. A `pthread_mutex_t` lock safely synchronizes the active client counter.
* **`v3_command_parser/`**: 
  * `serverCom.c`: Adds a command execution loop (`/echo`, `/time`, `/random`, `/quit`).
  * `serverComList.c`: Introduces a doubly linked list (`client_data *first, *last`) to actively track and manage client sessions dynamically, adding the `/list` command.
* **`v4_encrypted_chat/`**: The final version implementing a mailbox messaging system (`/alias`, `/list`, `/send`, `/recv`) secured with **AES-256-GCM encryption** and **Base64 encoding** via OpenSSL. Includes a specialized encrypted client (`client.c`) and shared `crypto.h` header.

---

## Compilation & Usage

### Prerequisites
On Debian/Ubuntu systems, ensure build tools and OpenSSL development headers are installed:
```bash
sudo apt-get install build-essential libssl-dev
```

### Quick Build (Makefile)
You can build all servers and clients at once from the root directory:
```bash
make        # Builds client, v1, v2, v3, and v4
make clean  # Removes all compiled binaries
```

---

### Manual Compilation

#### 1. Building Versions 1–3 (Unencrypted)
Compile the base client and any of the earlier servers using `gcc`:

```bash
# Compile base client (Root Directory)
gcc client.c -o client

# Compile v1 (Echo Server)
cd v1_echo_server
gcc serverEcho.c -o server

# Compile v2 (Multithreaded Server)
cd v2_multithreaded
gcc serverMulti.c -o server -lpthread

# Compile v3 (Command Server with Client List)
cd v3_command_parser
gcc serverComList.c -o server -lpthread
```

Run the server on a specified port (default is 4242), then connect using the base client:
```bash
./server 4242
./client 127.0.0.1 4242
```

#### 2. Building Version 4 (AES Encrypted Chat)
The final version requires OpenSSL (`-lssl -lcrypto`) and uses the dedicated encrypted client inside the `v4_encrypted_chat/` directory:

```bash
cd v4_encrypted_chat

# Compile the Encrypted Server
gcc serverMess.c -o server_enc -lpthread -lssl -lcrypto

# Compile the Encrypted Client
gcc client.c -o client_enc -lssl -lcrypto
```

Run the encrypted server and connect with one or more encrypted clients:
```bash
# Terminal 1: Start encrypted server
./server_enc 4242

# Terminal 2: Connect first client
./client_enc 127.0.0.1 4242

# Terminal 3: Connect second client
./client_enc 127.0.0.1 4242
```

---

## Available Commands

Both slash prefix (`/command`) and plain format (`command`) are supported:

| Command | Version | Description | Example |
| :--- | :---: | :--- | :--- |
| `/echo <text>` | v3, v4 | Echoes the input text back from the server | `/echo hello world` |
| `/time` | v3, v4 | Returns the current server timestamp | `/time` |
| `/random [max]` | v3, v4 | Generates a pseudo-random integer up to `max` | `/random 100` |
| `/list` | v3 (List), v4 | Lists connected client IDs or aliases | `/list` |
| `/alias <name>` | v4 | Sets user alias (1–8 alphanumeric characters) | `/alias alice` |
| `/send <alias> <msg>` | v4 | Enqueues a message in recipient's mailbox | `/send bob Hey Bob!` |
| `/recv` | v4 | Retrieves the next queued message from your mailbox | `/recv` |
| `/quit` (or `q`) | All | Gracefully disconnects session | `/quit` |

---

## Technical Details & Security Considerations

### 1. Cryptographic Pipeline (Version 4)
* **Cipher**: AES-256 in Galois/Counter Mode (GCM), providing Authenticated Encryption with Associated Data (AEAD).
* **IV/Nonce**: A unique 12-byte random Initialization Vector (IV) is generated for each message via `RAND_bytes()`.
* **Integrity Tag**: A 16-byte authentication tag is generated on encryption and verified on decryption to detect tampering.
* **Transport Framing**: Raw binary ciphertexts `(IV + Ciphertext + Tag)` are Base64-encoded with a newline delimiter (`\n`) for reliable transmission across stream-oriented TCP sockets.

### 2. Things to Keep in Mind (Portfolio & Educational Context)
* **Pre-Shared Key (PSK)**: `crypto.h` uses a static 256-bit key for local demonstration. In a production environment, this should be replaced with a dynamic key exchange mechanism (such as Ephemeral Diffie-Hellman / `EVP_PKEY`) and TLS certificate verification.
* **Concurrency Model**: The servers use a **thread-per-client** model (`pthread_create`). This is clear and effective for dozens of concurrent clients, but for production systems handling tens of thousands of connections, an asynchronous event loop (`epoll`, `io_uring`, or `kqueue`) is preferred.
* **Buffer Capacities**: Plaintext messages in Version 4 are dimensioned for interactive chat lines up to ~1.5 KB to accommodate Base64 overhead in the 2048-byte buffer.
* **Mailbox Lifecycle**: Message boxes in Version 4 are kept in-memory and associated with active sessions; undelivered messages are safely freed when a client session terminates.
