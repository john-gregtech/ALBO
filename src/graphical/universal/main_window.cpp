#include "graphical/universal/main_window.h"
#include "graphical/universal/login_dialog.h"
#include "graphical/universal/network_controller.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>

namespace prototype::graphical {

    MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
        setup_ui();
        setup_menus();
        setWindowTitle("ALBO Secure Messenger");
        resize(900, 600);

        // Initialize Controller
        controller = new prototype::network::NetworkController(this);

        // Connect Network Signals
        connect(controller, &prototype::network::NetworkController::messageReceived, this, [this](QString sender, QString text) {
            chat_display->append("<b>" + sender + ":</b> " + text);
        });

        connect(controller, &prototype::network::NetworkController::logMessage, this, [this](QString log) {
            chat_display->append("<i>SYSTEM: " + log + "</i>");
        });

        connect(controller, &prototype::network::NetworkController::authResult, this, [this](bool success, QString message) {
            if (success) {
                update_account_state(true);
                chat_display->append("<font color='green'><b>SYSTEM: " + message + "</b></font>");
            } else {
                QMessageBox::critical(this, "Authentication Failed", message);
                chat_display->append("<font color='red'><b>SYSTEM: Auth Failure - " + message + "</b></font>");
            }
        });

        connect(controller, &prototype::network::NetworkController::contactAdded, this, [this](QString uuid, QString username) {
            // Check if exists
            bool found = false;
            for(int i=0; i<contacts_root->childCount(); ++i) {
                if(contacts_root->child(i)->text(0) == username) { found=true; break; }
            }
            if (!found) {
                auto *item = new QTreeWidgetItem(contacts_root);
                item->setText(0, username);
                item->setToolTip(0, uuid);
            }
        });

        connect(send_button, &QPushButton::clicked, this, [this]() {
            QString text = input_box->text();
            if (text.isEmpty()) return;
            
            auto *item = address_book->currentItem();
            if (!item || item == contacts_root || item == groups_root) {
                QMessageBox::warning(this, "No Recipient", "Select a contact or group first.");
                return;
            }

            QString target = item->text(0);
            controller->sendMessage(target.toStdString(), text.toStdString());
            chat_display->append("<b>YOU -> " + target + ":</b> " + text);
            input_box->clear();
        });

        update_account_state(false); // Default state
        
        // Load history on selection
        connect(address_book, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int column) {
            if (item->parent() == contacts_root || item->parent() == groups_root) {
                QString name = item->text(0);
                chat_display->clear();
                chat_display->append("<i>SYSTEM: Chat history with " + name + "</i>");
                auto history = controller->fetchHistory(name.toStdString());
                std::string my_uuid = controller->getMyUUID();
                for (auto& h : history) {
                    QString label = (h.sender_uuid == my_uuid) ? "YOU" : name;
                    chat_display->append("<b>" + label + ":</b> " + QString::fromStdString(std::string(h.encrypted_payload.begin(), h.encrypted_payload.end())));
                }
            }
        });

        // Auto-connect to localhost for now
        controller->connectToServer("127.0.0.1", 5555);
    }

    MainWindow::~MainWindow() = default;

    void MainWindow::setup_ui() {
        auto *central_widget = new QWidget(this);
        auto *main_layout = new QHBoxLayout(central_widget);

        address_book = new QTreeWidget();
        address_book->setHeaderLabel("Address Book");
        address_book->setMaximumWidth(250);
        
        contacts_root = new QTreeWidgetItem(address_book);
        contacts_root->setText(0, "Contacts");
        contacts_root->setExpanded(true);

        groups_root = new QTreeWidgetItem(address_book);
        groups_root->setText(0, "Group Chats");
        groups_root->setExpanded(true);

        auto *sidebar_layout = new QVBoxLayout();
        auto *sidebar_tools = new QToolBar();
        auto *act_add_contact = new QAction("+ Contact", this);
        auto *act_new_group = new QAction("+ Group", this);
        auto *act_del_group = new QAction("- Delete", this);
        
        connect(act_add_contact, &QAction::triggered, this, &MainWindow::onAddContact);
        connect(act_new_group, &QAction::triggered, this, &MainWindow::onCreateGroup);
        connect(act_del_group, &QAction::triggered, this, &MainWindow::onDeleteGroup);

        sidebar_tools->addAction(act_add_contact);
        sidebar_tools->addAction(act_new_group);
        sidebar_tools->addAction(act_del_group);

        sidebar_layout->addWidget(sidebar_tools);
        sidebar_layout->addWidget(address_book);

        auto *chat_layout = new QVBoxLayout();
        chat_display = new QTextEdit();
        chat_display->setReadOnly(true);
        auto *input_layout = new QHBoxLayout();
        input_box = new QLineEdit();
        send_button = new QPushButton("Send");
        input_layout->addWidget(input_box);
        input_layout->addWidget(send_button);
        chat_layout->addWidget(chat_display);
        chat_layout->addLayout(input_layout);

        main_layout->addLayout(sidebar_layout);
        main_layout->addLayout(chat_layout);
        setCentralWidget(central_widget);
    }

    void MainWindow::setup_menus() {
        auto *conn_menu = menuBar()->addMenu("&Connections");
        auto *act_change_server = new QAction("&Switch Server...", this);
        connect(act_change_server, &QAction::triggered, this, &MainWindow::onChangeServer);
        conn_menu->addAction(act_change_server);

        auto *account_menu = menuBar()->addMenu("&Account");
        act_login = new QAction("&Login", this);
        act_status = new QAction("&Status", this);
        act_logout = new QAction("&Logout", this);
        connect(act_login, &QAction::triggered, this, &MainWindow::onLogin);
        connect(act_status, &QAction::triggered, this, &MainWindow::onStatus);
        connect(act_logout, &QAction::triggered, this, &MainWindow::onLogout);
        account_menu->addAction(act_login);
        account_menu->addAction(act_status);
        account_menu->addSeparator();
        account_menu->addAction(act_logout);

        auto *about_menu = menuBar()->addMenu("&About");
        about_menu->addAction("ALBO v1.0 Pre-Alpha");
    }

    void MainWindow::update_account_state(bool logged_in) {
        act_login->setEnabled(!logged_in);
        act_logout->setEnabled(logged_in);
        act_status->setEnabled(logged_in);
        input_box->setEnabled(logged_in);
        send_button->setEnabled(logged_in);
    }

    void MainWindow::onLogin() {
        LoginDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            QString user = dlg.getUsername();
            QString pass = dlg.getPassword();
            if (dlg.isRegistering()) {
                chat_display->append("<i>SYSTEM: Attempting registration for " + user + "...</i>");
                controller->performRegistration(user.toStdString(), pass.toStdString());
            } else {
                chat_display->append("<i>SYSTEM: Logging in as " + user + "...</i>");
                controller->performLogin(user.toStdString(), pass.toStdString());
            }
        }
    }

    void MainWindow::onLogout() {
        controller->performLogout();
        update_account_state(false);
        chat_display->append("<i>SYSTEM: Logged out.</i>");
    }

    void MainWindow::onStatus() {
        QMessageBox::information(this, "Profile", "Status: Online");
    }

    void MainWindow::onAddContact() {
        bool ok;
        QString name = QInputDialog::getText(this, "Add Contact", "Username:", QLineEdit::Normal, "", &ok);
        if (ok && !name.isEmpty()) {
            controller->addContact(name.toStdString());
        }
    }

    void MainWindow::onCreateGroup() {
        bool ok;
        QString gname = QInputDialog::getText(this, "Create Group", "Group Name:", QLineEdit::Normal, "", &ok);
        if (ok && !gname.isEmpty()) {
            auto *item = new QTreeWidgetItem(groups_root);
            item->setText(0, gname);
        }
    }

    void MainWindow::onDeleteGroup() {
        auto *item = address_book->currentItem();
        if (item && item->parent() == groups_root) { delete item; }
    }

    void MainWindow::onChangeServer() {
        bool ok;
        QString ip = QInputDialog::getText(this, "Change Server", "Server IP:", QLineEdit::Normal, "127.0.0.1", &ok);
        if (ok && !ip.isEmpty()) {
            int port = QInputDialog::getInt(this, "Change Server", "Server Port:", 5555, 1, 65535, 1, &ok);
            if (ok) {
                chat_display->append("<i>SYSTEM: Connecting to " + ip + ":" + QString::number(port) + "...</i>");
                controller->connectToServer(ip.toStdString(), port);
            }
        }
    }

}
