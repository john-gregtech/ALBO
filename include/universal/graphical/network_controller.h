#pragma once
#include <QObject>
#include <QThread>
#include <memory>
#include <string>
#include "universal/cryptowrapper/X25519.h"
#include "universal/network/secure_socket.h"
#include "universal/network/database.h"
#include "universal/network/crypto_service.h"
#include "universal/network/identity_manager.h"

namespace prototype::network {

    class NetworkController : public QObject {
        Q_OBJECT

    public:
        explicit NetworkController(QObject *parent = nullptr);
        ~NetworkController() override;

        // Async connection & auth
        void connectToServer(const std::string& ip, int port);
        void performLogin(const std::string& user, const std::string& pwd);
        void performRegistration(const std::string& user, const std::string& pwd);
        void performLogout();
        void sendMessage(const std::string& target, const std::string& text);
        std::vector<prototype::database::MessageEntry> fetchHistory(const std::string& contact_name);
        void addContact(const std::string& name);
        void syncPreKeys();

        std::string getMyUUID() const { return my_uuid; }
        
        bool isHistoryDirty(const std::string& contact_name);
        void markHistoryClean(const std::string& contact_name);

    signals:
        void connectionEstablished();
        void authResult(bool success, QString message);
        void messageReceived(QString sender, QString text);
        void logMessage(QString log);
        void contactAdded(QString uuid, QString username);
        void historyChanged(QString contact_name);

    private:
        std::shared_ptr<SecureSocketManager> manager;
        std::shared_ptr<CryptoService> crypto;
        std::unique_ptr<IdentityManager> identity;
        prototype::database::DatabaseManager* local_db;
        
        std::string my_uuid;
        std::string my_username;

        // Key Exchange State
        std::string temp_user;
        std::string temp_pass;
        bool is_registering = false;
        prototype::cryptowrapper::X25519KeyPair ephemeral_keys;
        std::vector<uint8_t> session_key;

        // History tracking
        std::mutex history_mtx;
        std::unordered_map<std::string, bool> dirty_map;

        SSL_CTX* ssl_ctx = nullptr;
        bool is_running = false;

        void run_receiver(); // Background loop
        void startKeyExchange(); // New helper
    };

}
