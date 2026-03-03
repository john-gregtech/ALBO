#pragma once
#include <QMainWindow>
#include <QPushButton>
#include <QListWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QMenuBar>
#include <QToolBar>
#include <QTreeWidget>

namespace prototype::network { class NetworkController; }

namespace prototype::graphical {

    class MainWindow : public QMainWindow {
        Q_OBJECT

    public:
        explicit MainWindow(QWidget *parent = nullptr);
        ~MainWindow() override;

    private slots:
        void onAddContact();
        void onCreateGroup();
        void onDeleteGroup();
        void onChangeServer();
        void onLogin();
        void onLogout();
        void onStatus();

    private:
        prototype::network::NetworkController *controller;
        
        // Account Menu Actions
        QAction *act_login;
        QAction *act_logout;
        QAction *act_status;

        // Sidebar (Unified Address Book)
        QTreeWidget *address_book;
        QTreeWidgetItem *contacts_root;
        QTreeWidgetItem *groups_root;
        
        // Chat Area
        QTextEdit *chat_display;
        QLineEdit *input_box;
        QPushButton *send_button;

        void setup_ui();
        void setup_menus();
        void update_account_state(bool logged_in);
    };

}
