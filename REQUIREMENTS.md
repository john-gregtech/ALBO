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
- [x] **Ed25519 Primitives:** Identity signature generation and verification logic.
- [x] **TLS Secured Transport:** All socket traffic wrapped in an OpenSSL TLS tunnel (Self-signed auto-generation).

### **[NOT DONE]**
- [ ] **Identity Challenge (Enforced):** Mandatory Ed25519 signature verification during handshake to prove UUID ownership.
- [ ] **Group E2EE:** Implement "Sender Key" model so group broadcasts are encrypted once and readable by all members.
- [ ] **Double Ratchet (Optional/Future):** Full session ratcheting for long-lived conversations.

---

## 🛰️ Networking & Wire Protocol

### **[DONE]**
- [x] **Binary "Envelope" Protocol:** Magic bytes (`ALBO`), fixed-size headers, and versioning.
- [x] **Opaque Payload Routing:** Server-side UUID resolution and routing based ONLY on header data.
- [x] **Session Locking:** Prevention of multiple simultaneous logins for the same account.
- [x] **Sender Identification:** Injected `sender_name` in the protocol header for human-friendly recognition.
- [x] **Post-Compile Configuration:** Server settings (`server.conf`) for Port and Database paths.
- [x] **Thread-Safe Dispatcher:** Client-side single-reader model to prevent socket race conditions.
- [x] **Modular Architecture:** Logic split into specialized Handlers (Auth, Routing, RateLimiting).
- [x] **IP Rate Limiting:** Throttling logic to prevent brute-force and DoS at the connection level.

### **[NOT DONE]**
- [ ] **Async I/O Core:** Transition from threaded model to `epoll` (Linux) / `IOCP` (Windows).
- [ ] **Heartbeats/Keep-alive:** Detecting ghost connections and cleaning up registry.

---

## 🖥️ User Interface (TUI & GUI)

### **[DONE]**
- [x] **Split-Pane TUI:** Dedicated log area and fixed input line using ANSI escape codes.
- [x] **Non-Blocking Console:** Real-time message updates without interrupting user typing.
- [x] **Session Context:** Quick messaging (remembers last recipient) and sent-message feedback.

### **[NOT DONE]**
- [ ] **Qt Framework Integration:** Implement the main application window using Qt.
- [ ] **Contact List Manager:** Visual representation of the address book.
- [ ] **Message View:** Threaded conversation view with status indicators.

---

## 📂 Database & Persistence

### **[DONE]**
- [x] **Standardized Storage:** Linux data path `~/.local/share/albo/`.
- [x] **Store-and-Forward:** Offline message queue with automatic push-on-login.
- [x] **Local Vault:** Client-side DB for private pre-keys and message history.
- [x] **Unique Usernames:** Case-insensitive, unique human-friendly names.
- [x] **Group Tables:** Database support for group metadata and membership.

### **[NOT DONE]**
- [ ] **Database Encryption:** At-rest encryption for the local SQLite database.
- [ ] **Rotating Logs:** Timestamped server audit files.
