#include "graphical/universal/login_dialog.h"
#include <QFormLayout>
#include <QHBoxLayout>

namespace prototype::graphical {

    LoginDialog::LoginDialog(QWidget *parent) : QDialog(parent) {
        setup_ui();
        setWindowTitle("ALBO Identity");
    }

    void LoginDialog::setup_ui() {
        auto *layout = new QVBoxLayout(this);
        auto *form = new QFormLayout();

        user_edit = new QLineEdit();
        pass_edit = new QLineEdit();
        pass_edit->setEchoMode(QLineEdit::Password);

        form->addRow("Username:", user_edit);
        form->addRow("Password:", pass_edit);

        auto *btn_layout = new QHBoxLayout();
        auto *btn_login = new QPushButton("Login");
        auto *btn_reg = new QPushButton("Register New");

        connect(btn_login, &QPushButton::clicked, this, &LoginDialog::onLogin);
        connect(btn_reg, &QPushButton::clicked, this, &LoginDialog::onRegister);

        btn_layout->addWidget(btn_login);
        btn_layout->addWidget(btn_reg);

        layout->addLayout(form);
        layout->addLayout(btn_layout);
    }

    void LoginDialog::onLogin() {
        mode_register = false;
        accept();
    }

    void LoginDialog::onRegister() {
        mode_register = true;
        accept();
    }

}
