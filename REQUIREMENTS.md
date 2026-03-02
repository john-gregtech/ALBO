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

### **[NOT DONE]**
- [ ] **Per-Message Handshake:** Re-establishing a new Diffie-Hellman exchange for *every single message* to ensure maximum Forward Secrecy.
- [ ] **Double Ratchet (Optional/Future):** Evaluate if a full Double Ratchet (Signal Protocol) is needed or if the per-message DH meets the security target.
- [ ] **End-to-End Encryption (E2EE):** Ensuring the self-hosted server only acts as a mailbox for encrypted BLOBs it cannot read.

---

## 🖥️ User Interface (Qt GUI)

### **[NOT DONE]**
- [ ] **Qt Framework Integration:** Implement the main application window using Qt.
- [ ] **Contact List Manager:** Visual representation of the address book.
- [ ] **Message View:** Threaded conversation view with status indicators.
- [ ] **Connection Manager:** Visual feedback for server status and handshake progress.
- [ ] **Settings Panel:** Client-side configurations.

---

## 🌐 Networking & Server Architecture

### **[DONE]**
- [x] **Basic Socket Boilerplate:** Initial Windows/Linux networking code.

### **[NOT DONE]**
- [ ] **Self-Hosted Server Core:** Standalone server binary that anyone can host.
- [ ] **Post-Compile Configuration:** Extensive `config.json` or `.env` support for:
    - Whitelists (Who can join).
    - Logging Verbosity (Privacy vs. Debugging).
    - Message Retention (Auto-delete or local storage).
- [ ] **Async Networking:** Transition to non-blocking I/O (epoll/IOCP) for handling multiple concurrent group chats.
- [ ] **Plug-and-Play Setup:** Ensure the server is easily deployable with minimal dependencies.

---

## 🕵️ Privacy & User Discovery

### **[NOT DONE]**
- [ ] **Private Discovery:** Servers will **not** reveal registered users to strangers.
- [ ] **Address Book Requests:** Clients can only request status/info for contacts they already possess the UUID for.
- [ ] **Group Chat Logic:** Encrypted group handling where the server manages distribution but not content.
- [ ] **Identity Verification:** Hex-based fingerprinting for out-of-band contact verification.

---

## 📂 Database & Persistence

### **[DONE]**
- [x] **SQLite3 Integration:** Thread-safe C++ wrapper (`DatabaseManager`).
- [x] **Message Schema:** Support for BLOBs (Ciphertext/IV) and metadata.
- [x] **Contact Schema:** UUID-based local storage.

### **[NOT DONE]**
- [ ] **Database Encryption:** At-rest encryption for the local SQLite database.
- [ ] **Automated Backups:** Secure export of the local address book.

---

## 🔄 Identity & Multi-Device Sync

### **[NOT DONE]**
- [ ] **Cross-Platform Account Sync:** Instant synchronization of address books and keys between Linux and Windows upon login.
- [ ] **Session Handover:** Seamlessly transitioning active handshakes from one device to another.
- [ ] **State Reconciliation:** Ensuring message history is consistent across all logged-in devices.

---

## 📣 Advanced Group Messaging Logic

### **[NOT DONE]**
- [ ] **Virtual User Routing:** Treating group chats as "Virtual Users" on the server. Messages sent to a Group UUID are automatically routed/broadcasted to all members by the server.
- [ ] **Standard Group Chats:** Full E2EE group messaging where all members see all responses.
- [ ] **Broadcast (Batch) Groups:** Specialized groups where a "Manager" sends a message to all members, but member replies are routed **privately** back to the Manager only.
- [ ] **Role Management:** "Group Manager" permissions for whitelisting members, removing users, and changing group visibility.
- [ ] **Customizable Group Behaviors:** Server-side configuration to toggle between Broadcast, Normal, and Restricted group modes.
