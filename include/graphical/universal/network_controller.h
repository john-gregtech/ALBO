#pragma once
#include <QObject>
#include <QThread>
#include <memory>
#include <string>
#include "network/universal/secure_socket.h"
#include "network/universal/database.h"
#include "network/linux/client/crypto_service.h"
#include "network/linux/client/identity_manager.h"

namespace prototype::network {

    class NetworkController : public QObject {
        Q_OBJECT

    public:
        explicit NetworkController(QObject *parent = nullptr);
        ~NetworkController() override;

        // Async connection & auth
        void connectToServer(const std::string& ip, int port);
        void performLogin(const std::string& user, const std::string& pwd);
        void sendMessage(const std::string& target, const std::string& text);
        std::vector<prototype::database::MessageEntry> fetchHistory(const std::string& contact_name);
        void addContact(const std::string& name);
        void syncPreKeys();

    signals:
        void connectionEstablished();
        void authResult(bool success, QString message);
        void messageReceived(QString sender, QString text);
        void logMessage(QString log);
        void contactAdded(QString uuid, QString username);

    private:
        std::shared_ptr<SecureSocketManager> manager;
        std::shared_ptr<CryptoService> crypto;
        std::unique_ptr<IdentityManager> identity;
        prototype::database::DatabaseManager* local_db;
        
        std::string my_uuid;
        std::string my_username;

        SSL_CTX* ssl_ctx = nullptr;
        bool is_running = false;

        void run_receiver(); // Background loop
    };

}
