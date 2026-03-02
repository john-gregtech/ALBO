# ALBO: Professional Encrypted Messenger Roadmap

**Vision:** A high-security, self-hosted, plug-and-play messaging ecosystem. Prioritizes cryptographic integrity and user privacy over raw performance, while maintaining a responsive user experience.

---

## 🔐 Security & Cryptography (The Core)

### **[DONE]**
- [x] **AES-256-GCM:** Authenticated Encryption with 16-byte Integrity Tags.
- [x] **Argon2id:** Gold-standard password hashing and key derivation.
- [x] **X25519:** Modern Elliptic Curve Diffie-Hellman (ECDH) primitives.
- [x] **Secure Memory Management:** Platform-specific RAM wiping (`SecureZeroMemory` / `explicit_bzero`).
- [x] **SHA-256:** Data integrity and hashing.
- [x] **Per-Message Handshake:** Re-establishing a new Diffie-Hellman exchange for *every single message* using ephemeral keys.
- [x] **End-to-End Encryption (E2EE):** Server only routes opaque binary blobs; decryption happens strictly on the client.
- [x] **One-Time Pre-Keys (Signal-style):** Asynchronous encryption for offline users using pre-uploaded server-side public keys.

### **[NOT DONE]**
- [ ] **Double Ratchet (Optional/Future):** Full session ratcheting for long-lived conversations.
- [ ] **Identity Verification:** Hex-based fingerprinting for out-of-band contact verification.

---

## 🛰️ Networking & Wire Protocol

### **[DONE]**
- [x] **Binary "Envelope" Protocol:** Magic bytes (`ALBO`), fixed-size headers, and versioning.
- [x] **Opaque Payload Routing:** Server-side UUID resolution and routing based ONLY on header data.
- [x] **Session Locking:** Prevention of multiple simultaneous logins for the same account.
- [x] **Sender Identification:** Injected `sender_name` in the protocol header for human-friendly recognition.
- [x] **Post-Compile Configuration:** Server settings (`server.conf`) for Port and Database paths.
- [x] **Thread-Safe Dispatcher:** Client-side single-reader model to prevent socket race conditions.

### **[NOT DONE]**
- [ ] **Identity Challenge (Enforced):** Full Ed25519 signature verification during handshake (Primitives ready, enforcement pending).
- [ ] **Async I/O Core:** Transition from threaded model to `epoll` (Linux) / `IOCP` (Windows).
- [ ] **Heartbeats/Keep-alive:** Detecting ghost connections and cleaning up registry.

---

## 🪟 Windows vs. Linux Network Disparities

### **[CURRENT ARCHITECTURE]**
- **Linux:** Uses POSIX sockets (`<sys/socket.h>`, `<arpa/inet.h>`).
- **Windows:** Requires Winsock2 (`<winsock2.h>`, `<ws2tcpip.h>`) and `WSAStartup`/`WSACleanup` calls.

### **[ROADMAP FOR UNIFICATION]**
- [ ] **Abstraction Layer:** Create a `UniversalSocket` class to hide `recv` vs `recv` and `send` vs `send` differences.
- [ ] **Byte Order:** Ensure `uint64_t` UUID parts are handled with `ntohll`/`htonll` to prevent Big-Endian/Little-Endian corruption between OSs.
- [ ] **Secure Memory:** Use the existing `secure_erase` abstraction to handle `explicit_bzero` vs `SecureZeroMemory`.
- [ ] **Pathing:** Support `%APPDATA%` on Windows vs `~/.local/share` on Linux for database storage.

---

## 📣 Advanced Group Messaging Logic

### **[NOT DONE]**
- [ ] **Virtual User Routing:** Group chats as "Virtual Users" on the server.
- [ ] **Standard Group Chats:** Full E2EE group messaging where all members see all responses.
- [ ] **Broadcast (Batch) Groups:** Responses routed privately back to the Manager only.

---

## 📂 Database & Persistence

### **[DONE]**
- [x] **Standardized Storage:** Linux data path `~/.local/share/albo/`.
- [x] **Store-and-Forward:** Offline message queue with automatic push-on-login.
- [x] **Local Vault:** Client-side DB for private pre-keys and message history.
- [x] **Unique Usernames:** Case-insensitive, unique human-friendly names.

### **[NOT DONE]**
- [ ] **Database Encryption:** At-rest encryption for the local SQLite database.
- [ ] **Rotating Logs:** Timestamped server audit files.
